/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* shim.c - copy a PMIx client's socket traffic to the shell
 *
 * Loaded into a client with LD_PRELOAD when the pmix.wirecapture shell
 * option is set.  It interposes the calls a client makes on its
 * server socket and writes what it sees to the channel named by
 * FLUX_PMIX_CAPTURE_FD.  It does not decode.
 *
 * Only connections to the PMIx server whose URI was found in the environment
 * are captured.  The client's other file descriptors are ignored.
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#include "src/common/libutil/getenv_prefix.h"

#include "shim.h"

static int capture_fd = -1;
static struct sockaddr_storage server_addr;
static socklen_t server_addrlen = 0;
static bool initialized = false;

/* The fds known to be PMIx server connections.  A client has very
 * few, normally one, so a static array suffices.
 */
#define MAX_CONN 16
static int conn_fd[MAX_CONN];
static int conn_count = 0;

/* The interposed calls run on whatever threads the client uses, and
 * openpmix has a progress thread of its own.  One lock covers both
 * the fd set and the writes, so that a record and its payload reach
 * the channel together.
 */
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static ssize_t (*real_read) (int, void *, size_t);
static ssize_t (*real_write) (int, const void *, size_t);
static ssize_t (*real_readv) (int, const struct iovec *, int);
static ssize_t (*real_writev) (int, const struct iovec *, int);
static ssize_t (*real_recv) (int, void *, size_t, int);
static ssize_t (*real_send) (int, const void *, size_t, int);
static int (*real_connect) (int, const struct sockaddr *, socklen_t);
static int (*real_close) (int);

/* The server's address comes from the PMIX_SERVER_URI* variables the
 * shell sets, which look like "pmix-server.0;tcp4://127.0.0.1:PORT"
 * or "pmix-server.0;tcp6://[::1]:PORT".  They all name the same
 * listener so the first one found is used, even though the client
 * might use any of them.
 * Returns -1 if none is set or the value is not in that form.
 */
static int parse_server_addr (struct sockaddr_storage *ss, socklen_t *len)
{
    const char *uri = getenv_prefix ("PMIX_SERVER_URI");
    const char *host;
    const char *colon;
    char hostbuf[INET6_ADDRSTRLEN];
    char *endptr;
    size_t hostlen;
    long port;

    if (!uri || !(host = strstr (uri, "://")))
        return -1;
    host += 3;
    if (!(colon = strrchr (host, ':')))
        return -1;
    port = strtol (colon + 1, &endptr, 10);
    if (colon[1] == '\0' || *endptr != '\0' || port <= 0 || port > 65535)
        return -1;
    hostlen = colon - host;
    if (hostlen >= 2 && host[0] == '[' && host[hostlen - 1] == ']') {
        host++;
        hostlen -= 2;
    }
    if (hostlen == 0 || hostlen >= sizeof (hostbuf))
        return -1;
    memcpy (hostbuf, host, hostlen);
    hostbuf[hostlen] = '\0';

    memset (ss, 0, sizeof (*ss));
    struct sockaddr_in *sin = (struct sockaddr_in *)ss;
    struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)ss;
    if (inet_pton (AF_INET, hostbuf, &sin->sin_addr) == 1) {
        sin->sin_family = AF_INET;
        sin->sin_port = htons ((uint16_t)port);
        *len = sizeof (*sin);
        return 0;
    }
    if (inet_pton (AF_INET6, hostbuf, &s6->sin6_addr) == 1) {
        s6->sin6_family = AF_INET6;
        s6->sin6_port = htons ((uint16_t)port);
        *len = sizeof (*s6);
        return 0;
    }
    return -1;
}

static void init (void)
{
    const char *s;

    real_read = dlsym (RTLD_NEXT, "read");
    real_write = dlsym (RTLD_NEXT, "write");
    real_readv = dlsym (RTLD_NEXT, "readv");
    real_writev = dlsym (RTLD_NEXT, "writev");
    real_recv = dlsym (RTLD_NEXT, "recv");
    real_send = dlsym (RTLD_NEXT, "send");
    real_connect = dlsym (RTLD_NEXT, "connect");
    real_close = dlsym (RTLD_NEXT, "close");

    if ((s = getenv (PMIX_CAPTURE_ENV))) {
        char *endptr;
        long fd = strtol (s, &endptr, 10);
        if (*s != '\0' && *endptr == '\0' && fd >= 0 && fd <= INT32_MAX)
            capture_fd = (int)fd;
    }
    if (parse_server_addr (&server_addr, &server_addrlen) < 0)
        server_addrlen = 0;
    initialized = true;
}

/* Every interposed call starts here.  The constructor below runs
 * first in the ordinary case, but a library initializer that runs
 * before ours could call read(), so this is checked rather than
 * assumed.
 */
static inline void ensure_init (void)
{
    if (!initialized)
        init ();
}

__attribute__ ((constructor))
static void shim_init (void)
{
    ensure_init ();
}

static uint64_t now_usec (void)
{
    struct timespec ts;

    if (clock_gettime (CLOCK_REALTIME, &ts) < 0)
        return 0;
    return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

/* Little-endian stores, so a capture means the same thing wherever
 * it is read.
 */
static void put_u32 (uint8_t *p, uint32_t v)
{
    p[0] = v & 0xff;
    p[1] = (v >> 8) & 0xff;
    p[2] = (v >> 16) & 0xff;
    p[3] = (v >> 24) & 0xff;
}

static void put_u64 (uint8_t *p, uint64_t v)
{
    put_u32 (p, (uint32_t)v);
    put_u32 (p + 4, (uint32_t)(v >> 32));
}

/* Write a vector in full, advancing past a short write.  Returns -1
 * if the channel is gone, which disables tracing rather than failing
 * the client's call: a capture is diagnostic, and losing it must not
 * change what the program does.
 */
static int writev_all (int fd, struct iovec *iov, int iovcnt)
{
    while (iovcnt > 0) {
        ssize_t n = real_writev (fd, iov, iovcnt);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            return -1;
        /* Drop the iovecs fully written and trim the one partly so. */
        while (iovcnt > 0 && (size_t)n >= iov[0].iov_len) {
            n -= iov[0].iov_len;
            iov++;
            iovcnt--;
        }
        if (iovcnt > 0) {
            iov[0].iov_base = (uint8_t *)iov[0].iov_base + n;
            iov[0].iov_len -= n;
        }
    }
    return 0;
}

/* Caller holds the lock.
 */
static void emit (enum pmix_capture_op op,
                  int fd,
                  const void *data,
                  size_t len)
{
    uint8_t hdr[PMIX_CAPTURE_HDR_SIZE];

    if (capture_fd < 0)
        return;
    memset (hdr, 0, sizeof (hdr));
    put_u32 (hdr, PMIX_CAPTURE_MAGIC);
    put_u32 (hdr + 4, (uint32_t)getpid ());
    put_u32 (hdr + 8, (uint32_t)fd);
    put_u32 (hdr + 12, (uint32_t)op);
    put_u64 (hdr + 16, now_usec ());
    put_u32 (hdr + 24, (uint32_t)len);

    /* Header and payload in one writev, so that a record reaches the
     * socket whole as often as the kernel allows.  The reader
     * reassembles a split record regardless, but a split costs it a
     * buffer copy, and two separate writes made one a near certainty
     * whenever the shell woke between them.
     */
    struct iovec iov[2] = {
        { .iov_base = hdr, .iov_len = sizeof (hdr) },
        { .iov_base = (void *)data, .iov_len = len },
    };
    if (writev_all (capture_fd, iov, len > 0 ? 2 : 1) < 0) {
        /* The shell is not reading, or the channel closed.  Stop
         * tracing; the client carries on unaffected.
         */
        capture_fd = -1;
    }
}

static bool is_captured (int fd)
{
    for (int i = 0; i < conn_count; i++) {
        if (conn_fd[i] == fd)
            return true;
    }
    return false;
}

static void add_conn (int fd)
{
    if (conn_count < MAX_CONN)
        conn_fd[conn_count++] = fd;
}

static void del_conn (int fd)
{
    for (int i = 0; i < conn_count; i++) {
        if (conn_fd[i] == fd) {
            conn_fd[i] = conn_fd[--conn_count];
            return;
        }
    }
}

/* A PMIx server connection is a TCP connection to the address and
 * port named by PMIX_SERVER_URI*, whatever that address is.
 */
static bool is_server (const struct sockaddr *addr, socklen_t len)
{
    if (!addr || server_addrlen == 0 || addr->sa_family != server_addr.ss_family)
        return false;
    if (addr->sa_family == AF_INET && len >= sizeof (struct sockaddr_in)) {
        const struct sockaddr_in *a = (const struct sockaddr_in *)addr;
        const struct sockaddr_in *b = (const struct sockaddr_in *)&server_addr;
        return a->sin_port == b->sin_port
            && a->sin_addr.s_addr == b->sin_addr.s_addr;
    }
    if (addr->sa_family == AF_INET6 && len >= sizeof (struct sockaddr_in6)) {
        const struct sockaddr_in6 *a = (const struct sockaddr_in6 *)addr;
        const struct sockaddr_in6 *b = (const struct sockaddr_in6 *)&server_addr;
        return a->sin6_port == b->sin6_port
            && IN6_ARE_ADDR_EQUAL (&a->sin6_addr, &b->sin6_addr);
    }
    return false;
}

int connect (int fd, const struct sockaddr *addr, socklen_t len)
{
    int rc;

    ensure_init ();
    rc = real_connect (fd, addr, len);
    /* A non-blocking connect returns EINPROGRESS and completes
     * later; it is still this fd's connection, so it is captured from
     * here either way.
     */
    if ((rc == 0 || errno == EINPROGRESS) && is_server (addr, len)) {
        pthread_mutex_lock (&lock);
        if (!is_captured (fd)) {
            add_conn (fd);
            emit (PMIX_CAPTURE_OP_OPEN, fd, NULL, 0);
        }
        pthread_mutex_unlock (&lock);
    }
    return rc;
}

int close (int fd)
{
    ensure_init ();
    pthread_mutex_lock (&lock);
    if (is_captured (fd)) {
        emit (PMIX_CAPTURE_OP_CLOSE, fd, NULL, 0);
        del_conn (fd);
    }
    pthread_mutex_unlock (&lock);
    return real_close (fd);
}

/* Record the bytes a call actually moved, after it returns: a short
 * read or write must be reported at its real length, or the shell
 * would frame the stream wrongly.
 */
static void record (enum pmix_capture_op op,
                    int fd,
                    const void *buf,
                    ssize_t n)
{
    if (n <= 0)
        return;
    pthread_mutex_lock (&lock);
    if (is_captured (fd))
        emit (op, fd, buf, (size_t)n);
    pthread_mutex_unlock (&lock);
}

ssize_t read (int fd, void *buf, size_t len)
{
    ssize_t n;

    ensure_init ();
    n = real_read (fd, buf, len);
    record (PMIX_CAPTURE_OP_READ, fd, buf, n);
    return n;
}

ssize_t write (int fd, const void *buf, size_t len)
{
    ssize_t n;

    ensure_init ();
    n = real_write (fd, buf, len);
    record (PMIX_CAPTURE_OP_WRITE, fd, buf, n);
    return n;
}

/* openpmix sends each message with writev(), the header and payload
 * in separate iovecs, so the scatter/gather calls matter as much as
 * the plain ones.  Record only the bytes the call actually moved: a
 * short writev must appear at its real length or the stream is
 * framed wrongly on the other side.
 */
static void record_iov (enum pmix_capture_op op,
                        int fd,
                        const struct iovec *iov,
                        int iovcnt,
                        ssize_t n)
{
    if (n <= 0)
        return;
    pthread_mutex_lock (&lock);
    if (is_captured (fd)) {
        size_t left = (size_t)n;
        for (int i = 0; i < iovcnt && left > 0; i++) {
            size_t take = iov[i].iov_len < left ? iov[i].iov_len : left;
            if (take > 0)
                emit (op, fd, iov[i].iov_base, take);
            left -= take;
        }
    }
    pthread_mutex_unlock (&lock);
}

ssize_t readv (int fd, const struct iovec *iov, int iovcnt)
{
    ssize_t n;

    ensure_init ();
    n = real_readv (fd, iov, iovcnt);
    record_iov (PMIX_CAPTURE_OP_READ, fd, iov, iovcnt, n);
    return n;
}

ssize_t writev (int fd, const struct iovec *iov, int iovcnt)
{
    ssize_t n;

    ensure_init ();
    n = real_writev (fd, iov, iovcnt);
    record_iov (PMIX_CAPTURE_OP_WRITE, fd, iov, iovcnt, n);
    return n;
}

ssize_t recv (int fd, void *buf, size_t len, int flags)
{
    ssize_t n;

    ensure_init ();
    n = real_recv (fd, buf, len, flags);
    /* MSG_PEEK leaves the bytes to be read again; recording them
     * would double them in the stream.
     */
    if (!(flags & MSG_PEEK))
        record (PMIX_CAPTURE_OP_READ, fd, buf, n);
    return n;
}

ssize_t send (int fd, const void *buf, size_t len, int flags)
{
    ssize_t n;

    ensure_init ();
    n = real_send (fd, buf, len, flags);
    record (PMIX_CAPTURE_OP_WRITE, fd, buf, n);
    return n;
}

// vi:ts=4 sw=4 expandtab
