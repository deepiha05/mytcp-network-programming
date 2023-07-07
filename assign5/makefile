CC = gcc
AR = ar
CFLAGS = -Wall -Werror -pedantic -std=c99

all: libmysocket.a

libmysocket.a: mysocket.o
    $(AR) rcs $@ $^

mysocket.o: mysocket.c mysocket.h
    $(CC) $(CFLAGS) -c -o $@ $<

clean:
    rm -f *.o libmysocket.a