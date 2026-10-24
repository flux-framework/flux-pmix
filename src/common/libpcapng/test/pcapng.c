/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* Exercise the writer's interface.  Whether Wireshark can read what
 * it writes is checked by t/t0010-capture.t with tshark, on captures
 * of real jobs.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "tap.h"
#include "src/common/libpcapng/pcapng.h"

static const uint8_t payload1[] = { 0x01, 0x02, 0x03, 0x04, 0x05 };  // odd
static const uint8_t payload2[] = { 0xaa, 0xbb, 0xcc, 0xdd };        // aligned

static void write_capture (const char *path)
{
    struct pcapng_writer *w;
    int if0;
    int if1;

    w = pcapng_writer_create (path, "test_pcapng");
    ok (w != NULL, "pcapng_writer_create works");

    if0 = pcapng_writer_interface (w, "conn0", "first connection");
    ok (if0 == 0, "first interface is 0");
    if1 = pcapng_writer_interface (w, "conn1", NULL);
    ok (if1 == 1, "second interface is 1");

    ok (pcapng_writer_packet (w, 0, 1000, PCAPNG_DIR_SENT, "pmix",
                              "hello", payload1, sizeof (payload1)) == 0,
        "write packet with odd payload length");
    ok (pcapng_writer_packet (w, 1, 2000, PCAPNG_DIR_RECV, "pmix",
                              NULL, payload2, sizeof (payload2)) == 0,
        "write packet with aligned payload and no info");
    ok (pcapng_writer_packet (w, 0, 0x123456789ULL, PCAPNG_DIR_UNKNOWN,
                              NULL, "open", NULL, 0) == 0,
        "write empty packet with no dissector or direction");

    errno = 0;
    ok (pcapng_writer_packet (w, 2, 0, PCAPNG_DIR_SENT, NULL, NULL,
                              payload1, 1) < 0 && errno == EINVAL,
        "write to an interface that was not added fails with EINVAL");
    errno = 0;
    ok (pcapng_writer_packet (w, 0, 0, PCAPNG_DIR_SENT, NULL, NULL,
                              NULL, 1) < 0 && errno == EINVAL,
        "write with NULL data and nonzero len fails with EINVAL");

    ok (pcapng_writer_destroy (w) == 0, "pcapng_writer_destroy works");
}

static void write_bad_args (void)
{
    errno = 0;
    ok (pcapng_writer_create (NULL, NULL) == NULL && errno == EINVAL,
        "pcapng_writer_create path=NULL fails with EINVAL");
    errno = 0;
    ok (pcapng_writer_fcreate (NULL, NULL) == NULL && errno == EINVAL,
        "pcapng_writer_fcreate f=NULL fails with EINVAL");
    errno = 0;
    ok (pcapng_writer_create ("/nonexistent/dir/x", NULL) == NULL
        && errno == ENOENT,
        "pcapng_writer_create on an unwritable path fails with ENOENT");
    errno = 0;
    ok (pcapng_writer_interface (NULL, NULL, NULL) < 0 && errno == EINVAL,
        "pcapng_writer_interface w=NULL fails with EINVAL");
    ok (pcapng_writer_destroy (NULL) == 0, "pcapng_writer_destroy NULL is ok");
}

static bool files_identical (const char *path1, const char *path2)
{
    FILE *f1 = fopen (path1, "r");
    FILE *f2 = fopen (path2, "r");
    bool same = false;
    int c1;
    int c2;

    if (!f1 || !f2)
        BAIL_OUT ("cannot open file for comparison");
    do {
        c1 = fgetc (f1);
        c2 = fgetc (f2);
    } while (c1 == c2 && c1 != EOF);
    same = (c1 == c2);
    fclose (f1);
    fclose (f2);
    return same;
}

/* A sorted writer given packets out of order must produce the same
 * file as an unsorted writer given them in order, with ties in the
 * order they were given.
 */
static void check_sorted (const char *path1, const char *path2)
{
    struct pcapng_writer *w;
    const uint8_t pkt[] = { 'a', 'b', 'c', 'd', 'e' };
    uint64_t times[] = { 1, 2, 3, 3, 3 };
    const int scrambled[] = { 2, 0, 3, 1, 4 };

    w = pcapng_writer_create (path1, NULL);
    ok (w != NULL, "unsorted writer created");
    ok (pcapng_writer_interface (w, NULL, NULL) == 0, "interface added");
    for (int i = 0; i < 5; i++) {
        if (pcapng_writer_packet (w, 0, times[i], PCAPNG_DIR_SENT,
                                  NULL, NULL, &pkt[i], 1) < 0)
            BAIL_OUT ("write failed");
    }
    ok (pcapng_writer_destroy (w) == 0, "unsorted writer destroyed");

    w = pcapng_writer_create (path2, NULL);
    ok (w != NULL, "sorted writer created");
    pcapng_writer_set_sorted (w, true);
    ok (pcapng_writer_interface (w, NULL, NULL) == 0, "interface added");
    for (int i = 0; i < 5; i++) {
        int j = scrambled[i];
        if (pcapng_writer_packet (w, 0, times[j], PCAPNG_DIR_SENT,
                                  NULL, NULL, &pkt[j], 1) < 0)
            BAIL_OUT ("write failed");
    }
    ok (pcapng_writer_destroy (w) == 0, "sorted writer destroyed");

    ok (files_identical (path1, path2),
        "sorted writer output matches packets written in time order");
}

int main (int argc, char *argv[])
{
    char path1[] = "/tmp/test_pcapng.XXXXXX";
    char path2[] = "/tmp/test_pcapng.XXXXXX";
    int fd;

    plan (NO_PLAN);

    if ((fd = mkstemp (path1)) < 0 || close (fd) < 0
        || (fd = mkstemp (path2)) < 0 || close (fd) < 0)
        BAIL_OUT ("mkstemp failed");

    write_capture (path1);
    write_bad_args ();
    check_sorted (path1, path2);

    unlink (path1);
    unlink (path2);
    done_testing ();
}

// vi:ts=4 sw=4 expandtab
