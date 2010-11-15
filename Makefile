CFLAGS = -O2 -Wall -Wno-parentheses -lpthread -lsocketcan -g

all: socketcand

socketcand: socketcand.c statistics.c
	gcc ${CFLAGS} -o socketcand socketcand.c statistics.c

clean:
	rm -f socketcand *.o

distclean:
	rm -f socketcand *.o *~
