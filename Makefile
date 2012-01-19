a
%CFLAGS = -Wall -g -DNDEBUG
CFLAGS = -Wall -g

all:
	gcc -c $(CFLAGS) -o sockets.o sockets.c
	gcc -c $(CFLAGS) -o crc.o crc.c
	gcc $(CFLAGS) -o client sockets.o crc.o client.c
	gcc $(CFLAGS) -o server sockets.o crc.o server.c
