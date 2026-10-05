// MyTCP test client, run against the echo server.
//
// usage: ./client [server-ip] [port]
//
// Checks that
//   1. bursts of messages of random sizes (1 to 5000 bytes) come back as the
//      same messages: same count, same lengths, same bytes, same order, even
//      though TCP merges and splits them on the way
//   2. my_recv with a buffer smaller than the message returns only the first
//      part and discards the rest, without disturbing the next message
//   3. messages longer than MYTCP_MAX_MSG_LEN are rejected, and sending
//      0 bytes is a no-op
// Exits with status 0 if every check passes.

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "mysocket.h"

#define ROUNDS 50
#define BURST  8   // messages sent back to back before reading the echoes

static int failures = 0;

static void check(int ok, const char *what)
{
    if (!ok)
    {
        printf("FAIL: %s\n", what);
        failures++;
    }
}

static void fill(char *buf, size_t len, unsigned seed)
{
    for (size_t i = 0; i < len; i++)
        buf[i] = (char)(seed * 31 + i * 7);
}

int main(int argc, char *argv[])
{
    const char *ip = argc > 1 ? argv[1] : "127.0.0.1";
    int port = argc > 2 ? atoi(argv[2]) : 20000;

    static char sent[BURST][MYTCP_MAX_MSG_LEN];
    static char got[MYTCP_MAX_MSG_LEN + 1];
    size_t lens[BURST];

    int sockfd = my_socket(AF_INET, SOCK_MyTCP, 0);
    if (sockfd < 0)
    {
        perror("my_socket");
        exit(1);
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    inet_aton(ip, &serv_addr.sin_addr);
    serv_addr.sin_port = htons(port);

    if (my_connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("my_connect");
        exit(1);
    }

    // 1. Message boundaries survive bursts of random-size messages
    srand(1964);
    int total = 0;
    for (int round = 0; round < ROUNDS; round++)
    {
        for (int i = 0; i < BURST; i++)
        {
            // include the edge cases 1, 999, 1000, 1001 and 5000 bytes
            static const size_t edge[] = {1, 999, 1000, 1001, MYTCP_MAX_MSG_LEN};
            lens[i] = round < 5 && i == 0 ? edge[round] : (size_t)(1 + rand() % MYTCP_MAX_MSG_LEN);
            fill(sent[i], lens[i], round * BURST + i);
            check(my_send(sockfd, sent[i], lens[i], 0) == (ssize_t)lens[i], "my_send returns length");
        }
        for (int i = 0; i < BURST; i++)
        {
            ssize_t n = my_recv(sockfd, got, sizeof(got), 0);
            check(n == (ssize_t)lens[i], "echoed message has the same length");
            check(n >= 0 && memcmp(got, sent[i], lens[i]) == 0, "echoed message has the same bytes");
            total++;
        }
    }
    printf("boundaries: %d messages echoed intact\n", total);

    // 2. A short receive buffer truncates one message only
    fill(sent[0], 100, 1);
    fill(sent[1], 50, 2);
    my_send(sockfd, sent[0], 100, 0);
    my_send(sockfd, sent[1], 50, 0);
    check(my_recv(sockfd, got, 10, 0) == 10, "truncated my_recv returns buffer length");
    check(memcmp(got, sent[0], 10) == 0, "truncated my_recv returns start of message");
    check(my_recv(sockfd, got, sizeof(got), 0) == 50, "next message is intact after truncation");
    check(memcmp(got, sent[1], 50) == 0, "next message bytes are intact after truncation");
    printf("truncation: checked\n");

    // 3. Oversized messages are rejected; empty sends do nothing
    errno = 0;
    check(my_send(sockfd, got, MYTCP_MAX_MSG_LEN + 1, 0) == -1 && errno == EMSGSIZE,
          "message over the maximum length is rejected with EMSGSIZE");
    check(my_send(sockfd, got, 0, 0) == 0, "sending 0 bytes returns 0");
    printf("lengths: checked\n");

    my_close(sockfd);

    if (failures)
    {
        printf("%d check(s) FAILED\n", failures);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
