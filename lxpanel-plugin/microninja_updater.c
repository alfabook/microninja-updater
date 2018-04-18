/* Sources based on:
 *
 * kano_updater.c
 *
 * Copyright (C) 2014 Kano Computing Ltd.
 * License: http://www.gnu.org/licenses/gpl-2.0.txt GNU General Public License v2
 *
 */

/*
 * Copyright (C) 2016 Alfabook srl
 * License: http://www.gnu.org/licenses/gpl-2.0.txt GNU General Public License v2
 *
*/

#include <gtk/gtk.h>
#include <gdk/gdk.h>

//#include <glib/gi18n-lib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gio/gio.h>

#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

#include <lxpanel/plugin.h>

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <pwd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libintl.h>
#include <locale.h>
#include <string.h>

#include <kdesk-hourglass.h>

#define _(STRING) gettext(STRING)

/* Icon file paths */
#define NO_UPDATES_ICON_FILE \
    "/usr/share/microninja-updater/images/widget-no-updates.png"
#define CHECKING_FOR_NORMAL_UPDATES_ICON_FILE \
    "/usr/share/microninja-updater/images/widget-checking-for-normal-updates.png"
#define CHECKING_FOR_URGENT_UPDATES_ICON_FILE \
    "/usr/share/microninja-updater/images/widget-checking-for-urgent-updates.png"
#define UPDATES_AVAILABLE_ICON_FILE \
    "/usr/share/microninja-updater/images/widget-updates-available.png"
#define DOWNLOADING_UPDATES_ICON_FILE \
    "/usr/share/microninja-updater/images/widget-downloading-updates.png"
#define UPDATES_DOWNLOADED_ICON_FILE \
    "/usr/share/microninja-updater/images/widget-updates-downloaded.png"

#define CHECK_FOR_UPDATES_CMD "sudo /usr/bin/microninja-updater check"
#define DOWNLOAD_CMD "sudo /usr/bin/microninja-updater download"
#define SOUND_CMD "/usr/bin/aplay /usr/share/microninja-media/sounds/microninja-updater-start.wav"
#define SOUND_FINISHED_CMD "/usr/bin/aplay /usr/share/microninja-media/sounds/microninja-updater-finished.wav"

/* Frequency with which we check for updates depending on priorities */
#define POLL_INTERVAL (30 * 60 * 1000) /* 30 minutes in microseconds */

#include "common.h"

typedef struct {
    GFile *status_file;
    GFileMonitor *monitor;

    UpdatingStatus status;

    GtkWidget *icon;
    guint timer;
    LXPanel *panel;
} microninja_updater_plugin_t;

static gboolean show_menu(GtkWidget *, GdkEventButton *,
                          microninja_updater_plugin_t *);
static void selection_done(GtkWidget *);
static gboolean update_status(microninja_updater_plugin_t *);
static gboolean check_for_updates(microninja_updater_plugin_t *);

static void launch_cmd(const char *cmd);
static void plugin_destructor(gpointer user_data);
static void menu_pos(GtkMenu *menu, gint *x, gint *y, gboolean *push_in,
                     GtkWidget *widget);

void file_monitor_cb(GFileMonitor *monitor, GFile *first, GFile *second,
                     GFileMonitorEvent event, gpointer user_data);

static GtkWidget *plugin_constructor(LXPanel *panel, config_setting_t *settings)
{
    /*localization settings*/
    setlocale (LC_ALL, "");
    bindtextdomain ("microninja_updater", "/usr/share/locale/microninja/microninja-updater/");
    textdomain ("microninja_updater");

    /* allocate our private structure instance */
    microninja_updater_plugin_t *plugin_data = g_new0(microninja_updater_plugin_t, 1);

    plugin_data->panel = panel;
    plugin_data->status = CHECKING_FOR_URGENT_UPDATES;

    GtkWidget *icon = gtk_image_new_from_file(CHECKING_FOR_URGENT_UPDATES_ICON_FILE);
    plugin_data->icon = icon;

    /* need to create a widget to show */
    GtkWidget *pwid = gtk_event_box_new();
    lxpanel_plugin_set_data(pwid, plugin_data, plugin_destructor);

    /* set border width */
    gtk_container_set_border_width(GTK_CONTAINER(pwid), 0);

    /* add the label to the container */
    gtk_container_add(GTK_CONTAINER(pwid), GTK_WIDGET(icon));

    /* our widget doesn't have a window... */
    gtk_widget_set_has_window(pwid, FALSE);

    gtk_signal_connect(GTK_OBJECT(pwid), "button-press-event",
                       GTK_SIGNAL_FUNC(show_menu), plugin_data);

    /* Set a tooltip to the icon to show when the mouse sits over the it */
    GtkTooltips *tooltips;
    tooltips = gtk_tooltips_new();
    gtk_tooltips_set_tip(tooltips, GTK_WIDGET(icon), _("Keep Microninja updated"), NULL);

    gtk_widget_set_sensitive(icon, TRUE);

    plugin_data->timer = g_timeout_add(POLL_INTERVAL,
                                       (GSourceFunc) check_for_updates,
                                       (gpointer) plugin_data);

    /* show our widget */
    gtk_widget_show_all(pwid);

    /* launching check for udates */
    launch_cmd(CHECK_FOR_UPDATES_CMD);

    /* Start watching the file for status changes. */
    plugin_data->status_file = g_file_new_for_path(UPDATE_STATUS_FILE);
    g_assert(plugin_data->status_file != NULL);

    plugin_data->monitor = g_file_monitor(plugin_data->status_file,
                                          G_FILE_MONITOR_NONE, NULL, NULL);
    g_assert(plugin_data->monitor != NULL);
    g_signal_connect(plugin_data->monitor, "changed",
                     G_CALLBACK(file_monitor_cb), (gpointer) plugin_data);

    return pwid;
}

static void plugin_destructor(gpointer user_data)
{
    microninja_updater_plugin_t *plugin_data = (microninja_updater_plugin_t *)user_data;

    g_object_unref(plugin_data->monitor);

    /* Disconnect the timer. */
    g_source_remove(plugin_data->timer);

    g_free(plugin_data);
}

void file_monitor_cb(GFileMonitor *monitor, GFile *first, GFile *second,
                     GFileMonitorEvent event, gpointer user_data)
{
    microninja_updater_plugin_t *plugin_data = (microninja_updater_plugin_t *)user_data;
    update_status(plugin_data);

    if(plugin_data->status == SYSTEM_UPDATED) {
        launch_cmd(SOUND_FINISHED_CMD);
    }
}

static void launch_cmd(const char *cmd)
{
    GAppInfo *appinfo = NULL;
    gboolean ret = FALSE;

    appinfo = g_app_info_create_from_commandline(cmd, NULL,
              G_APP_INFO_CREATE_NONE, NULL);

    if (appinfo == NULL) {
        perror("Command launch failed.");
        return;
    }

    ret = g_app_info_launch(appinfo, NULL, NULL, NULL);
    if (!ret) {
        perror("Command launch failed.");
    }
}

static gboolean check_for_updates(microninja_updater_plugin_t *plugin_data)
{
    plugin_data->status = CHECKING_FOR_URGENT_UPDATES;
    gtk_image_set_from_file(GTK_IMAGE(plugin_data->icon), CHECKING_FOR_URGENT_UPDATES_ICON_FILE);

    launch_cmd(CHECK_FOR_UPDATES_CMD);

    return TRUE;
}

static gboolean read_status(microninja_updater_plugin_t *plugin_data)
{
    FILE *ptr;
    ptr = fopen(UPDATE_STATUS_FILE, "rb");

    plugin_data->status = NO_UPDATES_AVAILABLE;

    if(ptr) {
        fread(&plugin_data->status, sizeof(UpdatingStatus), 1, ptr);
        fclose(ptr);

        if(plugin_data->status == CHECKING_FOR_URGENT_UPDATES) {
            gtk_image_set_from_file(GTK_IMAGE(plugin_data->icon), CHECKING_FOR_URGENT_UPDATES_ICON_FILE);
        } else if(plugin_data->status == CHECKING_FOR_NORMAL_UPDATES) {
            gtk_image_set_from_file(GTK_IMAGE(plugin_data->icon), CHECKING_FOR_NORMAL_UPDATES_ICON_FILE);
        } else if(plugin_data->status == NO_UPDATES_AVAILABLE) {
            gtk_image_set_from_file(GTK_IMAGE(plugin_data->icon), NO_UPDATES_ICON_FILE);
        } else if(plugin_data->status == UPDATES_AVAILABLE) {
            gtk_image_set_from_file(GTK_IMAGE(plugin_data->icon), UPDATES_AVAILABLE_ICON_FILE);
        } else if(plugin_data->status == DOWNLOADING_UPDATES) {
            gtk_image_set_from_file(GTK_IMAGE(plugin_data->icon), DOWNLOADING_UPDATES_ICON_FILE);
        } else if(plugin_data->status == SYSTEM_UPDATED) {
            gtk_image_set_from_file(GTK_IMAGE(plugin_data->icon), UPDATES_DOWNLOADED_ICON_FILE);
        }

        return TRUE;
    }
    return FALSE;
}

static gboolean update_status(microninja_updater_plugin_t *plugin_data)
{
    read_status(plugin_data);
    return TRUE;
}

void download_clicked(GtkWidget *widget, microninja_updater_plugin_t *plugin_data)
{
    plugin_data->status = DOWNLOADING_UPDATES;
    gtk_image_set_from_file(GTK_IMAGE(plugin_data->icon), DOWNLOADING_UPDATES_ICON_FILE);

    /* Launch updater */
    launch_cmd(DOWNLOAD_CMD);

    /* Play sound */
    launch_cmd(SOUND_CMD);
}

void check_for_updates_clicked(GtkWidget *widget, microninja_updater_plugin_t *plugin_data)
{
    plugin_data->status = CHECKING_FOR_URGENT_UPDATES;
    gtk_image_set_from_file(GTK_IMAGE(plugin_data->icon), CHECKING_FOR_URGENT_UPDATES_ICON_FILE);

    launch_cmd(CHECK_FOR_UPDATES_CMD);
}

static void menu_add_item(GtkWidget *menu, gchar *label, gpointer activate_cb,
                          gpointer user_data, gboolean active)
{
    GtkWidget *item;
    item = gtk_menu_item_new_with_label(label);

    if (activate_cb)
        g_signal_connect(item, "activate",
                         G_CALLBACK(activate_cb), user_data);

    if (!active)
        gtk_widget_set_sensitive(item, FALSE);

    gtk_menu_append(GTK_MENU(menu), item);
    gtk_widget_show(item);
}

static gboolean show_menu(GtkWidget *widget, GdkEventButton *event,
                          microninja_updater_plugin_t *plugin_data)
{
    setlocale (LC_ALL, "");
    bindtextdomain ("microninja_updater", "/usr/share/locale/microninja/microninja-updater/");
    textdomain ("microninja_updater");

    GtkWidget *menu = gtk_menu_new();

    if (event->button != 1)
        return FALSE;

    /* Create the menu items */
    menu_add_item(menu, _("Microninja Updates"), NULL, NULL, FALSE);

    if(plugin_data->status == CHECKING_FOR_NORMAL_UPDATES) {
        menu_add_item(menu, _("Checking for updates ..."),
                      NULL, NULL, FALSE);
    }
    if(plugin_data->status == CHECKING_FOR_URGENT_UPDATES) {
        menu_add_item(menu, _("Checking for urgent updates ..."),
                      NULL, NULL, FALSE);
    }
    else if(plugin_data->status == NO_UPDATES_AVAILABLE) {
        menu_add_item(menu, _("No updates available"),
                      NULL, NULL, FALSE);
        menu_add_item(menu, _("Check again"),
                      G_CALLBACK(check_for_updates_clicked),
                      plugin_data, TRUE);
    } else if(plugin_data->status == UPDATES_AVAILABLE) {
        menu_add_item(menu, _("Install updates"),
                      G_CALLBACK(download_clicked), plugin_data, TRUE);
    } else if(plugin_data->status == DOWNLOADING_UPDATES) {
        menu_add_item(menu, _("Installing updates ..."),
                      NULL, NULL, FALSE);
    } else if(plugin_data->status == SYSTEM_UPDATED) {
        menu_add_item(menu, _("System updated"),
                      NULL, NULL, FALSE);
        menu_add_item(menu, _("Check again"),
                      G_CALLBACK(check_for_updates_clicked),
                      plugin_data, TRUE);
    }

    g_signal_connect(menu, "selection-done",
                     G_CALLBACK(selection_done), NULL);

    /* Show the menu. */
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
                   (GtkMenuPositionFunc) menu_pos, widget,
                   event->button, event->time);

    return TRUE;
}

static void selection_done(GtkWidget *menu)
{
    gtk_widget_destroy(menu);
}

static void menu_pos(GtkMenu *menu, gint *x, gint *y, gboolean *push_in,
                     GtkWidget *widget)
{
    int ox, oy, w, h;
    microninja_updater_plugin_t *plugin_data = lxpanel_plugin_get_data(widget);
    GtkAllocation allocation;

    gtk_widget_get_allocation(GTK_WIDGET(widget), &allocation);

    gdk_window_get_origin(gtk_widget_get_window(widget), &ox, &oy);

    /* FIXME The X origin is being truncated for some reason, reset
       it from the allocaation. */
    ox = allocation.x;

#if GTK_CHECK_VERSION(2,20,0)
    GtkRequisition requisition;
    gtk_widget_get_requisition(GTK_WIDGET(menu), &requisition);
    w = requisition.width;
    h = requisition.height;

#else
    w = GTK_WIDGET(menu)->requisition.width;
    h = GTK_WIDGET(menu)->requisition.height;
#endif
    if (panel_get_orientation(plugin_data->panel) == GTK_ORIENTATION_HORIZONTAL) {
        *x = ox;
        if (*x + w > gdk_screen_width())
            *x = ox + allocation.width - w;
        *y = oy - h;
        if (*y < 0)
            *y = oy + allocation.height;
    } else {
        *x = ox + allocation.width;
        if (*x > gdk_screen_width())
            *x = ox - w;
        *y = oy;
        if (*y + h >  gdk_screen_height())
            *y = oy + allocation.height - h;
    }

    /* Debugging prints */
    /*printf("widget: x,y=%d,%d  w,h=%d,%d\n", ox, oy, allocation.width, allocation.height );
    printf("w-h %d %d\n", w, h); */

    *push_in = TRUE;

    return;
}

FM_DEFINE_MODULE(lxpanel_gtk, microninja_updater)

/* Plugin descriptor. */
LXPanelPluginInit fm_module_init_lxpanel_gtk = {
    .name = "Microninja Updater",
    .description = "Keep your Microninja OS updated.",
    .new_instance = plugin_constructor,
    .one_per_system = FALSE,
    .expand_available = FALSE
};
