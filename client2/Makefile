#
#
CC=gcc
CFLAGS=-g

# comment line below for Linux machines
#LIB= -lsocket -lnsl

all: client2

client2:	client2.o
	$(CC) $(CFLAGS) -o $@ client2.o $(LIB)

client2.o:	client2.c 

clean:
	\rm -f client2

squeaky:
	make clean
	\rm -f client2.o

tar:
	cd ..; tar czvf socket.tar.gz socket/Makefile socket/server.c socket/client.c socket/README; cd socket; mv ../socket.tar.gz .

