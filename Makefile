#
# Makefile.  Generated from Makefile.in by configure.
#

sourcefiles = socketcand.c statistics.c beacon.c state_bcm.c state_raw.c state_control.c
executable = socketcand
srcdir = .
prefix = /usr/local
datarootdir = ${prefix}/share
bindir = ${exec_prefix}/bin
mandir = ${datarootdir}/man
sysconfdir = ${prefix}/etc
SCRIPT = "init"
CFLAGS = -g -O2
LIBS = -lpthread -lconfig 
init_script = yes
rc_script = no
CC = gcc

all: socketcand

socketcand: $(SOURCEFILES)
	$(CC) $(CFLAGS) -B static $(LIBS) -o $(executable) $(sourcefiles)

clean:
	rm -f $(executable) *.o

distclean:
	rm -f $(executable) *.o *~

install: socketcand
	cp $(srcdir)/socketcand $(bindir)/
	cp $(srcdir)/socketcand.1 $(mandir)/
	cp -n $(srcdir)/etc/socketcand.conf $(sysconfdir)/
	if [ $(init_script) = yes ]; then cp $(srcdir)/init.d/socketcand $(prefix)/etc/init.d/socketcand; fi
	if [ $(rc_script) = yes ]; then cp $(srcdir)/rc.d/socketcand $(prefix)/etc/rc.d/socketcand; fi
