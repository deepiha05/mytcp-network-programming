// Networks Assignment : 6
// 20CS10024 - Devendra Palod
// 20CS30015 - Deepiha S
//
// PingNetInfo: discovers the route to a host one link at a time and
// estimates the latency and bandwidth of every link, using ICMP over a raw
// socket.
//
// compile:  gcc pingnetinfo.c -o pingnetinfo
// run:      sudo ./pingnetinfo <site> <n> <T>
//             site  host name or IP address to probe
//             n     pings sent per data size for every node
//             T     seconds between two pings
//
// For each hop h (TTL = h):
//   1. Discover the node: send 5 echo requests with TTL h, 1 second apart.
//      The router at hop h answers with Time Exceeded (the destination
//      answers with Echo Reply). The node is the address that answered.
//   2. Ping that node directly, n times with no data and n times with
//      DATA_SIZE bytes of data, T seconds apart, keeping the minimum RTT
//      of each (the sample with the least queuing delay).
//   3. With X the previous node and Y this node:
//        latency(X-Y)   = (rtt0(Y) - rtt0(X)) / 2
//        bandwidth(X-Y) = 2 * DATA_SIZE * 8 / (extra(Y) - extra(X))
//      where extra(N) = rttData(N) - rtt0(N) is the time the extra data
//      adds on the whole path to N, and it is crossed twice (request and
//      reply both carry the data). The local host is X for the first link,
//      with rtt0 = extra = 0.
//
// Replies are matched to requests by ICMP identifier and sequence number,
// so late, duplicate, out-of-order and unrelated packets are recognised and
// skipped. Hops that do not answer are reported and skipped.
//
// The operating system adds the IP header to the ICMP packets we send.
// Build with -DSIMULATE_DELAY to add an artificial, size-dependent delay to
// every received packet, for testing on small networks (see delay()).

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#define MAX_HOPS        30
#define DISCOVER_PROBES 5     // probes per hop to identify the node
#define REPLY_TIMEOUT   1.0   // seconds to wait for each reply
#define DATA_SIZE       1024  // payload of the large ping, in bytes
#define ICMP_HDR_LEN    8
#define RECV_BUF        1500

int sockfd;
uint16_t ident;           // ICMP identifier of our packets (process id)
uint16_t next_seq = 1;    // ICMP sequence number of the next packet
struct sockaddr_in dest_addr;

// Result of one probe
typedef struct
{
    int answered;         // 1 if a matching reply arrived in time
    int type;             // ICMP type of the reply
    struct in_addr from;  // who sent the reply
    double rtt_ms;        // round-trip time in milliseconds
} probe_result;

double now_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

void sleep_ms(double ms)
{
    if (ms > 0)
        usleep((useconds_t)(ms * 1000));
}

#ifdef SIMULATE_DELAY
// On a small network every RTT is a fraction of a millisecond. For testing,
// delay each received packet by 0.5 ms plus 2 microseconds per byte, so the
// data size makes a measurable difference.
void delay(int bytes)
{
    sleep_ms(0.5 + bytes * 0.002);
}
#endif

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
    printf("|    type:%-2d    |    code:%-2d    |        checksum:%-6d        |\n", icmp->icmp_type, icmp->icmp_code, ntohs(icmp->icmp_cksum));
    printf("-----------------------------------------------------------------\n");
    if (icmp->icmp_type == ICMP_ECHO || icmp->icmp_type == ICMP_ECHOREPLY)
    {
        printf("|           id:%-6d           |        sequence:%-6d        |\n", ntohs(icmp->icmp_id), ntohs(icmp->icmp_seq));
        printf("-----------------------------------------------------------------\n");
    }
}

// function to print ip header
void print_ip_header(struct ip *ip)
{
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

void print_tcp_header(struct tcphdr *tcp)
{
    printf("   TCP  source port: %u  destination port: %u  sequence: %u\n",
           ntohs(tcp->th_sport), ntohs(tcp->th_dport), (unsigned)ntohl(tcp->th_seq));
}

void print_udp_header(struct udphdr *udp)
{
    printf("   UDP  source port: %u  destination port: %u  length: %u  checksum: %u\n",
           ntohs(udp->uh_sport), ntohs(udp->uh_dport), ntohs(udp->uh_ulen), ntohs(udp->uh_sum));
}

// For ICMP errors other than Time Exceeded: print the IP header and the
// transport header of the dropped packet carried in the data part.
void print_icmp_error_data(struct icmp *icmp, int icmp_len)
{
    if (icmp_len < ICMP_HDR_LEN + (int)sizeof(struct ip))
    {
        printf("   (no data in this ICMP packet)\n");
        return;
    }
    struct ip *inner = (struct ip *)((char *)icmp + ICMP_HDR_LEN);
    int inner_hl = inner->ip_hl << 2;
    char *payload = (char *)inner + inner_hl;
    int payload_len = icmp_len - ICMP_HDR_LEN - inner_hl;

    printf("IP header of the packet that was dropped:\n");
    print_ip_header(inner);
    // ICMP errors carry only the first 8 bytes after the IP header
    if (inner->ip_p == IPPROTO_UDP && payload_len >= 8)
        print_udp_header((struct udphdr *)payload);
    else if (inner->ip_p == IPPROTO_TCP && payload_len >= 8)
        print_tcp_header((struct tcphdr *)payload);
    else
        printf("   Unknown protocol (%u)\n", inner->ip_p);
}

// Sends an Echo Request with `datalen` bytes of data and the given TTL.
// Returns the sequence number used.
uint16_t send_echo(struct in_addr to, int ttl, int datalen)
{
    char packet[ICMP_HDR_LEN + DATA_SIZE];
    struct icmp *icmp = (struct icmp *)packet;
    struct sockaddr_in addr;
    uint16_t seq = next_seq++;

    memset(packet, 0, sizeof(packet));
    icmp->icmp_type = ICMP_ECHO;
    icmp->icmp_code = 0;
    icmp->icmp_id = htons(ident);
    icmp->icmp_seq = htons(seq);
    for (int i = 0; i < datalen; i++)
        packet[ICMP_HDR_LEN + i] = (char)('A' + i % 26);
    icmp->icmp_cksum = 0;
    icmp->icmp_cksum = cal_chksum((unsigned short *)packet, ICMP_HDR_LEN + datalen);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr = to;

    setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl));
    printf("ICMP header of sent packet (to %s, TTL %d, %d data bytes):\n", inet_ntoa(to), ttl, datalen);
    printICMP(icmp);
    if (sendto(sockfd, packet, ICMP_HDR_LEN + datalen, 0, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        perror("sendto");
    return seq;
}

// Waits up to REPLY_TIMEOUT seconds for the reply to our packet `seq`: an
// Echo Reply, or an ICMP error (Time Exceeded, Destination Unreachable, ...)
// quoting it. Every packet received meanwhile is printed; ones that do not
// match are skipped. `sent_at` is the send time from now_ms().
probe_result wait_reply(uint16_t seq, double sent_at)
{
    probe_result res = {0};
    char buf[RECV_BUF];
    double deadline = sent_at + REPLY_TIMEOUT * 1000;

    for (;;)
    {
        double left = deadline - now_ms();
        if (left <= 0)
            return res;

        fd_set readfd;
        FD_ZERO(&readfd);
        FD_SET(sockfd, &readfd);
        struct timeval tv = {(time_t)(left / 1000), (suseconds_t)(((long)left % 1000) * 1000)};
        int m = select(sockfd + 1, &readfd, NULL, NULL, &tv);
        if (m < 0)
        {
            if (errno == EINTR)
                continue;
            perror("select");
            return res;
        }
        if (m == 0)
            return res;

        struct sockaddr_in from;
        socklen_t fromlen = sizeof(from);
        int n = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&from, &fromlen);
        double arrived = now_ms();
        if (n < 0)
        {
            perror("recvfrom");
            continue;
        }
#ifdef SIMULATE_DELAY
        delay(n);
        arrived = now_ms();
#endif

        struct ip *ip = (struct ip *)buf;
        int hl = ip->ip_hl << 2;
        if (n < hl + ICMP_HDR_LEN)
            continue;
#ifdef __APPLE__
        // macOS raw sockets deliver ip_len and ip_off in host byte order,
        // with the header length subtracted from ip_len; restore them
        ip->ip_len = htons(ip->ip_len + hl);
        ip->ip_off = htons(ip->ip_off);
#endif
        struct icmp *icmp = (struct icmp *)(buf + hl);
        int icmp_len = n - hl;

        // our own Echo Requests show up when probing this host itself
        if (icmp->icmp_type == ICMP_ECHO)
            continue;

        printf("ICMP header of received packet (from %s):\n", inet_ntoa(from.sin_addr));
        printICMP(icmp);
        printf("IP header of received packet:\n");
        print_ip_header(ip);

        int match = 0;
        if (icmp->icmp_type == ICMP_ECHOREPLY)
        {
            match = ntohs(icmp->icmp_id) == ident && ntohs(icmp->icmp_seq) == seq;
        }
        else
        {
            if (icmp->icmp_type != ICMP_TIMXCEED)
                print_icmp_error_data(icmp, icmp_len);

            // An ICMP error quotes the IP header and first 8 bytes of the
            // packet that caused it, which for us is our Echo Request.
            if (icmp_len >= ICMP_HDR_LEN + (int)sizeof(struct ip))
            {
                struct ip *inner = (struct ip *)((char *)icmp + ICMP_HDR_LEN);
                int inner_hl = inner->ip_hl << 2;
                if (inner->ip_p == IPPROTO_ICMP && icmp_len >= ICMP_HDR_LEN + inner_hl + ICMP_HDR_LEN)
                {
                    struct icmp *orig = (struct icmp *)((char *)inner + inner_hl);
                    match = orig->icmp_type == ICMP_ECHO && ntohs(orig->icmp_id) == ident &&
                            ntohs(orig->icmp_seq) == seq;
                }
            }
        }

        if (!match)
        {
            printf("(not a reply to the current probe, skipped)\n");
            continue;
        }

        res.answered = 1;
        res.type = icmp->icmp_type;
        res.from = from.sin_addr;
        res.rtt_ms = arrived - sent_at;
        return res;
    }
}

// One probe: send an Echo Request and wait for its reply
probe_result probe(struct in_addr to, int ttl, int datalen)
{
    double sent_at = now_ms();
    uint16_t seq = send_echo(to, ttl, datalen);
    return wait_reply(seq, sent_at);
}

// Finds the node at the given hop. Returns 1 and fills node / reached
// (1 if the node is the destination) if any probe was answered.
int discover_hop(int ttl, struct in_addr *node, int *reached)
{
    struct in_addr addrs[DISCOVER_PROBES];
    int types[DISCOVER_PROBES], count = 0;

    for (int i = 0; i < DISCOVER_PROBES; i++)
    {
        double start = now_ms();
        probe_result r = probe(dest_addr.sin_addr, ttl, 0);
        if (r.answered)
        {
            addrs[count] = r.from;
            types[count] = r.type;
            count++;
        }
        else
            printf("Probe %d at hop %d: no reply (timeout)\n", i + 1, ttl);
        if (i < DISCOVER_PROBES - 1)
            sleep_ms(1000 - (now_ms() - start));  // probes 1 second apart
    }
    if (count == 0)
        return 0;

    // Routes can change or be load balanced, so take the address that
    // answered most often.
    int best = 0, best_votes = 0;
    for (int i = 0; i < count; i++)
    {
        int votes = 0;
        for (int j = 0; j < count; j++)
            votes += addrs[j].s_addr == addrs[i].s_addr;
        if (votes > best_votes)
        {
            best = i;
            best_votes = votes;
        }
    }
    if (best_votes < count)
        printf("Note: replies at hop %d came from different addresses (load-balanced route?)\n", ttl);

    *node = addrs[best];
    *reached = types[best] == ICMP_ECHOREPLY || addrs[best].s_addr == dest_addr.sin_addr.s_addr;
    return 1;
}

// Pings node n times with datalen bytes of data, T seconds apart.
// Returns the minimum RTT in ms, or -1 if no ping was answered.
double min_rtt(struct in_addr node, int datalen, int n, double T)
{
    double best = -1;
    for (int i = 0; i < n; i++)
    {
        double start = now_ms();
        probe_result r = probe(node, 64, datalen);
        if (r.answered && r.type == ICMP_ECHOREPLY && (best < 0 || r.rtt_ms < best))
            best = r.rtt_ms;
        if (i < n - 1)
            sleep_ms(T * 1000 - (now_ms() - start));
    }
    return best;
}

int main(int argc, char *argv[])
{
    if (argc < 4)
    {
        printf("Usage: %s <site> <n> <T>\n", argv[0]);
        exit(1);
    }

    int n = atoi(argv[2]);
    double T = atof(argv[3]);
    if (n < 1 || T < 0)
    {
        printf("n must be at least 1 and T must not be negative\n");
        exit(1);
    }

    struct hostent *hp = gethostbyname(argv[1]);
    if (hp == NULL)
    {
        printf("Unknown host %s\n", argv[1]);
        exit(1);
    }
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    memcpy(&dest_addr.sin_addr, hp->h_addr, hp->h_length);

    sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0)
    {
        perror("socket (raw sockets need root, try sudo)");
        exit(1);
    }
    if (setuid(getuid()) < 0)  // root is no longer needed once the socket exists
        perror("setuid");
    ident = getpid() & 0xFFFF;

    char dest_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &dest_addr.sin_addr, dest_str, sizeof(dest_str));
    printf("PingNetInfo to %s (%s): %d pings per size, %.1f s apart, data size %d bytes\n\n",
           argv[1], dest_str, n, T, DATA_SIZE);

    // previous node on the path; the local host to start with
    char prev_str[INET_ADDRSTRLEN] = "local host";
    int prev_known = 1;     // rtt0 and extra of the previous node are known
    double prev_rtt0 = 0, prev_extra = 0;

    for (int ttl = 1; ttl <= MAX_HOPS; ttl++)
    {
        printf("=================== Hop %d ===================\n", ttl);

        struct in_addr node;
        int reached = 0;
        if (!discover_hop(ttl, &node, &reached))
        {
            printf("\nHop %d: * no node replied. It cannot be identified, so the links on\n"
                   "either side of it cannot be measured. Moving on to hop %d.\n\n", ttl, ttl + 1);
            strcpy(prev_str, "unknown node");
            prev_known = 0;
            continue;
        }

        char node_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &node, node_str, sizeof(node_str));
        printf("\nHop %d node: %s\n", ttl, node_str);

        printf("Measuring latency: %d pings with no data\n", n);
        double rtt0 = min_rtt(node, 0, n, T);
        sleep_ms(T * 1000);
        printf("Measuring bandwidth: %d pings with %d bytes of data\n", n, DATA_SIZE);
        double rttd = min_rtt(node, DATA_SIZE, n, T);

        printf("\n*** Link %d: %s -> %s\n", ttl, prev_str, node_str);
        if (rtt0 < 0 || rttd < 0)
        {
            printf("    %s does not answer pings; this link cannot be measured.\n\n", node_str);
            prev_known = 0;
        }
        else
        {
            double extra = rttd - rtt0;
            printf("    RTT to %s: %.3f ms (no data), %.3f ms (%d bytes)\n", node_str, rtt0, rttd, DATA_SIZE);
            if (!prev_known)
                printf("    Previous node unknown; link latency and bandwidth cannot be separated.\n");
            else
            {
                double latency = (rtt0 - prev_rtt0) / 2;
                double link_time = extra - prev_extra;
                if (latency > 0)
                    printf("    Latency:   %.3f ms\n", latency);
                else
                    printf("    Latency:   too small to measure (below timing noise)\n");
                if (link_time > 0)
                    printf("    Bandwidth: %.3f Mbps\n", (2.0 * DATA_SIZE * 8) / (link_time / 1000) / 1e6);
                else
                    printf("    Bandwidth: too high to measure (below timing noise)\n");
            }
            printf("\n");
            prev_known = 1;
            prev_rtt0 = rtt0;
            prev_extra = extra;
        }
        strcpy(prev_str, node_str);

        if (reached)
        {
            printf("Destination %s reached in %d hops.\n", dest_str, ttl);
            return 0;
        }
        sleep_ms(T * 1000);
    }

    printf("Destination not reached within %d hops.\n", MAX_HOPS);
    return 0;
}
