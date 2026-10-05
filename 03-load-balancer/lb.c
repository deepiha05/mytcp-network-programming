#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <poll.h>
#include <arpa/inet.h>
#include <time.h>

int main(int argc, char * argv[])
{
	int			sockfd1, sockfd2, sockfd, newsockfd ; 
	int			clilen;
	struct sockaddr_in	cli_addr, serv_addr, serv_addr_1, serv_addr_2;
	
	int port_l, port1, port2;

	port_l = atoi(argv[1]);
	port1 = atoi(argv[2]);		
	port2 = atoi(argv[3]);
	
	int i;
	char buf[100], buf1[100], buf2[100];
	int load1=0, load2=0;	
	time_t time_initial, time_after_client;	

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Cannot create socket for loader\n");
		exit(0);
	}

	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= INADDR_ANY;
	serv_addr.sin_port		= htons(port_l);

	if (bind(sockfd, (struct sockaddr *) &serv_addr,
					sizeof(serv_addr)) < 0) {
		perror("Unable to bind local address for loader\n");
		exit(0);
	}

	listen(sockfd, 5);

	struct pollfd pollset[1];
	pollset[0].fd=sockfd;
	pollset[0].events=POLLIN;

	int ret1, timelimit = 5000;

	while(1)
	{
		time_initial = time(NULL);
		ret1 = poll(pollset,1,timelimit);

		if( ret1 == 1)
		{
			clilen = sizeof(cli_addr);
			newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr,
						&clilen) ;

			if (newsockfd < 0) {
				printf("Accept error\n");
				exit(0);
			}

			// fork a child process to handle client requests from client to loader
			if(fork() == 0)
			{
				close(sockfd);

				if(load1 >= load2)
				{
					// send client rquest to server 2

					printf("Sending client request to %d\n", port2);
					if ((sockfd2 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
						printf("Cannot create socket 2\n");
						exit(0);
					}
					serv_addr_2.sin_family	= AF_INET;
					inet_aton("127.0.0.1", &serv_addr_2.sin_addr);
					serv_addr_2.sin_port	= htons(port2);

					if ((connect(sockfd2, (struct sockaddr *) &serv_addr_2,
										sizeof(serv_addr_2))) < 0) {
						perror("Unable to connect to server 2\n");
						exit(0);
					}
					strcpy(buf2, "Send Time");
					send(sockfd2, buf2, strlen(buf2)+1, 0);
					for(i=0; i < 100; i++) buf2[i] = '\0';
					recv(sockfd2, buf2, 100, 0);
					// printf("buf2 = %s\n",buf2 );
					close(sockfd2);
					// send the result to client
					send(newsockfd, buf2, strlen(buf2) + 1, 0);
					close(newsockfd);
					exit(0);
				}
				else
				{
					// send client request to server 1

					printf("Sending client request to %d\n", port1);
					if ((sockfd1 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
					printf("Cannot create socket 1\n");
					exit(0);
					}
					serv_addr_1.sin_family	= AF_INET;
					inet_aton("127.0.0.1", &serv_addr_1.sin_addr);
					serv_addr_1.sin_port	= htons(port1);

					if ((connect(sockfd1, (struct sockaddr *) &serv_addr_1,
										sizeof(serv_addr_1))) < 0) {
						perror("Unable to connect to server 1\n");
						exit(0);
					}
					strcpy(buf1, "Send Time");
					send(sockfd1, buf1, strlen(buf1)+1, 0);
					for(i=0; i < 100; i++) buf1[i] = '\0';
					recv(sockfd1, buf1, 100, 0);
					// printf("buf1 = %s\n",buf1 );
					close(sockfd1);
					// send the result to client
					send(newsockfd, buf1, strlen(buf1) + 1, 0);
					close(newsockfd);
					exit(0);
				}	

			}

			close(newsockfd);
			time_after_client = time(NULL);
			timelimit = 5000 - difftime(time_after_client, time_initial)*1000;

		}

		if(ret1 == 0)
		{
			// get load from server 1 using sockfd1

			if ((sockfd1 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			printf("Cannot create socket\n");
			exit(0);
			}
			serv_addr_1.sin_family	= AF_INET;
			inet_aton("127.0.0.1", &serv_addr_1.sin_addr);
			serv_addr_1.sin_port	= htons(port1);

			if ((connect(sockfd1, (struct sockaddr *) &serv_addr_1,
								sizeof(serv_addr_1))) < 0) {
				perror("Unable to connect to server 1\n");
				exit(0);
			}

			strcpy(buf1,"Send Load");
			send(sockfd1, buf1, strlen(buf1) + 1, 0);

			for(i=0; i < 100; i++) buf1[i] = '\0';
			recv(sockfd1, buf1, 100, 0);
			printf("Load received from %d %s\n",port1, buf1);
			load1=atoi(buf1);

			close(sockfd1);

			// get load from server 2 using sockfd2

			if ((sockfd2 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
				printf("Cannot create socket\n");
				exit(0);
			}
			serv_addr_2.sin_family	= AF_INET;
			inet_aton("127.0.0.1", &serv_addr_2.sin_addr);
			serv_addr_2.sin_port	= htons(port2);

			if ((connect(sockfd2, (struct sockaddr *) &serv_addr_2,
								sizeof(serv_addr_2))) < 0) {
				perror("Unable to connect to server 2\n");
				exit(0);
			}

			strcpy(buf2,"Send Load");
			send(sockfd2, buf2, strlen(buf2) + 1, 0);

			for(i=0; i < 100; i++) buf2[i] = '\0';
			recv(sockfd2, buf2, 100, 0);
			printf("Load received from %d %s\n",port2, buf2);
			load2=atoi(buf2);

			close(sockfd2);

			timelimit = 5000;
			continue;

		}
	}

	return 0;
}
			

