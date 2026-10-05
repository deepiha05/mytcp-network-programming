// Iterative MyTCP echo server: every message received is sent straight back.
//
// usage: ./server [port] [clients]
//   clients = number of connections to serve before exiting (default: forever)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "mysocket.h"

int main(int argc, char *argv[])
{
    int port = argc > 1 ? atoi(argv[1]) : 20000;
    int clients = argc > 2 ? atoi(argv[2]) : -1;
    char buf[MYTCP_MAX_MSG_LEN];

    int sockfd = my_socket(AF_INET, SOCK_MyTCP, 0);
    if (sockfd < 0)
    {
        perror("my_socket");
        exit(1);
    }

    int one = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port);

    if (my_bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("my_bind");
        exit(1);
    }
    my_listen(sockfd, 5);
    printf("server: listening on port %d\n", port);
    fflush(stdout);

    for (int served = 0; clients < 0 || served < clients; served++)
    {
        struct sockaddr_in cli_addr;
        socklen_t clilen = sizeof(cli_addr);
        int newsockfd = my_accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (newsockfd < 0)
        {
            perror("my_accept");
            exit(1);
        }

        int count = 0;
        ssize_t n;
        while ((n = my_recv(newsockfd, buf, sizeof(buf), 0)) > 0)
        {
            if (my_send(newsockfd, buf, n, 0) < 0)
            {
                perror("my_send");
                break;
            }
            count++;
        }
        printf("server: client %s:%d done, echoed %d messages\n",
               inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port), count);
        fflush(stdout);
        my_close(newsockfd);
    }

    my_close(sockfd);
    return 0;
}
