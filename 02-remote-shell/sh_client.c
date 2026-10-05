/*    THE CLIENT PROCESS

   Logs in to the remote shell server, then reads shell commands from the
   keyboard, sends them to the server and prints the results, until the
   user types "exit".

   All strings are sent null-terminated. Client and server strictly take
   turns (one request, then one reply), so a string ends at its '\0'.
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CHUNK 50

/* Sends str including its terminating '\0'. Returns 0 on success, -1 on error. */
int send_string(int sockfd, const char *str)
{
	size_t len = strlen(str) + 1, sent = 0;
	while (sent < len) {
		ssize_t n = send(sockfd, str + sent, len - sent, 0);
		if (n <= 0)
			return -1;
		sent += n;
	}
	return 0;
}

/* Receives one null-terminated string of any length, in chunks of CHUNK
   bytes. Returns a malloc'd string, or NULL if the connection closed. */
char *recv_string(int sockfd)
{
	size_t cap = CHUNK, len = 0;
	char *str = malloc(cap);
	while (1) {
		if (len + CHUNK > cap)
			str = realloc(str, cap *= 2);
		ssize_t n = recv(sockfd, str + len, CHUNK, 0);
		if (n <= 0) {
			free(str);
			return NULL;
		}
		len += n;
		if (memchr(str + len - n, '\0', n) != NULL)
			return str;
	}
}

int main()
{
	int			sockfd ;
	struct sockaddr_in	serv_addr;
	char line[1024];

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Unable to create socket\n");
		exit(0);
	}

	/* IF YOUR SERVER RUNS ON SOME OTHER MACHINE, CHANGE THE IP ADDRESS
	   BELOW TO THE IP ADDRESS OF THE MACHINE WHERE THE SERVER RUNS. */
	serv_addr.sin_family	= AF_INET;
	inet_aton("127.0.0.1", &serv_addr.sin_addr);
	serv_addr.sin_port	= htons(20000);

	if ((connect(sockfd, (struct sockaddr *) &serv_addr,
						sizeof(serv_addr))) < 0) {
		perror("Unable to connect to server\n");
		exit(0);
	}

	/* "LOGIN:" prompt from the server */
	char *reply = recv_string(sockfd);
	if (reply == NULL) {
		printf("Server closed the connection\n");
		exit(0);
	}
	printf("%s ", reply);
	free(reply);

	if (fgets(line, sizeof(line), stdin) == NULL)
		exit(0);
	line[strcspn(line, "\n")] = '\0';
	send_string(sockfd, line);

	reply = recv_string(sockfd);
	if (reply == NULL || strcmp(reply, "FOUND") != 0) {
		printf("Invalid username\n");
		free(reply);
		close(sockfd);
		return 0;
	}
	free(reply);

	while (1) {
		printf("Enter a command: ");
		fflush(stdout);
		if (fgets(line, sizeof(line), stdin) == NULL)
			break;
		line[strcspn(line, "\n")] = '\0';
		if (strcmp(line, "exit") == 0)
			break;

		if (send_string(sockfd, line) < 0 || (reply = recv_string(sockfd)) == NULL) {
			printf("Server closed the connection\n");
			break;
		}

		if (strcmp(reply, "$$$$") == 0)
			printf("Invalid command\n");
		else if (strcmp(reply, "####") == 0)
			printf("Error in running command\n");
		else
			printf("%s\n", reply);
		free(reply);
	}

	close(sockfd);
	return 0;
}
