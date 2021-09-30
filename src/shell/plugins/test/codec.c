#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <jansson.h>
#include <pmix_server.h>

#include "src/common/libtap/tap.h"

#include "codec.h"

void check_data (void)
{
    json_t *o;
    char in_buf[64];
    int in_len;
    void *out_buf;
    size_t out_len;

    snprintf (in_buf, sizeof (in_buf), "foobar");
    in_len = strlen (in_buf) + 1;
    out_buf = NULL;
    out_len = -1;
    ok ((o = codec_data_encode (in_buf, in_len)) != NULL,
        "codec_data_encode %d bytes works", in_len);
    ok (codec_data_decode (o, &out_buf, &out_len) == 0,
        "codec_data_decode works");
    diag ("out_len = %d", out_len);
    ok (out_len == in_len
        && out_buf != NULL
        && memcmp (in_buf, out_buf, out_len) == 0,
        "codec_data_decode returned the correct value");
    free (out_buf);
}

void check_pointer (void)
{
    json_t *o;
    void *ptr_in;
    void *ptr_out;

    ptr_in = (void *)~0LL;
    ptr_out = (void *)0xdeadbeef;
    ok ((o = codec_pointer_encode (ptr_in)) != NULL,
        "codec_pointer_encode ~0LL works");
    ok (codec_pointer_decode (o, &ptr_out) == 0,
        "codec_pointer_decode works");
    ok (ptr_in == ptr_out,
        "codec_pointer_decode returned the correct value");

    ptr_in = 0;
    ptr_out = (void *)0xdeadbeef;
    ok ((o = codec_pointer_encode (ptr_in)) != NULL,
        "codec_pointer_encode 0 works");
    ok (codec_pointer_decode (o, &ptr_out) == 0,
        "codec_pointer_decode works");
    ok (ptr_in == ptr_out,
        "codec_pointer_decode returned the correct value");
}

void check_value ()
{
    json_t *o;
    pmix_value_t val;
    pmix_value_t val2;
    pmix_proc_t proc;

    val.type = PMIX_BOOL;
    val.data.flag = true;
    o = codec_value_encode (&val);
    ok (codec_value_decode (o, &val2) == 0
        && val.type == val2.type
        && val.data.flag == val2.data.flag,
        "codec_value_encode/decode works for boolean");
    json_decref (o);

    val.type = PMIX_BYTE;
    val.data.byte = 0x42;
    o = codec_value_encode (&val);
    ok (codec_value_decode (o, &val2) == 0
        && val.type == val2.type
        && val.data.byte == val2.data.byte,
        "codec_value_encode/decode works for byte");

    val.type = PMIX_STRING;
    val.data.string = "foo";
    o = codec_value_encode (&val);
    ok (codec_value_decode (o, &val2) == 0
        && val.type == val2.type
        && !strcmp (val.data.string, val2.data.string),
        "codec_value_encode/decode works for string");
    codec_value_release (&val2);

    val.type = PMIX_SIZE;
    val.data.size = 777;
    o = codec_value_encode (&val);
    ok (codec_value_decode (o, &val2) == 0
        && val.type == val2.type
        && val.data.size == val2.data.size,
        "codec_value_encode/decode works for size");

    val.type = PMIX_PID;
    val.data.pid = 69;
    o = codec_value_encode (&val);
    ok (codec_value_decode (o, &val2) == 0
        && val.type == val2.type
        && val.data.pid == val2.data.pid,
        "codec_value_encode/decode works for pid");

    // TODO INT
    // TODO INT8
    // TODO INT16
    // TODO INT32
    // TODO INT64

    // TODO UINT
    // TODO UINT8
    // TODO UINT64

    val.type = PMIX_UINT16;
    val.data.uint16 = 0x9999;
    o = codec_value_encode (&val);
    ok (codec_value_decode (o, &val2) == 0
        && val.type == val2.type
        && val.data.uint16 == val2.data.uint16,
        "codec_value_encode/decode works for uint16");

    val.type = PMIX_UINT32;
    val.data.uint32 = 0xeeeeffff;
    o = codec_value_encode (&val);
    ok (codec_value_decode (o, &val2) == 0
        && val.type == val2.type
        && val.data.uint32 == val2.data.uint32,
        "codec_value_encode/decode works for uint32");

    val.type = PMIX_PROC_RANK;
    val.data.rank= 1;
    o = codec_value_encode (&val);
    ok (codec_value_decode (o, &val2) == 0
        && val.type == val2.type
        && val.data.rank == val2.data.rank,
        "codec_value_encode/decode works for rank");

    // TODO FLOAT
    // TODO DOUBLE
    // TODO TIMEVAL
    // TODO TIME
    // TODO STATUS

    proc.rank = 3;
    snprintf (proc.nspace, sizeof (proc.nspace), "xyz");

    val.type = PMIX_PROC;
    val.data.proc = &proc;
    o = codec_value_encode (&val);
    ok (codec_value_decode (o, &val2) == 0
        && val.type == val2.type
        && val.data.proc->rank == val2.data.proc->rank
        && !strcmp (val.data.proc->nspace, val2.data.proc->nspace),
        "codec_value_encode/decode works for proc");
    codec_value_release (&val2);
}

void check_proc_array (void)
{
    json_t *o;
    pmix_proc_t *procs;
    size_t nprocs;

    /* null array in and out */
    o = codec_proc_array_encode (NULL, 0);
    ok (o != NULL,
        "codec_proc_array_encode procs=NULL nprocs=0 works");

    nprocs = 42;
    ok (codec_proc_array_decode (o, &procs, &nprocs) == 0,
        "codec_proc_array_decode works");
    ok (nprocs == 0,
        "nprocs is set to 0");
}

int main (int argc, char **argv)
{
    plan (NO_PLAN);

    check_pointer ();
    check_data ();
    check_value ();

    // TODO pmix_info_t
    // TODO pmix_proc_t

    check_proc_array ();

    // TODO array of pmix_info_t

    done_testing ();
    return 0;
}

// vi:ts=4 sw=4 expandtab
