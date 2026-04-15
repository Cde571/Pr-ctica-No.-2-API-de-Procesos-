CC=gcc
CFLAGS=-Wall -Wextra -Wpedantic -std=c11 -O2

all: wish

wish: src/wish.c
	$(CC) $(CFLAGS) -o wish src/wish.c

clean:
	rm -f wish *.o
