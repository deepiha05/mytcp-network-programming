CC = gcc

all: sh_server sh_client

sh_server: sh_server.c
	$(CC) -o $@ $<

sh_client: sh_client.c
	$(CC) -o $@ $<

clean:
	rm -f sh_server sh_client
