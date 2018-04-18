# Builds the microninja_updater.so widget for LXPanel
#
# Copyright (C) 2014 Kano Computing Ltd.
# License: http://www.gnu.org/licenses/gpl-2.0.txt GNU GPL v2

# Copyright (C) 2016 Alfabook srl.
# License: http://www.gnu.org/licenses/gpl-2.0.txt GNU GPL v2
# rebadged with microninja

CC=gcc
CFLAGS=-I../include `pkg-config --cflags lxpanel gtk+-2.0`
LIBS=`pkg-config --libs lxpanel gtk+-2.0` -lkdesk-hourglass
SRC=microninja_updater.c
BIN=microninja_updater.so
INSTALL_PATH=/usr/lib/`dpkg-architecture -qDEB_HOST_MULTIARCH`/lxpanel/plugins/

.PHONY: init

build: $(BIN)

#init:
#	cd .. && git submodule init
#	cd .. && git submodule update

install: $(BIN)
	install -p -m 0644 $(BIN) $(INSTALL_PATH)

$(BIN): $(SRC)
	$(CC) -Wall $(CFLAGS) -shared -fPIC $(SRC) -o $(BIN) $(LIBS)
