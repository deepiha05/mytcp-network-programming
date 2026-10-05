CC = gcc
AR = ar
CFLAGS = -Wall -Werror -std=gnu99
PORT = 20005

all: libmsocket.a server client

libmsocket.a: mysocket.o
	$(AR) rcs $@ $^

mysocket.o: mysocket.c mysocket.h
	$(CC) $(CFLAGS) -c -o $@ $<

server: server.c mysocket.h libmsocket.a
	$(CC) $(CFLAGS) -o $@ server.c -L. -lmsocket -pthread

client: client.c mysocket.h libmsocket.a
	$(CC) $(CFLAGS) -o $@ client.c -L. -lmsocket -pthread

# Start an echo server for two clients, run the test client twice against it
# (the second run checks the iterative server can take a new connection).
test: server client
	@./server $(PORT) 2 & \
	sleep 1; \
	./client 127.0.0.1 $(PORT) && ./client 127.0.0.1 $(PORT); \
	status=$$?; wait; exit $$status

clean:
	rm -f *.o libmsocket.a server client
