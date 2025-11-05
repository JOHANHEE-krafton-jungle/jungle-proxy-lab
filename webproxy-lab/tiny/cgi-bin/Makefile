CC = gcc
CFLAGS = -O2 -Wall -I ..
LDFLAGS = -lpthread

all: adder

adder: adder.c csapp.o
	$(CC) $(CFLAGS) -o adder adder.c csapp.o $(LDFLAGS)

csapp.o: ../csapp.c
	$(CC) $(CFLAGS) -c ../csapp.c

clean:
	rm -f adder *.o *~


