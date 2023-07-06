/*
			NETWORK PROGRAMMING WITH SOCKETS

In this program we illustrate the use of Berkeley sockets for interprocess
communication across the network. We show the communication between a server
process and a client process.

*/
#include <limits.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>

			/* THE SERVER PROCESS */
int main()
{
	int			sockfd, newsockfd ; /* Socket descriptors */
	int			clilen;
	struct sockaddr_in	cli_addr, serv_addr;

	int i;
	char buf[50];		/* We will use this buffer for communication */

	/* The following system call opens a socket. The first parameter
	   indicates the family of the protocol to be followed. For internet
	   protocols we use AF_INET. For TCP sockets the second parameter
	   is SOCK_STREAM. The third parameter is set to 0 for user
	   applications.
	*/
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("Cannot create socket\n");
		exit(0);
	}

	/* The structure "sockaddr_in" is defined in <netinet/in.h> for the
	   internet family of protocols. This has three main fields. The
 	   field "sin_family" specifies the family and is therefore AF_INET
	   for the internet family. The field "sin_addr" specifies the
	   internet address of the server. This field is set to INADDR_ANY
	   for machines having a single IP address. The field "sin_port"
	   specifies the port number of the server.
	*/
	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= INADDR_ANY;
	serv_addr.sin_port		= htons(20000);

	/* With the information provided in serv_addr, we associate the server
	   with its port using the bind() system call. 
	*/
	if (bind(sockfd, (struct sockaddr *) &serv_addr,
					sizeof(serv_addr)) < 0) {
		printf("Unable to bind local address\n");
		exit(0);
	}

	listen(sockfd, 5); /* This specifies that up to 5 concurrent client
			      requests will be queued up while the system is
			      executing the "accept" system call below.
			   */

	/* In this program we are illustrating a concurrent server -- one
	   which forks to accept multiple client connections concurrently.
	   As soon as the server accepts a connection from a client, it
	   forks a child which communicates with the client, while the
	   parent becomes free to accept a new connection. To facilitate
	   this, the accept() system call returns a new socket descriptor
	   which can be used by the child. The parent continues with the
	   original socket descriptor.
	*/
	while (1) {

		/* The accept() system call accepts a client connection.
		   It blocks the server until a client request comes.

		   The accept() system call fills up the client's details
		   in a struct sockaddr which is passed as a parameter.
		   The length of the structure is noted in clilen. Note
		   that the new socket descriptor returned by the accept()
		   system call is stored in "newsockfd".
		*/
		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr,
					&clilen) ;

		if (newsockfd < 0) {
			printf("Accept error\n");
			exit(0);
		}

		/* Having successfully accepted a client connection, the
		   server now forks. The parent closes the new socket
		   descriptor and loops back to accept the next connection.
		*/
		
		if (fork() == 0) 
		{

			/* This child process will now communicate with the
			   client through the send() and recv() system calls.
			*/
			close(sockfd);	/* Close the old socket since all
					   communications will be through
					   the new socket.
					*/

			/* We initialize the buffer, copy the message to it,
			   and send the message to the client. 
			*/
			
			strcpy(buf,"LOGIN: ");
			send(newsockfd, buf, strlen(buf) + 1, 0);

			/* We again initialize the buffer, and receive a 
			   message from the client. 
			*/
			for(i=0; i < 50; i++) buf[i] = '\0';
			recv(newsockfd, buf, 50, 0);
			
			FILE    *file;
		    char    line[50];
		    int flag=0;
		    
		    file = fopen("users.txt", "r");
		    // if(file == NULL)
		    // {
		       	
		    //    	return 1;
		    // }
		    
		    while(fgets(line, 50, file))
		    {
		        int line_len=strlen(line);
		        line[line_len-1]='\0';
		        
		        if(strcmp(buf,line)==0)
		        {
		        	flag=1;
		        	break;
		        }  
		        
		    }

		    if(flag==1)
		        {
		        	strcpy(buf,"FOUND");
		        	send(newsockfd, buf, strlen(buf) + 1, 0);
		        }
		    else if(flag==0)
		        {
		        	strcpy(buf,"NOT FOUND");
		        	send(newsockfd, buf, strlen(buf) + 1, 0);
		        }
    
    		fclose(file);

					recv(newsockfd, buf, 50, 0);
					printf("command is : %s\n",buf);

					if(strcmp(buf,"pwd")==0)
					{
						// char data[512];
						// get_popen(buf,data);

				        if (getcwd(buf, sizeof(buf)) != NULL) 
				        {
					       printf("Current working dir: %s\n", buf);
					       send(newsockfd, buf, strlen(buf) + 1, 0);
					    } 
					    else 
					    {
					       strcpy(buf,"####");
					       send(newsockfd, buf, strlen(buf) + 1, 0);
					    }
					}
					else if(buf[0]=='d' && buf[1]=='i' && buf[2]=='r')
					{
						char data[50];
						// get_popen(buf,data);
				  		// send(newsockfd, data, strlen(data) + 1, 0);

						DIR *folder;
					    struct dirent *entry;
					    int files = 0;

					    int j=0;char str[50];
						for(int i=3;i<strlen(buf);i++)
						{
							if(buf[i]==' ')
							str[j++]=buf[i];
						}
						str[j]='\0';



					    if((strlen(buf)-3 == strlen(str)) || (buf[3]=='\0'))
					    {
					    	folder = opendir(".");
						    if(folder == NULL)
						    {
						        strcpy(data,"####");
						        send(newsockfd, data, strlen(data) + 1, 0);
						    }

						    while( (entry=readdir(folder)) )
						    {
						        files++;
						        printf("File %3d: %s\n",files,entry->d_name);
						       
						        strcat(data,entry->d_name);
						        strcat(data,"\t");
						    }

						    send(newsockfd, data, strlen(data) + 1, 0);

						    closedir(folder);
						}

						else if(buf[4]!='\0' && buf[3]==' ')
						{
							int j=0;char file[50];
							for(int i=4;i<strlen(buf);i++)
							{
								file[j++]=buf[i];
							}
							file[j]='\0';
							printf("folder address: %s",file);

							folder = opendir(file);
						    if(folder == NULL)
						    {
						        strcpy(data,"####");
						        send(newsockfd, data, strlen(data) + 1, 0);
						    }

						    while( (entry=readdir(folder)) )
						    {
						        files++;
						        printf("File %3d: %s\n",files,entry->d_name);
						       
						        strcat(data,entry->d_name);
						        strcat(data,"\t");
						    }

						    send(newsockfd, data, strlen(data) + 1, 0);

						    closedir(folder);
						}
						else
						{
							strcpy(buf,"$$$$");
				        	send(newsockfd, buf, strlen(buf) + 1, 0);
						}
						

					    
					}
					else if(buf[0]=='c' && buf[1]=='d')
					{
						
						int j=0;char str[50];
						for(int i=2;i<strlen(buf);i++)
						{
							if(buf[i]==' ')
							str[j++]=buf[i];
						}
						str[j]='\0';

						if(strlen(buf)==2)
						{
							
				        	char s[50];
							// get_popen(buf,data);
							printf("%s\n", getcwd(s, 50));
  
						    // using the command
						    chdir(s);
						  
						    // printing current working directory
						    printf("%s\n", getcwd(buf, 50));
						    if(strcmp(s,buf)==0)
						    {
						    	strcpy(buf,"No change in directory");
						    }
					        send(newsockfd, buf, strlen(buf) + 1, 0);
						}
						else if(strlen(buf)>=3 && buf[2] !=' ')
						{
							strcpy(buf,"$$$$");
				        	send(newsockfd, buf, strlen(buf) + 1, 0);
						}
						else if(strlen(buf)-2 == strlen(str))
						{
							
				        	char s[50];
							// get_popen(buf,data);
							printf("%s\n", getcwd(s, 50));
  
						    // using the command
						    chdir(s);
						  
						    // printing current working directory
						    printf("%s\n", getcwd(buf, 50));
						    if(strcmp(s,buf)==0)
						    {
						    	strcpy(buf,"No change in directory");
						    }
					        send(newsockfd, buf, strlen(buf) + 1, 0);
						}
						else if(strlen(buf)>3 && buf[2] ==' ')
						{
							
							int j=0; char cmd[50];
							for(int i=3;i<strlen(buf);i++)
							{
								cmd[j++]=buf[i];
							}
							cmd[j]='\0';
							printf("cmd : %s\n",cmd);
							//printf("%s\n", getcwd(cmd, 100));
  
						    // using the command
						    int ch=chdir(cmd);
						  
						    // printing current working directory
						    if(ch==0)
						    {
						    	printf("%s\n", getcwd(buf, 50));
				        		send(newsockfd, buf, strlen(buf) + 1, 0);
						    }
						    else
						    {
						    	strcpy(buf,"####");
						    	send(newsockfd, buf, strlen(buf) + 1, 0);

						    }
						}
						else
						{
							int j=0; char cmd[50];
							for(int i=3;i<strlen(buf);i++)
							{
								cmd[j++]=buf[i];
							}
							cmd[j]='\0';
							printf("cmd : %s\n",cmd);
							//printf("%s\n", getcwd(cmd, 100));
  
						    // using the command
						    int ch=chdir(cmd);
						  
						    // printing current working directory
						    if(ch==0)
						    {
						    	printf("%s\n", getcwd(buf, 50));
				        		send(newsockfd, buf, strlen(buf) + 1, 0);
						    }
						    else
						    {
						    	strcpy(buf,"####");
						    	send(newsockfd, buf, strlen(buf) + 1, 0);

						    }
						}
						
					}
					else
					{
						strcpy(buf,"$$$$");
				        send(newsockfd, buf, strlen(buf) + 1, 0);
					}


			close(newsockfd);
			exit(0);
		}

		close(newsockfd);
	}
	return 0;
}
			

