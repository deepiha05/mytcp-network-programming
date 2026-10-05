// MyTCP: message-oriented sockets built on top of TCP.
//
// Everything sent with one my_send() call is received by exactly one
// my_recv() call, reliably and in FIFO order. The calls take the same
// parameters and return the same values as the corresponding TCP socket
// calls, except that my_socket() only accepts SOCK_MyTCP as the type.
//
// A program may have one MyTCP socket at a time; a server is iterative
// (it serves one accepted connection at a time).

#ifndef MYSOCKET_H
#define MYSOCKET_H

#include <sys/types.h>
#include <sys/socket.h>

#define SOCK_MyTCP 1964

#define MYTCP_MAX_MSG_LEN 5000  // largest message accepted by my_send()
#define MYTCP_TABLE_SIZE  10    // entries in Send_Message / Received_Message

// Opens a TCP socket, allocates the Send_Message and Received_Message
// tables and starts the R (receive) and S (send) threads.
int my_socket(int domain, int type, int protocol);

int my_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int my_listen(int sockfd, int backlog);

// Accepts a connection; R and S then work on the returned socket.
int my_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int my_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

// Queues one message (at most MYTCP_MAX_MSG_LEN bytes) in Send_Message and
// returns immediately; blocks only while the table is full. As with send(),
// a length of 0 sends nothing and returns 0.
ssize_t my_send(int sockfd, const void *buf, size_t len, int flags);

// Blocks until a whole message is available in Received_Message and copies
// it to buf. If the message is longer than len, the extra bytes are
// discarded. Returns 0 once the peer has closed the connection.
ssize_t my_recv(int sockfd, void *buf, size_t len, int flags);

// On an accepted connection: closes that connection, the listening socket
// stays usable. On the socket from my_socket(): stops the threads, frees the
// tables and closes everything. Messages already queued with my_send() are
// sent before the connection is closed; unread received messages are
// discarded.
int my_close(int sockfd);

#endif
