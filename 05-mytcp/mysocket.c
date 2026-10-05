// MyTCP: message-oriented sockets built on top of TCP.
//
// Wire format: every message is a 4-byte length (network byte order)
// followed by the message bytes. TCP may split or merge these on the way;
// thread R reads exactly the length and then exactly that many bytes, so the
// original message boundaries are always recovered.
//
// Thread S takes messages from Send_Message and writes them to the TCP
// socket, using send calls of at most 1000 bytes. Thread R reads messages
// from the TCP socket and stores them in Received_Message. my_send and
// my_recv only touch the tables. One mutex protects all shared state and a
// single condition variable is broadcast whenever that state changes.

#include "mysocket.h"

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define MAX_SEND_CHUNK 1000  // most bytes written by one send() call

// Linux: stop send() raising SIGPIPE when the peer has gone away.
// macOS has no MSG_NOSIGNAL and uses the SO_NOSIGPIPE socket option instead.
#ifdef MSG_NOSIGNAL
#define SEND_FLAGS MSG_NOSIGNAL
#else
#define SEND_FLAGS 0
#endif

typedef struct
{
    char data[MYTCP_MAX_MSG_LEN];
    size_t len;
} message;

// Circular FIFO holding at most MYTCP_TABLE_SIZE messages
typedef struct
{
    message entries[MYTCP_TABLE_SIZE];
    int head;   // index of the oldest message
    int count;  // number of messages stored
} mssg_table;

static mssg_table *send_table;  // Send_Message
static mssg_table *recv_table;  // Received_Message

static int sock_fd = -1;        // socket returned by my_socket
static int conn_fd = -1;        // connected socket R and S work on, -1 if none
static int conn_gen = 0;        // bumped whenever conn_fd changes
static int peer_closed = 0;     // R saw end of stream or an error on conn_fd
static int send_failed = 0;     // S could not write to conn_fd
static int r_busy = 0;          // R is inside recv() on conn_fd
static int s_busy = 0;          // S is inside send() on conn_fd
static int closing = 0;         // my_close is waiting for Send_Message to drain
static int shutting_down = 0;   // my_close is stopping the threads

static pthread_t R, S;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t state_changed = PTHREAD_COND_INITIALIZER;

static void table_push(mssg_table *t, const void *buf, size_t len)
{
    message *m = &t->entries[(t->head + t->count) % MYTCP_TABLE_SIZE];
    memcpy(m->data, buf, len);
    m->len = len;
    t->count++;
}

static message *table_front(mssg_table *t)
{
    return &t->entries[t->head];
}

static void table_pop(mssg_table *t)
{
    t->head = (t->head + 1) % MYTCP_TABLE_SIZE;
    t->count--;
}

static void table_clear(mssg_table *t)
{
    t->head = 0;
    t->count = 0;
}

static void disable_sigpipe(int fd)
{
#ifdef SO_NOSIGPIPE
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#else
    (void)fd;
#endif
}

// Reads exactly len bytes. Returns 1 on success, 0 on end of stream or error.
static int recv_full(int fd, void *buf, size_t len)
{
    size_t got = 0;
    while (got < len)
    {
        ssize_t n = recv(fd, (char *)buf + got, len - got, 0);
        if (n > 0)
            got += n;
        else if (n < 0 && errno == EINTR)
            continue;
        else
            return 0;
    }
    return 1;
}

// Writes len bytes in send calls of at most MAX_SEND_CHUNK bytes.
// Returns 1 on success, 0 on error.
static int send_full(int fd, const void *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len)
    {
        size_t chunk = len - sent;
        if (chunk > MAX_SEND_CHUNK)
            chunk = MAX_SEND_CHUNK;
        ssize_t n = send(fd, (const char *)buf + sent, chunk, SEND_FLAGS);
        if (n > 0)
            sent += n;
        else if (n < 0 && errno == EINTR)
            continue;
        else
            return 0;
    }
    return 1;
}

// Thread R: reads whole messages from the TCP connection into Received_Message
static void *recv_thread(void *arg)
{
    static message m;  // only R uses it
    (void)arg;

    pthread_mutex_lock(&lock);
    for (;;)
    {
        while (!shutting_down && (conn_fd < 0 || peer_closed))
            pthread_cond_wait(&state_changed, &lock);
        if (shutting_down)
            break;

        int fd = conn_fd, gen = conn_gen;
        r_busy = 1;
        pthread_mutex_unlock(&lock);

        uint32_t netlen;
        int ok = recv_full(fd, &netlen, sizeof(netlen));
        if (ok)
        {
            m.len = ntohl(netlen);
            ok = m.len <= MYTCP_MAX_MSG_LEN && recv_full(fd, m.data, m.len);
        }

        pthread_mutex_lock(&lock);
        r_busy = 0;
        pthread_cond_broadcast(&state_changed);
        if (gen != conn_gen)  // connection was closed by my_close meanwhile
            continue;
        if (!ok)
        {
            peer_closed = 1;
            continue;
        }

        // While my_close drains, nobody will read Received_Message any more,
        // so incoming messages are dropped instead of blocking R.
        while (!shutting_down && !closing && gen == conn_gen &&
               recv_table->count == MYTCP_TABLE_SIZE)
            pthread_cond_wait(&state_changed, &lock);
        if (!shutting_down && !closing && gen == conn_gen)
        {
            table_push(recv_table, m.data, m.len);
            pthread_cond_broadcast(&state_changed);
        }
    }
    pthread_mutex_unlock(&lock);
    return NULL;
}

// Thread S: writes messages from Send_Message to the TCP connection
static void *send_thread(void *arg)
{
    static message m;  // only S uses it
    (void)arg;

    pthread_mutex_lock(&lock);
    for (;;)
    {
        while (!shutting_down && (conn_fd < 0 || send_failed || send_table->count == 0))
            pthread_cond_wait(&state_changed, &lock);
        if (shutting_down)
            break;

        int fd = conn_fd, gen = conn_gen;
        m = *table_front(send_table);
        s_busy = 1;
        pthread_mutex_unlock(&lock);

        uint32_t netlen = htonl((uint32_t)m.len);
        int ok = send_full(fd, &netlen, sizeof(netlen)) && send_full(fd, m.data, m.len);

        pthread_mutex_lock(&lock);
        s_busy = 0;
        if (gen == conn_gen)
        {
            // The entry is removed only once it has been sent, so my_close
            // can wait for the table to drain.
            if (ok)
                table_pop(send_table);
            else
            {
                send_failed = 1;
                table_clear(send_table);
            }
        }
        pthread_cond_broadcast(&state_changed);
    }
    pthread_mutex_unlock(&lock);
    return NULL;
}

// Makes fd the connection R and S work on. Called with lock held.
static void set_connection(int fd)
{
    disable_sigpipe(fd);
    table_clear(send_table);
    table_clear(recv_table);
    conn_fd = fd;
    conn_gen++;
    peer_closed = 0;
    send_failed = 0;
    pthread_cond_broadcast(&state_changed);
}

// Detaches R and S from conn_fd and waits until neither is using it.
// The caller closes the descriptor. Called with lock held.
static void drop_connection(void)
{
    int fd = conn_fd;
    conn_fd = -1;
    conn_gen++;
    table_clear(send_table);
    table_clear(recv_table);
    shutdown(fd, SHUT_RDWR);  // wakes R or S if blocked in recv/send
    pthread_cond_broadcast(&state_changed);
    while (r_busy || s_busy)
        pthread_cond_wait(&state_changed, &lock);
}

int my_socket(int domain, int type, int protocol)
{
    if (type != SOCK_MyTCP)
    {
        errno = EPROTOTYPE;
        return -1;
    }

    pthread_mutex_lock(&lock);
    if (sock_fd >= 0)  // only one MyTCP socket per program
    {
        pthread_mutex_unlock(&lock);
        errno = EMFILE;
        return -1;
    }

    int fd = socket(domain, SOCK_STREAM, protocol);
    if (fd < 0)
    {
        pthread_mutex_unlock(&lock);
        return -1;
    }

    send_table = calloc(1, sizeof(mssg_table));
    recv_table = calloc(1, sizeof(mssg_table));
    if (send_table == NULL || recv_table == NULL)
    {
        free(send_table);
        free(recv_table);
        close(fd);
        pthread_mutex_unlock(&lock);
        errno = ENOMEM;
        return -1;
    }

    disable_sigpipe(fd);
    sock_fd = fd;
    conn_fd = -1;
    peer_closed = send_failed = r_busy = s_busy = shutting_down = 0;

    pthread_create(&R, NULL, recv_thread, NULL);
    pthread_create(&S, NULL, send_thread, NULL);

    pthread_mutex_unlock(&lock);
    return fd;
}

int my_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    return bind(sockfd, addr, addrlen);
}

int my_listen(int sockfd, int backlog)
{
    return listen(sockfd, backlog);
}

int my_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    pthread_mutex_lock(&lock);
    int ok = sockfd == sock_fd && conn_fd < 0;
    pthread_mutex_unlock(&lock);
    if (!ok)  // not our socket, or the previous connection is still open
    {
        errno = EINVAL;
        return -1;
    }

    int newfd = accept(sockfd, addr, addrlen);
    if (newfd < 0)
        return -1;

    pthread_mutex_lock(&lock);
    set_connection(newfd);
    pthread_mutex_unlock(&lock);
    return newfd;
}

int my_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    pthread_mutex_lock(&lock);
    int ok = sockfd == sock_fd && conn_fd < 0;
    pthread_mutex_unlock(&lock);
    if (!ok)
    {
        errno = EINVAL;
        return -1;
    }

    if (connect(sockfd, addr, addrlen) < 0)
        return -1;

    pthread_mutex_lock(&lock);
    set_connection(sockfd);
    pthread_mutex_unlock(&lock);
    return 0;
}

ssize_t my_send(int sockfd, const void *buf, size_t len, int flags)
{
    (void)flags;
    if (len > MYTCP_MAX_MSG_LEN)
    {
        errno = EMSGSIZE;
        return -1;
    }
    // Like send(), sending 0 bytes does nothing. Queuing an empty message
    // would make my_recv return 0, which already means end of stream.
    if (len == 0)
        return 0;

    pthread_mutex_lock(&lock);
    for (;;)
    {
        if (conn_fd < 0 || sockfd != conn_fd)
        {
            pthread_mutex_unlock(&lock);
            errno = ENOTCONN;
            return -1;
        }
        if (send_failed)
        {
            pthread_mutex_unlock(&lock);
            errno = EPIPE;
            return -1;
        }
        if (send_table->count < MYTCP_TABLE_SIZE)
            break;
        pthread_cond_wait(&state_changed, &lock);
    }

    table_push(send_table, buf, len);
    pthread_cond_broadcast(&state_changed);
    pthread_mutex_unlock(&lock);
    return len;
}

ssize_t my_recv(int sockfd, void *buf, size_t len, int flags)
{
    (void)flags;

    pthread_mutex_lock(&lock);
    for (;;)
    {
        if (conn_fd < 0 || sockfd != conn_fd)
        {
            pthread_mutex_unlock(&lock);
            errno = ENOTCONN;
            return -1;
        }
        if (recv_table->count > 0)
            break;
        if (peer_closed)  // orderly end of stream, like recv() returning 0
        {
            pthread_mutex_unlock(&lock);
            return 0;
        }
        pthread_cond_wait(&state_changed, &lock);
    }

    message *m = table_front(recv_table);
    size_t n = m->len < len ? m->len : len;  // extra bytes are discarded
    memcpy(buf, m->data, n);
    table_pop(recv_table);
    pthread_cond_broadcast(&state_changed);
    pthread_mutex_unlock(&lock);
    return n;
}

int my_close(int sockfd)
{
    pthread_mutex_lock(&lock);
    if (sock_fd < 0 || (sockfd != sock_fd && sockfd != conn_fd))
    {
        pthread_mutex_unlock(&lock);
        return close(sockfd);
    }

    if (conn_fd >= 0)
    {
        // Let S finish sending what the application has already queued
        int c = conn_fd;
        closing = 1;
        pthread_cond_broadcast(&state_changed);
        while (!send_failed && send_table->count > 0)
            pthread_cond_wait(&state_changed, &lock);
        drop_connection();
        closing = 0;
        if (c != sockfd)
            close(c);
    }

    if (sockfd != sock_fd)  // an accepted connection; keep listening
    {
        pthread_mutex_unlock(&lock);
        return close(sockfd);
    }

    // Closing the MyTCP socket itself: stop R and S and free the tables
    shutting_down = 1;
    pthread_cond_broadcast(&state_changed);
    pthread_mutex_unlock(&lock);

    pthread_join(R, NULL);
    pthread_join(S, NULL);

    pthread_mutex_lock(&lock);
    free(send_table);
    free(recv_table);
    send_table = recv_table = NULL;
    sock_fd = -1;
    shutting_down = 0;
    pthread_mutex_unlock(&lock);

    return close(sockfd);
}
