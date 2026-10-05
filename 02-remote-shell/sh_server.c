/*
			REMOTE SHELL SERVER

A concurrent TCP server. For every client it forks a child that:
  1. sends "LOGIN:" and receives a username
  2. replies "FOUND" if the username is listed in users.txt, otherwise
     "NOT FOUND" and closes the connection
  3. receives shell commands one at a time and sends back the result,
     until the client closes the connection

Supported commands (implemented with system calls, not system()):
  pwd             current directory                    (getcwd)
  dir [path]      entries of path, default "."         (opendir/readdir)
  cd [path]       change directory, reply new dir      (chdir)
Invalid commands get the reply "$$$$"; a valid command that fails (for
example cd to a missing directory) gets "####".

All strings are sent null-terminated. Client and server strictly take
turns (one request, then one reply), so a string ends at its '\0'.
*/
#include <limits.h>
#include <dirent.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define CHUNK 50

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

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

/* Returns 1 if username is a line of users.txt */
int valid_user(const char *username)
{
	FILE *file = fopen("users.txt", "r");
	char line[256];
	int found = 0;

	if (file == NULL)
		return 0;
	while (!found && fgets(line, sizeof(line), file)) {
		line[strcspn(line, "\r\n")] = '\0';
		found = strcmp(line, username) == 0;
	}
	fclose(file);
	return found;
}

/* Directory listing of path, entries separated by tabs. Returns a malloc'd
   string, or NULL if the directory cannot be opened. */
char *list_dir(const char *path)
{
	DIR *folder = opendir(path);
	struct dirent *entry;
	size_t cap = 256, len = 0;
	char *data;

	if (folder == NULL)
		return NULL;
	data = malloc(cap);
	data[0] = '\0';
	while ((entry = readdir(folder)) != NULL) {
		size_t n = strlen(entry->d_name);
		while (len + n + 2 > cap)
			data = realloc(data, cap *= 2);
		memcpy(data + len, entry->d_name, n);
		len += n;
		data[len++] = '\t';
		data[len] = '\0';
	}
	closedir(folder);
	return data;
}

/* Runs one command and returns the reply to send, as a malloc'd string */
char *run_command(char *cmd)
{
	char cwd[PATH_MAX];
	char *name, *arg;

	/* split into the command name and its (optional) argument */
	name = cmd + strspn(cmd, " ");
	arg = name + strcspn(name, " ");
	if (*arg != '\0') {
		*arg++ = '\0';
		arg += strspn(arg, " ");
	}
	for (char *end = arg + strlen(arg); end > arg && end[-1] == ' '; )
		*--end = '\0';

	if (strcmp(name, "pwd") == 0 && *arg == '\0') {
		if (getcwd(cwd, sizeof(cwd)) == NULL)
			return strdup("####");
		return strdup(cwd);
	}
	if (strcmp(name, "dir") == 0) {
		char *data = list_dir(*arg ? arg : ".");
		return data ? data : strdup("####");
	}
	if (strcmp(name, "cd") == 0) {
		if (*arg == '\0')
			return strdup("No change in directory");
		if (chdir(arg) != 0 || getcwd(cwd, sizeof(cwd)) == NULL)
			return strdup("####");
		return strdup(cwd);
	}
	return strdup("$$$$");
}

/* Talks to one client; runs in the child process */
void serve_client(int newsockfd)
{
	char *buf;

	send_string(newsockfd, "LOGIN:");

	buf = recv_string(newsockfd);
	if (buf == NULL)
		return;
	if (!valid_user(buf)) {
		send_string(newsockfd, "NOT FOUND");
		free(buf);
		return;
	}
	send_string(newsockfd, "FOUND");
	free(buf);

	while ((buf = recv_string(newsockfd)) != NULL) {
		printf("command is : %s\n", buf);
		char *result = run_command(buf);
		int err = send_string(newsockfd, result);
		free(result);
		free(buf);
		if (err < 0)
			break;
	}
}

int main()
{
	int			sockfd, newsockfd ; /* Socket descriptors */
	socklen_t		clilen;
	struct sockaddr_in	cli_addr, serv_addr;

	/* Children exit on their own; don't leave them as zombies */
	signal(SIGCHLD, SIG_IGN);

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Cannot create socket\n");
		exit(0);
	}

	/* allow restarting the server right away on the same port */
	int one = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= INADDR_ANY;
	serv_addr.sin_port		= htons(20000);

	if (bind(sockfd, (struct sockaddr *) &serv_addr,
					sizeof(serv_addr)) < 0) {
		printf("Unable to bind local address\n");
		exit(0);
	}

	listen(sockfd, 5); /* up to 5 pending connections are queued */

	/* Concurrent server: a child is forked for every client, while the
	   parent goes back to accept the next connection. */
	while (1) {
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr,
					&clilen) ;

		if (newsockfd < 0) {
			printf("Accept error\n");
			exit(0);
		}

		if (fork() == 0) {
			close(sockfd);	/* the child only uses newsockfd */
			serve_client(newsockfd);
			close(newsockfd);
			exit(0);
		}

		close(newsockfd);
	}
	return 0;
}
