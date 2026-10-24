/************************************************************\
 * Copyright 2026 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* pcapng.c - write pcapng capture files
 *
 * pcapng allows a file in either byte order and marks the order in
 * the section header, so the file is written in the host's order as
 * Wireshark and tcpdump do, and any reader of the format handles it.
 * Exported-PDU tags inside a packet are always big-endian, since that
 * is a separate format with its own rule.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "pcapng.h"

/* Block types
 */
#define BLOCK_SHB   0x0A0D0D0A
#define BLOCK_IDB   0x00000001
#define BLOCK_EPB   0x00000006

#define BYTE_ORDER_MAGIC 0x1A2B3C4D

/* Options, numbered per block type
 */
#define OPT_END         0
#define OPT_COMMENT     1
#define SHB_USERAPPL    4
#define IDB_NAME        2
#define IDB_DESCRIPTION 3
#define EPB_FLAGS       2

#define LINKTYPE_WIRESHARK_UPPER_PDU 252

/* Exported-PDU tags (Wireshark's wsutil/exported_pdu_tlvs.h)
 */
#define TAG_END             0
#define TAG_DISSECTOR_NAME  12
#define TAG_P2P_DIRECTION   35
#define TAG_COL_INFO_TEXT   36

#define PAD4(n) (((n) + 3) & ~(size_t)3)

/* A packet held back for sorting: its finished block and the key.
 */
struct held {
    uint8_t *block;
    size_t len;
    uint64_t usec;
    uint64_t seq;               // order written, to keep ties stable
};

struct pcapng_writer {
    FILE *f;
    bool owned;
    int nifaces;
    bool sorted;
    struct held *held;
    size_t nheld;
    size_t heldsize;
};

static void put_u16 (uint8_t *p, uint16_t v)
{
    memcpy (p, &v, 2);
}

static void put_u32 (uint8_t *p, uint32_t v)
{
    memcpy (p, &v, 4);
}

static void put_be16 (uint8_t *p, uint16_t v)
{
    p[0] = v >> 8;
    p[1] = v;
}

static void put_be32 (uint8_t *p, uint32_t v)
{
    p[0] = v >> 24;
    p[1] = v >> 16;
    p[2] = v >> 8;
    p[3] = v;
}

/* A block is assembled in memory and written whole, so that a
 * failure leaves the file at a block boundary.
 */
struct block {
    uint8_t *buf;
    size_t len;
    size_t size;
};

static int block_grow (struct block *b, size_t need)
{
    if (b->len + need > b->size) {
        size_t size = b->size ? b->size : 256;
        while (size < b->len + need)
            size *= 2;
        uint8_t *buf = realloc (b->buf, size);
        if (!buf)
            return -1;
        b->buf = buf;
        b->size = size;
    }
    return 0;
}

static int block_append (struct block *b, const void *data, size_t len)
{
    if (block_grow (b, PAD4 (len)) < 0)
        return -1;
    if (len > 0)
        memcpy (b->buf + b->len, data, len);
    memset (b->buf + b->len + len, 0, PAD4 (len) - len);
    b->len += PAD4 (len);
    return 0;
}

static int block_u16 (struct block *b, uint16_t v)
{
    if (block_grow (b, 2) < 0)
        return -1;
    put_u16 (b->buf + b->len, v);
    b->len += 2;
    return 0;
}

static int block_u32 (struct block *b, uint32_t v)
{
    if (block_grow (b, 4) < 0)
        return -1;
    put_u32 (b->buf + b->len, v);
    b->len += 4;
    return 0;
}

/* Block type and a placeholder for the length, which block_finish()
 * fills in.
 */
static int block_start (struct block *b, uint32_t type)
{
    memset (b, 0, sizeof (*b));
    return block_u32 (b, type) < 0 || block_u32 (b, 0) < 0 ? -1 : 0;
}

/* An option is a 16-bit code and length, then the value padded to a
 * multiple of 4.  The length is of the value alone.
 */
static int block_option (struct block *b,
                         uint16_t code,
                         const void *val,
                         size_t len)
{
    if (len > UINT16_MAX) {
        errno = EINVAL;
        return -1;
    }
    if (block_u16 (b, code) < 0
        || block_u16 (b, len) < 0
        || block_append (b, val, len) < 0)
        return -1;
    return 0;
}

static int block_option_string (struct block *b, uint16_t code, const char *s)
{
    return s ? block_option (b, code, s, strlen (s)) : 0;
}

/* Options are closed with an end option, and the total length is
 * stored at the front and the back.
 */
static int block_close (struct block *b)
{
    if (block_u16 (b, OPT_END) < 0 || block_u16 (b, 0) < 0)
        return -1;
    if (block_u32 (b, b->len + 4) < 0)
        return -1;
    put_u32 (b->buf + 4, b->len);
    return 0;
}

static int block_finish (struct block *b, FILE *f)
{
    int rc = -1;

    if (block_close (b) < 0)
        goto done;
    if (fwrite (b->buf, b->len, 1, f) != 1) {
        errno = EIO;
        goto done;
    }
    rc = 0;
done:
    free (b->buf);
    return rc;
}

/* Keep a closed block to write at the end, taking its buffer.
 */
static int block_hold (struct block *b, struct pcapng_writer *w, uint64_t usec)
{
    if (block_close (b) < 0) {
        free (b->buf);
        return -1;
    }
    if (w->nheld == w->heldsize) {
        size_t size = w->heldsize ? w->heldsize * 2 : 64;
        struct held *held = realloc (w->held, size * sizeof (*held));
        if (!held) {
            free (b->buf);
            return -1;
        }
        w->held = held;
        w->heldsize = size;
    }
    w->held[w->nheld].block = b->buf;
    w->held[w->nheld].len = b->len;
    w->held[w->nheld].usec = usec;
    w->held[w->nheld].seq = w->nheld;
    w->nheld++;
    return 0;
}

static int held_cmp (const void *a, const void *b)
{
    const struct held *x = a;
    const struct held *y = b;

    if (x->usec != y->usec)
        return x->usec < y->usec ? -1 : 1;
    return x->seq < y->seq ? -1 : x->seq > y->seq ? 1 : 0;
}

static int held_flush (struct pcapng_writer *w)
{
    int rc = 0;

    qsort (w->held, w->nheld, sizeof (w->held[0]), held_cmp);
    for (size_t i = 0; i < w->nheld; i++) {
        if (rc == 0 && fwrite (w->held[i].block, w->held[i].len, 1, w->f) != 1) {
            errno = EIO;
            rc = -1;
        }
        free (w->held[i].block);
    }
    free (w->held);
    w->held = NULL;
    w->nheld = w->heldsize = 0;
    return rc;
}

static int write_shb (FILE *f, const char *application)
{
    struct block b;
    uint64_t unspecified = UINT64_MAX;  // section length -1: not known

    if (block_start (&b, BLOCK_SHB) < 0
        || block_u32 (&b, BYTE_ORDER_MAGIC) < 0
        || block_u16 (&b, 1) < 0            // major version
        || block_u16 (&b, 0) < 0            // minor version
        || block_append (&b, &unspecified, 8) < 0
        || block_option_string (&b, SHB_USERAPPL, application) < 0) {
        free (b.buf);
        return -1;
    }
    return block_finish (&b, f);
}

struct pcapng_writer *pcapng_writer_fcreate (FILE *f, const char *application)
{
    struct pcapng_writer *w;

    if (!f) {
        errno = EINVAL;
        return NULL;
    }
    if (!(w = calloc (1, sizeof (*w))))
        return NULL;
    w->f = f;
    if (write_shb (f, application) < 0) {
        free (w);
        return NULL;
    }
    return w;
}

struct pcapng_writer *pcapng_writer_create (const char *path,
                                            const char *application)
{
    FILE *f;
    struct pcapng_writer *w;

    if (!path) {
        errno = EINVAL;
        return NULL;
    }
    if (!(f = fopen (path, "w")))
        return NULL;
    if (!(w = pcapng_writer_fcreate (f, application))) {
        int saved_errno = errno;
        fclose (f);
        errno = saved_errno;
        return NULL;
    }
    w->owned = true;
    return w;
}

int pcapng_writer_interface (struct pcapng_writer *w,
                             const char *name,
                             const char *description)
{
    struct block b;

    if (!w) {
        errno = EINVAL;
        return -1;
    }
    if (block_start (&b, BLOCK_IDB) < 0
        || block_u16 (&b, LINKTYPE_WIRESHARK_UPPER_PDU) < 0
        || block_u16 (&b, 0) < 0            // reserved
        || block_u32 (&b, 0) < 0            // snaplen: unlimited
        || block_option_string (&b, IDB_NAME, name) < 0
        || block_option_string (&b, IDB_DESCRIPTION, description) < 0) {
        free (b.buf);
        return -1;
    }
    if (block_finish (&b, w->f) < 0)
        return -1;
    return w->nifaces++;
}

/* An exported-PDU tag is big-endian, with the string value padded to
 * a multiple of 4 and the padding counted in the length, which is
 * what Wireshark itself writes.
 */
static int block_tag_string (struct block *b, uint16_t tag, const char *s)
{
    size_t len = strlen (s);
    uint8_t hdr[4];

    if (PAD4 (len) > UINT16_MAX) {
        errno = EINVAL;
        return -1;
    }
    put_be16 (hdr, tag);
    put_be16 (hdr + 2, PAD4 (len));
    return block_append (b, hdr, 4) < 0 || block_append (b, s, len) < 0
         ? -1 : 0;
}

static int block_tag_u32 (struct block *b, uint16_t tag, uint32_t v)
{
    uint8_t buf[8];

    put_be16 (buf, tag);
    put_be16 (buf + 2, 4);
    put_be32 (buf + 4, v);
    return block_append (b, buf, 8);
}

static int block_tag_end (struct block *b)
{
    uint8_t buf[4];

    put_be16 (buf, TAG_END);
    put_be16 (buf + 2, 0);
    return block_append (b, buf, 4);
}

int pcapng_writer_packet (struct pcapng_writer *w,
                          int ifid,
                          uint64_t usec,
                          enum pcapng_dir dir,
                          const char *dissector,
                          const char *info,
                          const void *data,
                          size_t len)
{
    struct block b;
    size_t pktlen_off;
    size_t data_off;

    if (!w || ifid < 0 || ifid >= w->nifaces || (!data && len > 0)) {
        errno = EINVAL;
        return -1;
    }
    if (block_start (&b, BLOCK_EPB) < 0
        || block_u32 (&b, ifid) < 0
        || block_u32 (&b, usec >> 32) < 0
        || block_u32 (&b, usec & 0xffffffff) < 0)
        goto error;
    /* Captured and original length are the tags plus the payload,
     * and are not known until the tags are written.
     */
    pktlen_off = b.len;
    if (block_u32 (&b, 0) < 0 || block_u32 (&b, 0) < 0)
        goto error;
    data_off = b.len;
    if (dissector && block_tag_string (&b, TAG_DISSECTOR_NAME, dissector) < 0)
        goto error;
    if (dir != PCAPNG_DIR_UNKNOWN
        && block_tag_u32 (&b, TAG_P2P_DIRECTION, dir) < 0)
        goto error;
    if (info && block_tag_string (&b, TAG_COL_INFO_TEXT, info) < 0)
        goto error;
    if (block_tag_end (&b) < 0)
        goto error;
    if (block_append (&b, data, len) < 0)
        goto error;
    /* The payload was padded by block_append(); the length fields
     * must not count that padding.
     */
    size_t pktlen = b.len - data_off - (PAD4 (len) - len);
    put_u32 (b.buf + pktlen_off, pktlen);
    put_u32 (b.buf + pktlen_off + 4, pktlen);
    /* The direction also goes in the pcapng packet flags.  Wireshark
     * before 3.6 does not know the exported-PDU direction tag, but
     * every version sets the direction from the flags, which is how
     * a dissector learns it.  Wireshark shows the flags word in full
     * under -V, where its other bits are Ethernet error flags.
     */
    if (dir != PCAPNG_DIR_UNKNOWN) {
        uint32_t flags = dir == PCAPNG_DIR_SENT ? 2 : 1;
        if (block_option (&b, EPB_FLAGS, &flags, 4) < 0)
            goto error;
    }
    if (w->sorted)
        return block_hold (&b, w, usec);
    return block_finish (&b, w->f);
error:
    free (b.buf);
    return -1;
}

void pcapng_writer_set_sorted (struct pcapng_writer *w, bool sorted)
{
    if (w && w->nheld == 0)
        w->sorted = sorted;
}

int pcapng_writer_destroy (struct pcapng_writer *w)
{
    int rc = 0;

    if (w) {
        int saved_errno = errno;
        if (w->nheld > 0 && held_flush (w) < 0)
            rc = -1;
        if (w->owned) {
            if (fclose (w->f) != 0)
                rc = -1;
        }
        else if (fflush (w->f) != 0)
            rc = -1;
        if (rc == 0)
            errno = saved_errno;
        free (w);
    }
    return rc;
}

// vi:ts=4 sw=4 expandtab
