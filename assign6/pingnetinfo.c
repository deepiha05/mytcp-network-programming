// Networks Assignment : 6 
// 20CS10024 - Devendra Palod
// 20CS30015 - Deepiha S

// to compile the file use the following command-
// gcc pingnetinfo.c -o p

// to run the file use the following command-
// sudo ./p <ip address> <number of hops> <timeout in seconds>

// ----------Note:-----------
// In our program we allow the Operating System to attach the IP header to the ICMP packets sent

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/ip_icmp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>

#define PACKET_SIZE 64
#define MAX_HOPS 10 // maximum number of hops assumed since we do not have very large networks for testing this program 

char sendpacket[PACKET_SIZE];
char recvpacket[PACKET_SIZE];
int sockfd;
pid_t pid;
struct sockaddr_in dest_addr;
struct sockaddr_in from;
struct timeval tvrecv;

double delay()
{
    srand(time(NULL));              // seed the random number generator with the current time
    int milliseconds = rand() % 100; // generate a random number between 0 and 100
    clock_t start_time = clock();   // get the current clock time
    while ((clock() - start_time) * 1000.0 / CLOCKS_PER_SEC < milliseconds);                                                    // wait until the desired time has elapsed
    return (clock() - start_time) * 1000.0 / CLOCKS_PER_SEC; // return the actual delay time in milliseconds as a double
}

unsigned short cal_chksum(unsigned short *addr, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = addr;
    unsigned short answer = 0;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *(unsigned char *)(&answer) = *(unsigned char *)w;
        sum += answer;
    }

    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    answer = ~sum;
    return answer;
}

// function to print icmp packet header
void printICMP(struct icmp *icmp)
{
    printf("-----------------------------------------------------------------\n");
    printf("|    type:%-2d    |    code:%-2d    |        checksum:%-6d        |\n", icmp->icmp_type, icmp->icmp_code, icmp->icmp_cksum);
    printf("-----------------------------------------------------------------\n");
    if (icmp->icmp_type == ICMP_ECHO || icmp->icmp_type == ICMP_ECHOREPLY)
        printf("|           id:%-6d           |        sequence:%-6d        |\n", icmp->icmp_id, icmp->icmp_seq);
    printf("-----------------------------------------------------------------\n");
}

// function to print ip header
void print_ip_header(struct ip *ip) {
    printf(" --------------------------------------------------------------------\n");
    printf("||==================================================================||\n");
    printf("   Version: %-2u   |  Header Length: %-2u  |  Type of Service: %-2u  \n", ip->ip_v, ip->ip_hl, ip->ip_tos);
    printf("||------------------------------------------------------------------||\n");
    printf("   Total Length: %-6u      |  Identification: %-6u                   \n", ntohs(ip->ip_len), ntohs(ip->ip_id));
    printf("||------------------------------------------------------------------||\n");
    printf("   Flags: %-2u    |  Fragment Offset: %-6u                           \n", ntohs(ip->ip_off) >> 13, ntohs(ip->ip_off) & 0x1fff);
    printf("||------------------------------------------------------------------||\n");
    printf("   Time to Live (TTL): %-3u|  Protocol: %-3u|  Header Checksum: %-6u     \n", ip->ip_ttl, ip->ip_p, ntohs(ip->ip_sum));
    printf("||------------------------------------------------------------------||\n");
    printf("   Source IP Address: %-15s                                          \n", inet_ntoa(ip->ip_src)); 
    printf("||------------------------------------------------------------------||\n");
    printf("   Destination IP Address: %-15s                                     \n", inet_ntoa(ip->ip_dst));
    printf("||==================================================================||\n");
    printf(" --------------------------------------------------------------------\n");
}

void send_packet(int ttl)
{
    int packsize;
    struct icmp *icmp;
    icmp = (struct icmp *)sendpacket;
    icmp->icmp_type = ICMP_ECHO;
    icmp->icmp_code = 0;
    icmp->icmp_cksum = 0;
    icmp->icmp_seq = ttl;
    icmp->icmp_id = pid;
   
    icmp->icmp_cksum = cal_chksum((unsigned short *)icmp, sizeof(icmp));
    setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
    printf("ICMP header of Sent packet: \n");
    printICMP(icmp);
   
    sendto(sockfd, sendpacket, sizeof(icmp), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    
}

char *data;
void send_packet_data(int ttl)
{
    data = (char *)malloc(16);
    int packsize;
    struct icmp *icmp;
    icmp = (struct icmp *)sendpacket;
    icmp->icmp_type = ICMP_ECHO;
    icmp->icmp_code = 0;
    icmp->icmp_cksum = 0;
    icmp->icmp_seq = ttl;
    icmp->icmp_id = pid;
    // packsize = 8 + datalen;
    icmp->icmp_cksum = cal_chksum((unsigned short *)icmp, packsize);
   
    setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
    strcpy(data, "Hello");
    memcpy((icmp + 8), data, sizeof(data));
    
    icmp->icmp_cksum = cal_chksum((unsigned short *)icmp, sizeof(icmp));
    printf("ICMP header of Sent packet: \n");
    printICMP(icmp);
    sendto(sockfd, sendpacket, sizeof(icmp), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
   
    free(data);
       
}

double *latency;   // Array to store latency values
double *bandwidth; // Array to store bandwidth values
int count = 0;


int recv_packet(int ttl, struct timeval *start_time)
{
    
    int m, fromlen;
    struct timeval tv, end_time;
    gettimeofday(&tv, NULL);
    
    if (gettimeofday(&end_time, NULL) == -1)
    {
        printf("not recv echo reply\n");
        perror("gettimeofday");
        return -1;
    }
   
    // Calculate round-trip time (RTT) in milliseconds
    double rtt = (double)(end_time.tv_sec - start_time->tv_sec) * 1000.0 +
                 (double)(end_time.tv_usec - start_time->tv_usec) / 1000.0 + delay();
   
    latency[count] = rtt;
   
    count++;
    

    while (1)
    {
       
        fd_set readfd;
        FD_ZERO(&readfd);
        FD_SET(sockfd, &readfd);
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        m = select(sockfd + 1, &readfd, NULL, NULL, &tv);
      
        if (m < 0)
        {
            printf("select error");
        }
        else if (m == 0)
        {
            printf("Packet not recieved - TIMEOUT OCCURED!\n ");
            return -1;
        }
        else
        {
            gettimeofday(&tv, NULL);
            long long end = tv.tv_sec * 1000 + tv.tv_usec / 1000;
            
            fromlen = sizeof(from);
            m = recvfrom(sockfd, recvpacket, sizeof(recvpacket), 0, (struct sockaddr *)&from, &fromlen);
            if (m < 0)
            {
                printf("recvfrom error\n");
                return -1;
            }
            
            struct ip *ip = (struct ip *)recvpacket;
            struct icmp *icmp = (struct icmp *)(recvpacket + (ip->ip_hl << 2));
            
            if (icmp->icmp_type == ICMP_TIMXCEED && icmp->icmp_code == ICMP_TIMXCEED_INTRANS)
            {
                printf("Packet dropped as Time to Live (TTL) Exceeded in Transit\n");
                printf("ICMP header of Recieved packet: \n");
                printICMP(icmp);
                printf("IP header of recieved packet: \n");
                print_ip_header(ip);
                
                return 1;
            }
            if (icmp->icmp_type == ICMP_ECHOREPLY  && icmp->icmp_seq == ttl)
            {
                printf("Echo Reply\n");
                printf("ICMP header of Recieved packet: \n");
                printICMP(icmp);
                printf("IP header of recieved packet: \n");
                print_ip_header(ip);
                return 0;
            }
           
        }
    }
   
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("Usage: %s <destination> <n> <T>\n", argv[0]);
        exit(1);
    }

    int n = atoi(argv[2]);
    int T = atoi(argv[3]);
    latency = (double *)malloc((2 * n + 10) * sizeof(double));
    bandwidth = (double *)malloc((2 * n + 10) * sizeof(double));
    int f = 0;
    char *dest = argv[1];
    struct hostent *hp;
    hp = gethostbyname(dest);
    if (hp == NULL)
    {
        printf("Unknown host %s\n", dest);
        exit(1);
    }
    bzero(&dest_addr, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    bcopy(hp->h_addr, &dest_addr.sin_addr, hp->h_length);
    pid = getpid();
    int ttl = 1, ret, ret2, ttl2 = 1;
    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0)
    {
        perror("socket error");
        exit(1);
    }
    setuid(getuid());
    printf("Tracing route to %s of ip address %s\n", dest, inet_ntoa(dest_addr.sin_addr));
    char *final_hop = inet_ntoa(dest_addr.sin_addr);
   
    char prev[INET_ADDRSTRLEN];
    char first[16];
    for (int j = 0; j < n; j++)
    {
        strcpy(prev,"hello");
        printf("-------------------Probe cycle %d------------------\n",j+1);
        while (ttl <= MAX_HOPS && ttl2 <= MAX_HOPS)
        {

            f = 0;
            
            printf("%d) ", ttl);
            fflush(stdout);

            
            struct timeval start_time;

            char *temp[10];
            for (int i = 0; i < 5; i++)
            {

               
                send_packet(ttl);

               
                ret = recv_packet(ttl, &start_time);

                temp[i] = inet_ntoa(from.sin_addr);
                
                if (i > 0)
                {
                    if (temp[i] != temp[i - 1])
                    {
                        exit(-1);
                    }
                }
                sleep(1); //wait for 1 sec
            }
            if (f == 1)
                break;
                
            if (strcmp(prev, inet_ntoa(dest_addr.sin_addr)) == 0)
            {
                printf("destination reached!\n");
                f = 1;
                count = 0;
                break;
            }
            strcpy(prev, inet_ntoa(from.sin_addr));
            const char* result = inet_ntoa(from.sin_addr);
            if (result != NULL) {
                strncpy(prev, result, INET_ADDRSTRLEN - 1);
                prev[INET_ADDRSTRLEN - 1] = '\0'; // Ensure null termination
            } else {
                // Handle error, inet_ntoa returned NULL
                printf("Error: Failed to convert IP address to string\n");
            }
            
            if(ret == -1)
            {
                printf("Current intermediate node address: %s\n",inet_ntoa(from.sin_addr));
                printf("\nContinue from previous hop and go to next hop due to timeout on recieving reply!\n\n");
                count =0;
                ttl++;
                ttl2++;
                continue;
            }
            else
            {
                printf("Successfully ensured the IP address of the next hop, going to the next hop!!!\n");
            }
           
            count = 0;
           

            for (int i = 0; i < n; i++)
            {

                
                if (gettimeofday(&start_time, NULL) == -1)
                {
                    printf("start time error\n");
                    perror("gettimeofday");
                    return 1;
                }

                
                send_packet(ttl);

                
                ret = recv_packet(ttl, &start_time);
                sleep(T);


                if (gettimeofday(&start_time, NULL) == -1)
                {
                    printf("start time error\n");
                    perror("gettimeofday");
                    return 1;
                }

                send_packet_data(ttl2);

                ret2 = recv_packet(ttl2, &start_time);

                sleep(T);
            }

            
            if(ret!=-1)
            {
                    printf("Current Intermediate node address: %s\n", inet_ntoa(from.sin_addr));
                    
                    double avg_latency = 0.0;

                  
                    double avg_bandwidth = 0.0;
                   
                    for (int i = 0; i < 2 * n; i += 2)
                    {
                       
                        avg_latency += latency[i];
                       
                    }

                   
                    for (int i = 0; i < 2 * n; i += 2)
                    {
                       
                        double tem = (abs(latency[i] - latency[i + 1]));
                        double tem1 = sizeof(data);
                        double tem2 = tem1 / tem;
                        avg_bandwidth += tem2;
                       
                    }
                   

                   
                    avg_latency /= n;
                   
                    avg_bandwidth /= n;
                    

                    printf("Latency: %.3f ms\n", avg_latency);
                
                    printf("Bandwidth: %.3f Mbps", avg_bandwidth);
                
            }
            
            count = 0;
            
            printf("\n\n");

            ttl++;
            ttl2++;
        }
        
        // if(ttl == MAX_HOPS && ttl2 == MAX_HOPS)
        //     printf("Max hops exceeded\n");
        
        ttl = 1;
        ttl2 = 1;
        count =0;
        f=0;
        sleep(T);
    }    
    return 0;
}
