# Fortian Utilization Monitor, aka MWMG or the Multi-Window Multi-Grapher
# Copyright (c) 2019, Fortian Inc.
# All rights reserved.

# See the file LICENSE which should have accompanied this software.

CC=gcc

# Define one of DEBUG or NDEBUG but not both
# Note that builds without DEBUG have not been tested recently.
DEBUG=-O2 -ggdb -Wunused -Werror -Wall -Wno-deprecated -Wno-deprecated-declarations -DDEBUG -UNDEBUG
#NDEBUG=-O9 -g -DGTK_NO_CHECK_CASTS -DNDEBUG -UDEBUG

# It's the 21st century, everyone should have GCC 4 or higher.
# Enable more POSIX compatibility and also turn on fortified string functions.

# N.B., SHOW_STATISTICS with USE_MGEN is an untested combination; they use the
# same field, though in different messages, and flow-ids 1, 2, and 3 are
# specifically reserved.  More importantly, building without USE_MGEN is also
# untested with this version of the code; turn it off at your own risk.
CFLAGS=$(DEBUG) $(NDEBUG) -DBUILD_DATE=\"`date '+%y%m%d.%H%M%S'`\" \
  -DBUILD_USER=\"$(USER)\" -D_REENTRANT -DUSE_MGEN=1 -DENCAP_DBLCOUNT=1 \
  -DRUN_FOREVER=1 -Wno-address-of-packed-member \
  -D_FORTIFY_SOURCE=1 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 \
  -D_DEFAULT_SOURCE=1 -pthread

# For Gtk+ 1 (may no longer work):
#GTKCFLAGS=`gtk-config --cflags`
#GTKLDFLAGS=`gtk-config --libs`

# For Gtk+2 (recommended):
GTKCFLAGS=`pkg-config --cflags gtk+-2.0`
GTKLDFLAGS=`pkg-config --libs gtk+-2.0`

# TODO: port to gtk+3, especially
# https://developer.gnome.org/gtk3/stable/gtk-migrating-2-to-3.html#id-1.6.3.3.8
# For Gtk+ 3 (doesn't work yet):
#GTKCFLAGS=`pkg-config --cflags gtk+-3.0`
#GTKLDFLAGS=`pkg-config --libs gtk+-3.0`

# Various debug options:
#EFENCE=-lefence
#LDFLAGS += -pg
#CFLAGS += -pg

LDFLAGS=$(DEBUG) $(NDEBUG) $(EFENCE) -pthread

VERSION=3.1.0

.PHONY: all clean deb tar

all: mwmg recollect pcapper svcreqollect delay

clean:
	rm -f recollect mwmg pcapper svcreqollect tags *.o

pcapper.o: pcapper.c ftfm.h linuxif.h

pcapper: pcapper.o linuxif.o
	$(CC) $^ $(LDFLAGS) -lpcap -o $@

recollect.o: recollect.c

recollect: recollect.o
	$(CC) $^ $(LDFLAGS) -o $@

svcreqollect.o: svcreqollect.c

svcreqollect: svcreqollect.o
	$(CC) $^ -o $@ $(LDFLAGS) -lcurl -ljson-c

mwmg.o: CFLAGS += $(GTKCFLAGS)
mwmg.o: mwmg.c mwmg.h bits.xpm reqs.xpm

mwmg: mwmg.o
	$(CC) $^ $(LDFLAGS) $(GTKLDFLAGS) -lm -o $@

deb: mwmg_$(VERSION)-1_amd64.deb

mwmg_$(VERSION)-1_amd64.deb: debian/debian-binary debian/control.tar.gz \
    debian/data.tar.gz
	$(MAKE) -C debian $@

debian/control.tar.gz: debian/control/control debian/control/md5sums
	$(MAKE) -C debian $(@F)

debian/control/md5sums: debian/data/usr/bin/mwmg debian/data/usr/bin/recollect \
    debian/data/usr/bin/pcapper
	$(MAKE) -C debian control/md5sums

debian/data.tar.gz: debian/data/usr/bin/mwmg debian/data/usr/bin/recollect \
    debian/data/usr/bin/pcapper debian/debian-binary
	$(MAKE) -C debian $(@F)

debian/data/usr/bin/mwmg: mwmg
	cp -f $^ $@
	strip $@
	ssh -x -l root localhost chown -v root:root ${PWD}/$@

debian/data/usr/bin/recollect: recollect
	cp -f $^ $@
	strip $@
	ssh -x -l root localhost chown -v root:root ${PWD}/$@

debian/data/usr/bin/pcapper: pcapper
	cp -f $^ $@
	strip $@
	ssh -x -l root localhost chown -v root:root ${PWD}/$@

tar: clean
	tar -C .. -cvJf ../mwmg-$(shell date +%Y%m%d-%H%M%S).tar.xz --exclude-backups --exclude-vcs --exclude-vcs-ignores --exclude Attic mwmg
