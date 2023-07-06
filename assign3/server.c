#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>  
#include <math.h> 

int main(int argc, char * argv[])
{
	int			sockfd1, newsockfd1; 
	int			clilen;
	struct sockaddr_in	cli_addr, serv_addr_1;
	int x1, port1;
	char load1[50];

	int i;
	char buf[100];

	port1 = atoi(argv[1]);

	if ((sockfd1 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Cannot create socket\n");
		exit(0);
	}

	serv_addr_1.sin_family		= AF_INET;
	serv_addr_1.sin_addr.s_addr	= INADDR_ANY;
	serv_addr_1.sin_port		= htons(port1);

	if (bind(sockfd1, (struct sockaddr *) &serv_addr_1,
					sizeof(serv_addr_1)) < 0) {
		perror("Unable to bind local address for server\n");
		exit(0);
	}

	listen(sockfd1, 5);

	while (1) 
	{

		clilen = sizeof(cli_addr);
		newsockfd1 = accept(sockfd1, (struct sockaddr *) &cli_addr,
					&clilen);
		if (newsockfd1 < 0) 
		{
			perror("Accept error in server\n");
			exit(0);
		}

			for(i=0; i < 100; i++) buf[i] = '\0';
				recv(newsockfd1, buf, 100, 0);

			if(strcmp(buf, "Send Load")==0)
			{
				srand(port1);
				x1 = (rand() % 100)+1;
				sprintf(load1, "%d", x1);
				
				strcpy(buf, load1);
				printf("Load sent : %s\n",buf);
				send(newsockfd1, buf, strlen(buf) + 1, 0);
			
			}
			
			else if(strcmp(buf,"Send Time")==0)
			{
				time_t t;
				time(&t);
				strcpy(buf, ctime(&t));
				send(newsockfd1, buf, strlen(buf) + 1, 0);
				
			}	
		close(newsockfd1);
			
	}
	return 0;
}
			

