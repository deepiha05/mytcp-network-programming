#include "mysocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAXDATASIZE 100
#define MAX_MESSAGES 1000
#define MAX_BITS_PER_BYTE 8
#define HEADER "10101010"
// #define FLAG "01111110"
#define TRAILER "/EOM"
#define ESCAPE "//"
#define MAX_BITS_PER_BYTE 8

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

// Function to remove bit stuffing from a message string
char *remove_bit_stuffing(char *message)
{
    int i, count = 0;
    char *dest = message;

    for (i = 0; message[i] != '\0'; i++)
    {
        if (message[i] == '1')
        {
            count++;
            *dest++ = '1';
        }
        else
        {
            count = 0;
            *dest++ = '0';
        }
        if (count == 5)
        {
            i++;
            count = 0;
        }
    }
    *dest = '\0';

    return message;
}

// Function to extract the message from a coded message string
char *extract_message(char *coded_message)
{
    // Check header
    if (strncmp(coded_message, HEADER, strlen(HEADER)) != 0)
    {
        printf("Error: Header not found.\n");
        return NULL;
    }

    // Remove bit stuffing
    remove_bit_stuffing(coded_message + strlen(HEADER));

    // Find trailer
    char *trailer = strstr(coded_message, TRAILER);
    if (trailer == NULL)
    {
        printf("Error: Trailer not found.\n");
        return NULL;
    }

    // Remove escape sequence if present
    char *message = (char *)malloc((trailer - coded_message) / MAX_BITS_PER_BYTE + 1);
    char *dest = message;
    char *src = coded_message + strlen(HEADER);
    while (src < trailer)
    {
        if (strncmp(src, ESCAPE, strlen(ESCAPE)) == 0)
        {
            src += strlen(ESCAPE);
        }
        *dest++ = strtol(src, &src, 2);
    }
    *dest = '\0';

    return message;
}

// Function to perform bit stuffing on a message string
char *bit_stuffing(char *message)
{
    int i, count = 0;
    char *stuffed_message = (char *)malloc(strlen(message) * 2 * sizeof(char));
    strcpy(stuffed_message, HEADER);

    for (i = 0; message[i] != '\0'; i++)
    {
        if (message[i] == '1')
        {
            count++;
            strcat(stuffed_message, "1");
        }
        else
        {
            count = 0;
            strcat(stuffed_message, "0");
        }
        if (count == 5)
        {
            count = 0;
            strcat(stuffed_message, "0");
        }
    }

    // strcat(stuffed_message, FLAG);
    return stuffed_message;
}

// function that returns bit stuffed binary message along with header and trailer
char *convert_to_binary(const char *message)
{
    const char *trailer_code = "/EOM";
    const char *escape_sequence = "//";
    int message_len = strlen(message);
    char *binary_message = malloc((message_len * MAX_BITS_PER_BYTE) + 1);
    char *binary_trailer_code = convert_to_binary(trailer_code);
    char *binary_escape_sequence = convert_to_binary(escape_sequence);

    // Iterate through each character of the input message and convert to binary
    for (int i = 0; i < message_len; i++)
    {
        char binary_char[MAX_BITS_PER_BYTE + 1];
        memset(binary_char, 0, sizeof(binary_char));

        // Check if the current character matches the trailer code
        if (strncmp(&message[i], trailer_code, strlen(trailer_code)) == 0)
        {
            // Append escape sequence to binary message
            strcat(binary_message, binary_escape_sequence);
            i += strlen(trailer_code) - 1; // Skip over trailer code
        }
        else
        {
            // Convert character to binary string
            int char_val = (int)message[i];
            for (int j = MAX_BITS_PER_BYTE - 1; j >= 0; j--)
            {
                if (char_val & (1 << j))
                {
                    strcat(binary_char, "1");
                }
                else
                {
                    strcat(binary_char, "0");
                }
            }
            // Copy binary string into binary message
            strcat(binary_message, binary_char);
        }
    }

    // Append binary trailer code to binary message
    char *stuffed_message;
    strcpy(stuffed_message, bit_stuffing(binary_message));
    strcat(binary_message, binary_trailer_code);

    free(binary_trailer_code);
    free(binary_escape_sequence);

    return binary_message; // returns bit stuffed binary message along with header and trailer
}

void *Receive_Thread(void *arg)
{
    printf("Receive Thread Started\n");
    sleep(15);                                  // thread R
    printf("Receive Thread after sleep\n");
    MyTCPsocket *mysocket = (MyTCPsocket *)arg; // Cast arg to MyTCPsocket pointer
    // Implement receive logic here
    printf("Receive Thread 1\n");
    char buffer[1000];      // Buffer to store received data
    int lenbytes, msgbytes; // Number of bytes received
    printf("Receive Thread 2\n");
    // Receive loop
    while (1)
    {
        printf("Receive Thread 3\n");
        // Receive data from socket
        lenbytes = recv(mysocket->sockfd, buffer, sizeof(buffer), 0);
        if (lenbytes == -1)
        {
            perror("recv");
            exit(1);
        }
    }
    printf("Receive Thread 4\n");
    lenbytes = atoi(buffer);
    int temp = 0;
    char *str;
    int f = 0;
    printf("Receive Thread 5\n");
    while (temp != lenbytes)
    {
        printf("Receive Thread 6\n");
        msgbytes = recv(mysocket->sockfd, buffer, 1000, 0); // Receive data from socket
        f++;
        if (f == 1)
        {
            strcpy(str, buffer);
        }
        else
        {
            strcat(str, buffer);
        }
        temp += msgbytes;
        if (msgbytes == -1)
        {
            perror("recv");
            exit(1);
        }
    }
    printf("Receive Thread 7\n");
    // strcpy(buffer,extract_message(buffer));
    int numbytes = strlen(str);

    printf("Receive thread 8\n");
    // Update Received_Message table
    mysocket->Received_Message[mysocket->Received_Message_Count].data = (char *)malloc(numbytes * sizeof(char));
    pthread_mutex_lock(&mysocket->recieve_message_mutex);
    printf("Receive thread 9\n");
    // Add the message to the receive table
    strcpy(mysocket->Received_Message[mysocket->Received_Message_Count].data, str);
    mysocket->Received_Message[mysocket->Received_Message_Count].length = numbytes;
    mysocket->Received_Message_Count++;
    printf("Receive thread 10\n");
    // Release the lock on the receive_message_mutex
    pthread_mutex_unlock(&mysocket->recieve_message_mutex);
    printf("Receive thread 11\n");
    pthread_exit(NULL);
}

void *Send_Thread(void *arg)
{ // thread S

    printf("Send Thread Started\n");
    sleep(5);
    printf("Send Thread after sleep\n");
    MyTCPsocket *mysocket = (MyTCPsocket *)arg;
    printf("Send Thread 1\n");
    // Implement send logic here
    int sockfd = mysocket->sockfd;
    printf("Send Thread 2\n");

    while (1)
    {
        printf("Send Thread 3\n");
        // Wait for a message to send
        printf("msg count%d\n", mysocket->Send_Message_Count);
        while (mysocket->Send_Message_Count == 0)
        {
            usleep(10000); // Sleep for 10ms to avoid busy waiting
            printf("Send Thread 3.5\n");
            pthread_cond_wait(&cond, NULL);
        }
        printf("Send Thread 4\n");
        // Get the next message from the send table
        Message *message=(Message*)malloc(sizeof(Message));
        pthread_mutex_lock(&mysocket->send_message_mutex);
        printf("Send Thread 5\n");
        *message = mysocket->Send_Message[0];
        printf("Send Thread 6\n");
        // Remove the message from the send table by shifting all the remaining messages
        for (int i = 1; i < mysocket->Send_Message_Count; i++)
        {
            mysocket->Send_Message[i - 1] = mysocket->Send_Message[i];
        }
        printf("Send Thread 7\n");
        // Decrement the send count
        mysocket->Send_Message_Count--;
        printf("Send Thread 8\n");
        pthread_mutex_unlock(&mysocket->send_message_mutex);
            printf("Send Thread 9\n");
        // Send message to socket
        char *s;
        sprintf(s, "%d", message->length);
        printf("Send Thread 10\n"); 
        int length_sent = send(sockfd, s, sizeof(s), 0);
        printf("Send Thread 11\n");
        if (length_sent < 0)
        {
            // Handle send error
            perror("send");
            exit(1);
        }
        int temp = 0;
        while (temp != message->length)
        {
            printf("Send Thread 12\n");
            int message_sent = send(sockfd, message->data + temp, sizeof(message->data), 0);
            temp += message_sent;
            if (message_sent < 0)
            {
                // Handle send error
                perror("send");
                exit(1);
            }
        }
        printf("Send Thread 13\n");
    }

    return NULL;
}

// Function to create a new MyTCP socket
MyTCPsocket *my_socket(int domain, int type, int protocol)
{
    // Allocate memory for MyTCPsocket structure
    MyTCPsocket *mysocket = (MyTCPsocket *)malloc(sizeof(MyTCPsocket));
    if (mysocket == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate memory for MyTCPsocket structure\n");
        return NULL;
    }

    // Create socket
    mysocket->sockfd = socket(domain, type, protocol);
    if (mysocket->sockfd == -1)
    {
        fprintf(stderr, "Error: Failed to create socket\n");
        free(mysocket);
        return NULL;
    }

    mysocket->Received_Message_Count = 0;
    mysocket->Send_Message_Count = 0;

    // Allocate memory for Send_Message and Received_Message tables
    mysocket->Send_Message = (Message *)malloc(sizeof(Message));
    if (mysocket->Send_Message == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate memory for Send_Message table\n");
        free(mysocket);
        return NULL;
    }
    mysocket->Received_Message = (Message *)malloc(sizeof(Message));
    if (mysocket->Received_Message == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate memory for Received_Message table\n");
        free(mysocket->Send_Message);
        free(mysocket);
        return NULL;
    }

    // Initialize Send_Message and Received_Message tables
    for (int i = 0; i < mysocket->Send_Message_Count; i++)
    {
        mysocket->Send_Message[i].data = (char *)malloc(1 * sizeof(char));
        memset(mysocket->Send_Message[i].data, '\0', 1 * sizeof(char));
        mysocket->Send_Message[i].length = 0;
    }

    for (int i = 0; i < mysocket->Received_Message_Count; i++)
    {
        mysocket->Received_Message[i].data = (char *)malloc(1 * sizeof(char));
        memset(mysocket->Received_Message[i].data, '\0', 1 * sizeof(char));
        mysocket->Received_Message[i].length = 0;
    }
    // mysocket->Send_Message->data = NULL;
    // mysocket->Send_Message->length = 0;
    // mysocket->Received_Message->data = NULL;
    // mysocket->Received_Message->length = 0;

    // Initialize R and S threads
    pthread_mutex_init(&mysocket->send_message_mutex, NULL); 
    pthread_mutex_init(&mysocket->recieve_message_mutex, NULL); 
    
    if (pthread_create(&mysocket->R, NULL, Receive_Thread, mysocket) != 0)
    {
        fprintf(stderr, "Error: Failed to create receive thread\n");
        free(mysocket->Received_Message);
        free(mysocket->Send_Message);
        free(mysocket);
        return NULL;
    }
    if (pthread_create(&mysocket->S, NULL, Send_Thread, mysocket) != 0)
    {
        fprintf(stderr, "Error: Failed to create send thread\n");
        pthread_cancel(mysocket->R);
        free(mysocket->Received_Message);
        free(mysocket->Send_Message);
        free(mysocket);
        return NULL;
    }

    return mysocket;
}

int my_bind(MyTCPsocket *mysocket, const struct sockaddr *addr, socklen_t addrlen)
{
    int result = bind(mysocket->sockfd, addr, addrlen);
    if (result < 0)
    {
        perror("bind failed");
        return -1;
    }
    return 0;
}

int my_listen(MyTCPsocket *mysocket, int backlog)
{
    if (listen(mysocket->sockfd, backlog) < 0)
    {
        perror("Error in my_listen(): ");
        return -1;
    }
    return 0;
}

MyTCPsocket *my_accept(MyTCPsocket *mysocket, struct sockaddr *addr, socklen_t *addrlen)
{
    printf("my accept started\n");
    MyTCPsocket *newsocket = (MyTCPsocket *)malloc(sizeof(MyTCPsocket));
    if (newsocket == NULL)
    {
        fprintf(stderr, "Error: Unable to allocate memory for new socket\n");
        return NULL;
    }
    printf("my accept 1\n");
    // Allocate memory for Send_Message and Received_Message tables
    newsocket->Send_Message = (Message *)malloc(sizeof(Message));
    if (newsocket->Send_Message == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate memory for Send_Message table\n");
        free(newsocket);
        return NULL;
    }
    printf("my accept 2\n");
    newsocket->Received_Message = (Message *)malloc(sizeof(Message));
    if (newsocket->Received_Message == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate memory for Received_Message table\n");
        free(newsocket->Send_Message);
        free(newsocket);
        return NULL;
    }
    printf("my accept 3\n");
    // Initialize Send_Message and Received_Message tables
    for (int i = 0; i < mysocket->Send_Message_Count; i++)
    {
        mysocket->Send_Message[i].data = (char *)malloc(1 * sizeof(char));
        memset(mysocket->Send_Message[i].data, '\0', 1 * sizeof(char));
        mysocket->Send_Message[i].length = 0;
    }
    printf("my accept 4\n");
    for (int i = 0; i < mysocket->Received_Message_Count; i++)
    {
        mysocket->Received_Message[i].data = (char *)malloc(1 * sizeof(char));
        memset(mysocket->Received_Message[i].data, '\0', 1 * sizeof(char));
        mysocket->Received_Message[i].length = 0;
    }
    printf("my accept 5\n");
    // newsocket->Send_Message->data = NULL;
    // newsocket->Send_Message->length = 0;
    // newsocket->Received_Message->data = NULL;
    // newsocket->Received_Message->length = 0;

    // pthread_create(&newsocket->R, NULL, Receive_Thread, (void *)newsocket);
    
    newsocket->sockfd = accept(mysocket->sockfd, addr, addrlen);
    if (newsocket->sockfd < 0)
    {
        fprintf(stderr, "Error: Unable to accept new connection\n");
        return NULL;
    }
    printf("my accept ended\n");
    return newsocket;
}

int my_connect(MyTCPsocket *mysocket, const struct sockaddr *addr, socklen_t addrlen)
{
    printf("my connect started\n");
    // Call the standard connect() function to establish a connection with the specified address
    int ret = connect(mysocket->sockfd, addr, addrlen);
    printf("my connect 1\n");
    if (ret < 0)
    {
        // Error occurred, return the error code
        printf("my connect if\n");
        return ret;
    }
    else
    {
        // Connection established successfully
        // Start the receive thread to receive data from the server
        // pthread_create(&mysocket->R, NULL, Receive_Thread, (void *)mysocket);
        printf("my connect else\n");
        return 0;
    }
}

// Function to send data to a socket
int my_send(MyTCPsocket *mysocket, const void *msg, size_t len, int flags)
{   
    printf("my send started\n");
    // Allocate memory for a new message
    Message *new_message= (Message *)malloc(sizeof(Message));
    printf("my send started1\n");
    // new_message = (Message *)malloc(sizeof(Message));
    printf("my send started2\n");
    new_message->data = (char *)malloc(len + 1); // Allocate memory for the message data
    printf("my send started3\n");
    printf("my send 1\n");
    // Allocate memory for the message data

    // Copy the message data to the new message
    strcpy(new_message->data, msg);
    new_message->length = len;
    printf("my send 2\n");
    // do bit stuffing, add headers and trailer
    // strcpy(new_message.data,convert_to_binary(new_message.data)); //convert_to_binary is a function that converts the message to binary
    // new_message.length = strlen(new_message.data); //update the length of the message

    // Assume that the MyTCPsocket struct has been initialized

    // Lock the send_message_mutex to prevent other threads from modifying the table
    pthread_mutex_lock(&mysocket->send_message_mutex);
    printf("my send 3\n");
    // Add the message to the Send_Message table
    int index = 0; // Find an empty slot in the Send_Message table
    while (mysocket->Send_Message[index].data != NULL && index < MAX_MESSAGES)
    {
        index++;
    }
    printf("my send 4\n");
    if (index < MAX_MESSAGES)
    {
        mysocket->Send_Message[index] = *new_message;
    }
    printf("my send 5\n");

    // pthread_mutex_unlock the send_message_mutex to allow other threads to modify the table
    pthread_mutex_unlock(&mysocket->send_message_mutex);
    
    printf("my send 6\n");
    // Add the message to the send message table
    // mysocket->Send_Message = new_message;
    mysocket->Send_Message_Count++;
    pthread_cond_signal(&cond);
    printf("msg count=%d\n", mysocket->Send_Message_Count);
    printf("my send 7\n");
    // Notify the send thread that a new message is ready to send
    // pthread_cond_signal(&mysocket->S_cond);

    return 0;
}

int my_recv(MyTCPsocket *mysocket, void *buf, size_t len, int flags)
{
    printf("my recv started\n");
    // Lock the receive message mutex
    pthread_mutex_lock(&mysocket->recieve_message_mutex);
    printf("my recv 1\n");
    // Get the first message in the table
    Message *msg= (Message *)malloc(sizeof(Message));
    *msg = mysocket->Received_Message[0];
    msg->data= (char *)malloc(msg->length + 1);
    printf("my recv 2\n");  
    // Shift all messages in the table one index to the left
    for (int i = 1; i < MAX_MESSAGES; i++)
    {
        mysocket->Received_Message[i - 1] = mysocket->Received_Message[i];
    }
    printf("my recv 3\n");
    // Decrement the message count
    mysocket->Received_Message_Count--;
    printf("my recv 4\n");
    // pthread_mutex_unlock the receive message mutex
    pthread_mutex_unlock(&mysocket->recieve_message_mutex);
    printf("my recv 5\n");
    printf("The message is : %s", msg->data);

    return 0;
}

int my_close(MyTCPsocket *mysocket)
{
    sleep(5);
    // Stop receive and send threads
    pthread_cancel(mysocket->R);
    pthread_cancel(mysocket->S);

    // Close socket file descriptor
    int status = close(mysocket->sockfd);

    // Free memory used by message tables
    free(mysocket->Send_Message);
    free(mysocket->Received_Message);

    // Free memory used by MyTCPsocket struct
    free(mysocket);

    return status;
}
