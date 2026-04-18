CC = gcc
CFLAGS = -O0 -Wall -I . -g
LIB = -lpthread

all: echoserver echoclient

echoserver: echoserver.c echo.c csapp.o
	$(CC) $(CFLAGS) -o echoserver echoserver.c echo.c csapp.o $(LIB)

echoclient: echoclient.c csapp.o
	$(CC) $(CFLAGS) -o echoclient echoclient.c csapp.o $(LIB)

csapp.o: csapp.c
	$(CC) $(CFLAGS) -c csapp.c

clean:
	rm -f *.o echoserver echoclient *~
