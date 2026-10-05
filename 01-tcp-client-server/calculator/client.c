#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


int main()
{
	int			sockfd ;
	struct sockaddr_in	serv_addr;
	int i;
	char buf[100], exp[5];

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Unable to create socket\n");
		exit(0);
	}

	serv_addr.sin_family	= AF_INET;
	inet_aton("127.0.0.1", &serv_addr.sin_addr);
	serv_addr.sin_port	= htons(20000);

	if ((connect(sockfd, (struct sockaddr *) &serv_addr,
						sizeof(serv_addr))) < 0) {
		perror("Unable to connect to server\n");
		exit(0);
	}

	for(i=0; i < 100; i++) buf[i] = '\0';
	
	for(int j = 0; i<5; j++) exp[i] = '\0';

	while(1)
	{
		printf("Enter an arithmetic expression: \n");	
		size_t len = 0;
		char c;
		for(i = 0; i < 100; i++)
			buf[i] = '\0';
		for(i = 0; i < 5; i++)
			exp[i] = '\0';
		while((c = getchar()) && c != '\n'){
			exp[len++]=c;
			if(len==5){
				send(sockfd, exp, 5, 0);
				len = 0;
				for(i = 0; i < 5; i++)
					exp[i] = '\0';
			}
		}
		send(sockfd, exp, 5, 0);
		recv(sockfd, buf, 100, 0);
		if(strcmp(buf, "exit") == 0)
			break;
		printf("Answer = %s\n", buf);
	}
	close(sockfd);
	return 0;
}

