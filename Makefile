#
#
CC=gcc
CFLAGS=-g

# comment line below for Linux machines
#LIB= -lsocket -lnsl

all: server client

server:	server.o
	$(CC) $(CFLAGS) -o $@ server.o $(LIB)

client:	client.o
	$(CC) $(CFLAGS) -o $@ client.o $(LIB)

server.o:	server.c

client.o:	client.c 

clean:
	\rm -f server client

squeaky:
	make clean
	\rm -f server.o client.o

tar:
	cd ..; tar czvf socket.tar.gz socket/Makefile socket/server.c socket/client.c socket/README; cd socket; mv ../socket.tar.gz .

