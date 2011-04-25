SOURCEFILES = socketcand.c statistics.c beacon.c state_bcm.c state_raw.c state_control.c
EXECUTEABLE = socketcand
CC = gcc
VERSION_STRING = \"0.1.1\"

ifeq ($(DEBUG), 1)
	CFLAGS = -Wall -Wno-parentheses -lpthread -lconfig -g -DDEBUG -DVERSION_STRING=$(VERSION_STRING)
else
	CFLAGS = -Wall -Wno-parentheses -lpthread -lconfig -O2 -DVERSION_STRING=$(VERSION_STRING)
endif

all: socketcand

socketcand: $(SOURCEFILES)
	$(CC) $(CFLAGS) -o $(EXECUTEABLE) $(SOURCEFILES)

clean:
	rm -f $(EXECUTEABLE) *.o

distclean:
	rm -f $(EXECUTEABLE) *.o *~

install: socketcand
	cp socketcand /usr/local/bin/socketcand
	cp ./init.d/socketcand /etc/init.d/socketcand
	cp ./socketcand.1 /usr/share/man/man1/
	cp -n ./etc/socketcand.conf /etc/
