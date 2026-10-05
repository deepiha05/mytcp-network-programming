CC = gcc

all: lb server client

lb: lb.c
	$(CC) -o $@ $<

server: server.c
	$(CC) -o $@ $<

client: client.c
	$(CC) -o $@ $<

clean:
	rm -f lb server client
