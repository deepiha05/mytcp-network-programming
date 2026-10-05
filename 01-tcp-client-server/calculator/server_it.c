#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>  
#include <ctype.h>
#include <math.h>

float numbers[10000];
char ops[10000];
int n=-1, o=-1;

void push_num(float num)
{
	numbers[++n]= num;
} 
void push_op(char ch)
{
	ops[++o]=ch;
}
float pop_num()
{
	return numbers[n--];
}
char pop_op()
{
	return ops[o--];
}

int is_operator(char ch)
{
	return(ch == '+' || ch == '-' || ch == '*' || ch == '/');
}

float simple_operation(float numbers[], char ops[])
{
	float x, y; char op;
	x = pop_num();
	y = pop_num();
	op = pop_op();
	switch(op)
	{
		case '+': return x+y;
		case '-': return y-x;
		case '*': return x*y;
		case '/': return y/x;
	}
	return 0;

}

float exp_evaluation(char *exp)
{
	int i,l;
	float r, ans, num;
	int len = strlen(exp);

	for(i=0;exp[i]!='\0';i++)
	{
		char c = exp[i];
		if(isdigit(c) != 0 || c == '.')
		{
            char *val = (char*)malloc(100);
            l = 0;
            while(isdigit(exp[i]) != 0 || exp[i] == '.')
            {
                val[l] = c;
				l++;
                i++;
                if(i<strlen(exp))
                    c=exp[i];
                else
                    break;
            }    
            i--;
			float value = atof(val);
            free(val);
            push_num(value);
		}

		else if(c == ' ')
		{
			continue;
		}

		else if(c == '(')
		{
			push_op(c);
		}
		else if(c == ')')
		{
			while(ops[o] != '(')
			{
				r = simple_operation(numbers, ops);
				push_num(r);
			}
			pop_op();
		}
		else if(is_operator(c))
		{
			while(o!= -1 && ops[o] != '(')
			{
				ans = simple_operation(numbers, ops);
				push_num(ans);
			}
			push_op(c);		
		}
		
	}
	while(o!= -1)
	{
		ans = simple_operation(numbers, ops);
		push_num(ans);
	}

	return pop_num();

}

int main()
{
	int			sockfd, newsockfd;
	int			clilen;
	struct sockaddr_in	cli_addr, serv_addr;

	int i;
	char exp[5];

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Cannot create socket\n");
		exit(0);
	}

	serv_addr.sin_family		= AF_INET;
	serv_addr.sin_addr.s_addr	= INADDR_ANY;
	serv_addr.sin_port		= htons(20000);

	if (bind(sockfd, (struct sockaddr *) &serv_addr,
					sizeof(serv_addr)) < 0) {
		perror("Unable to bind local address\n");
		exit(0);
	}

	listen(sockfd, 5); 

	while (1) 
	{

		clilen = sizeof(cli_addr);
		newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr,
					&clilen) ;

		if (newsockfd < 0) {
			perror("Accept error\n");
			exit(0);
		}

		size_t len = 0;
		int size = 100;

        while(1)
        {
            char *str;
            for(i = 0; i < 5; i++)
            {
                exp[i] = '\0';
            }
            len = 0;
            size = 5;
            str = malloc(sizeof(*str)*size);  
            do 
            {
                for(i = 0; i < 5; i++)
                    exp[i] = '\0';
                recv(newsockfd, exp, 5, 0);
                for(int j = 0; j < 5; j++)
                {
                    str[len++] = exp[j];
                } 
                str = realloc(str, sizeof(*str)*(size+=5)); 
            }while(exp[4] != '\0');
            if(strcmp(str, "-1") == 0)
                break;
            float f = exp_evaluation(str);
		    gcvt(f, 5, str);
		    send(newsockfd, str, strlen(str) + 1, 0);
            for(i = 0; i < 5; i++)
                exp[i] = '\0';
            for(i = 0; i < size; i++)
                str[i] = '\0';
        }

        send(newsockfd, "exit", 100, 0);
			
		close(newsockfd);
	}
	return 0;
}
			

