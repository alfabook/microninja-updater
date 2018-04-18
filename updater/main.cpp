/*
 * Very simple parser for some apt-get commands
 *
 * Copyright (C) 2016 Alfabook srl
 * License: http://www.gnu.org/licenses/gpl-2.0.txt GNU General Public License v2
 *
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <dirent.h>

#include "common.h"

//apt-get update: sudo apt-get update -o Dir::Etc::sourcelist="sources.list.d/microninja.list" -o Dir::Etc::sourceparts="-" -o APT::Get::List-Cleanup="0"
//upgrade: sudo apt-get upgrade -o Dir::Etc::sourcelist="sources.list.d/microninja.list" -o Dir::Etc::sourceparts="-" -o APT::Get::List-Cleanup="0"

//sudo apt-get update -o Dir::Etc::sourcelist="sources.list.d/microninja-urgent.list" -o Dir::Etc::sourceparts="-" -o APT::Get::List-Cleanup=\"0\" && apt-get install microninja-script-update -y -q --force-yes && apt-get dist-upgrade -o Dir::Etc::sourcelist="sources.list.d/microninja-urgent.list" -o Dir::Etc::sourceparts="-" -o APT::Get::List-Cleanup="0" -y -q --force-yes

void write_status(UpdatingStatus status)
{
    FILE *write_ptr;
    write_ptr = fopen(UPDATE_STATUS_FILE, "wb");
    if(write_ptr) {
        fwrite(&status, sizeof(UpdatingStatus), 1, write_ptr);
        fclose(write_ptr);
    }
}

void execute_updater_script()
{
    DIR* dir = opendir("/usr/share/microninja-updater/update");
    if (dir)
    {
        chdir("/usr/share/microninja-updater/update");
        FILE *stream;
        char buffer[2048];
        const char * command = "unzip -o update.zip && chmod +x update && ./update";
        stream = popen(command, "r");
        if(stream == NULL)
        {
            perror("popen");
            return;
        }
        while(fgets(buffer, 2048, stream))
        {
            //do nothing, in this case
        }
        pclose(stream);
        closedir(dir);
    }
}

void execute_urgent_updates()
{
    FILE *stream;
    char buffer[4096];
    const char * command = "dpkg --configure -a;apt-get -f install -y -q --force-yes;"
			   "apt-get update -o Dir::Etc::sourcelist"
                           "=\"sources.list.d/microninja-urgent.list\" -o Dir::Etc::sourceparts=\"-\" "
                           "-o APT::Get::List-Cleanup=\"0\";apt-get install microninja-script-update "
                           "-y -q --force-yes;apt-get dist-upgrade "
                           "-o Dir::Etc::sourcelist=\"sources.list.d/microninja-urgent.list\" "
                           "-o Dir::Etc::sourceparts=\"-\" -o APT::Get::List-Cleanup=\"0\" "
                           "-y -q --force-yes;apt-get -f install -y -q --force-yes";
    stream = popen(command, "r");
    if(stream == NULL)
    {
        perror("popen");
        return;
    }
    while(fgets(buffer, 4096, stream))
    {
        //do nothing, in this case 
    }
    pclose(stream);
    execute_updater_script();
}

void execute_apt_update()
{
    FILE *stream;
    char buffer[2048];
    const char * command = "apt-get update -o Dir::Etc::sourcelist"
                           "=\"sources.list.d/microninja.list\" -o Dir::Etc::sourceparts=\"-\" "
                           "-o APT::Get::List-Cleanup=\"0\"";
    stream = popen(command, "r");
    if(stream == NULL)
    {
        perror("popen");
        return;
    }
    while(fgets(buffer, 2048, stream))
    {
        //do nothing, in this case
    }
    pclose(stream);
}

bool check_upgrades_available()
{
    FILE *stream;
    char buffer[2048];
    const char * command = "LANGUAGE=en_GB apt-get --just-print dist-upgrade "
                           "-o Dir::Etc::sourcelist=\"sources.list.d/microninja.list\" "
                           "-o Dir::Etc::sourceparts=\"-\" -o APT::Get::List-Cleanup=\"0\"";
    stream = popen(command, "r");
    if(stream == NULL)
    {
        perror("popen");
        return false;
    }

    bool needs_upgrade = false;
    while(fgets(buffer, 2048, stream))
    {
        if(strstr(buffer, "The following packages will be upgraded:")) {
            needs_upgrade = true;
        }
    }
    pclose(stream);

    return needs_upgrade;
}

void upgrade_system()
{
    FILE *stream;
    char buffer[2048];
    const char * command = "apt-get dist-upgrade "
                           "-o Dir::Etc::sourcelist=\"sources.list.d/microninja.list\" "
                           "-o Dir::Etc::sourceparts=\"-\" -o APT::Get::List-Cleanup=\"0\" "
                           "-y -q --force-yes;apt-get -f install -y -q --force-yes";
    stream = popen(command, "r");
    if(stream == NULL)
    {
        perror("popen");
        return;
    }

    bool needs_upgrade = true;
    while(fgets(buffer, 2048, stream))
    {
        //do nothing, in this case
    }
    pclose(stream);
    execute_updater_script();
}

int main(int argc, char** argv)
{
    if(argc != 2) {
        fprintf(stderr, "Wrong arguments number\n");
        return 0;
    }

    //will be executed in background
    if (daemon(0, 0) == -1) {
        fprintf(stderr, "daemon error: %s", strerror(errno));
        return EXIT_FAILURE;
    }

    if(!strcmp(argv[1], "check")) {
        //urgent update execution
        execute_urgent_updates();
        UpdatingStatus status = CHECKING_FOR_NORMAL_UPDATES;
        write_status(status);

        execute_apt_update();
        bool result = check_upgrades_available();

        if(result) {
            UpdatingStatus status = UPDATES_AVAILABLE;
            write_status(status);
        } else {
            UpdatingStatus status = NO_UPDATES_AVAILABLE;
            write_status(status);
        }
    }

    else if(!strcmp(argv[1], "download")) {
        upgrade_system();

        UpdatingStatus status = SYSTEM_UPDATED;
        write_status(SYSTEM_UPDATED);
    }
}


