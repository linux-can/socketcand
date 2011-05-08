SOURCEFILES = socketcand.c statistics.c beacon.c state_bcm.c state_raw.c state_control.c
EXECUTABLE = socketcand
CC = gcc
VERSION_STRING = \"0.1.2\"
DESTDIR =
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = /usr/share/man/man1
SCRIPT = "init"

ifeq ($(DEBUG), 1)
	CFLAGS = -Wall -Wno-parentheses -lpthread -lconfig -g -DDEBUG -DVERSION_STRING=$(VERSION_STRING)
else
	CFLAGS = -Wall -Wno-parentheses -lpthread -lconfig -O2 -DVERSION_STRING=$(VERSION_STRING)
endif

all: socketcand

socketcand: $(SOURCEFILES)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(SOURCEFILES)

clean:
	rm -f $(EXECUTEABLE) *.o

distclean:
	rm -f $(EXECUTEABLE) *.o *~

install: socketcand
	cp socketcand $(DESTDIR)$(BINDIR)
	cp ./socketcand.1 $(DESTDIR)$(MANDIR)/
	cp -n ./etc/socketcand.conf $(DESTDIR)/etc/
	if [ $(SCRIPT) = init ]; then cp ./init.d/socketcand $(DESTDIR)/etc/init.d/socketcand; fi
	if [ $(SCRIPT) = rc ]; then cp ./rc.d/socketcand $(DESTDIR)/etc/rc.d/socketcand; fi
