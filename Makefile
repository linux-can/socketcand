SOURCEFILES = socketcand.c statistics.c beacon.c state_bcm.c state_raw.c
EXECUTEABLE = socketcand
CC = gcc

ifeq ($(DEBUG), 1)
	CFLAGS = -Wall -Wno-parentheses -lpthread -lsocketcan -g -DDEBUG
else
	CFLAGS = -Wall -Wno-parentheses -lpthread -lsocketcan -O2
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
