CC = gcc

all: daytime/time_server daytime/time_client calculator/server calculator/client

daytime/time_server: daytime/time_server.c
	$(CC) -o $@ $<

daytime/time_client: daytime/time_client.c
	$(CC) -o $@ $<

calculator/server: calculator/server_it.c
	$(CC) -o $@ $< -lm

calculator/client: calculator/client.c
	$(CC) -o $@ $<

clean:
	rm -f daytime/time_server daytime/time_client calculator/server calculator/client
