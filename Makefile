%CFLAGS = -Wall -g -DNDEBUG
CFLAGS = -Wall -g

all:
	gcc -c $(CFLAGS) -o common.o common.c
	gcc -c $(CFLAGS) -o clientFuncs.o clientFuncs.c
	gcc -c $(CFLAGS) -o serverFuncs.o serverFuncs.c
	gcc -c $(CFLAGS) -o crc.o crc.c
	gcc $(CFLAGS) -o client common.o crc.o clientFuncs.o client.c
	gcc $(CFLAGS) -o server common.o crc.o serverFuncs.o server.c

clean:
	rm -f *.o client server
