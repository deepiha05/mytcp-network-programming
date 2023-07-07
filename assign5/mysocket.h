#ifndef MYTCP_H
#define MYTCP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#define SOCK_MyTCP 3 // Value to specify custom TCP socket type

// Struct to hold message data
typedef struct{
    char *data; // Pointer to message data
    int length; // Length of message
}  Message;

// Struct to hold socket data
typedef struct  {
    int sockfd; // Socket file descriptor
    Message *Send_Message; // Table to hold messages to send
    Message *Received_Message; // Table to hold received messages
    int Received_Message_Count;
    int Send_Message_Count; 
    pthread_t R;
    pthread_t S; 
    pthread_mutex_t send_message_mutex; 
    pthread_mutex_t recieve_message_mutex; 
} MyTCPsocket;



// Function prototypes
MyTCPsocket *my_socket(int domain, int type, int protocol);
int my_bind(MyTCPsocket *mysocket, const struct sockaddr *addr, socklen_t addrlen);
int my_listen(MyTCPsocket *mysocket, int backlog);
MyTCPsocket *my_accept(MyTCPsocket *mysocket, struct sockaddr *addr, socklen_t *addrlen);
int my_connect(MyTCPsocket *mysocket, const struct sockaddr *addr, socklen_t addrlen);
int my_send(MyTCPsocket *mysocket, const void *msg, size_t len, int flags);
int my_recv(MyTCPsocket *mysocket, void *buf, size_t len, int flags);
int my_close(MyTCPsocket *mysocket);

#endif /* MYTCP_H */