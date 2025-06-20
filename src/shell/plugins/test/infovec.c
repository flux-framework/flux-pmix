/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <errno.h>

#include "src/common/libtap/tap.h"

#include "infovec.h"

void expansion (int count)
{
    struct infovec *iv;
    int errors;

    if (!(iv = infovec_create ()))
        BAIL_OUT ("could not create infovec");

    errors = 0;
    for (int i = 0; i < count; i++) {
        if (infovec_set_u32 (iv, "foo", i) < 0)
            errors++;
    }

    ok (errors == 0,
        "able to run infovec_set_u32 %dx with no errors", count);
    ok (infovec_count (iv) == count,
        "infovec_count returns expected size");

    errors = 0;
    for (int i = 0; i < count; i++) {
        pmix_info_t *info = infovec_info (iv);
        if (strcmp (info[i].key, "foo") != 0)
            errors++;
        if (info[i].value.type != PMIX_UINT32)
            errors++;
        if (info[i].value.data.uint32 != i)
            errors++;
    }
    ok (errors == 0,
        "infovec_info returns array with correct contents");

    infovec_destroy (iv);
}

void basic (void)
{
    struct infovec *iv;
    pmix_info_t *info;

    lives_ok ({ infovec_destroy (NULL); },
        "infovec_destroy iv=NULL doesn't crash");

    iv = infovec_create ();
    ok (iv != NULL,
        "infovec_create works");
    ok (infovec_count (iv) == 0,
        "infovec_count returns 0");
    lives_ok ({ infovec_destroy (iv); },
        "infovec_destroy doesn't crash on empty array");

    if (!(iv = infovec_create ()))
        BAIL_OUT ("infovec_create failed");

    /* u32 */
    ok (infovec_set_u32 (iv, "foo", 42) == 0,
        "infovec_set_u32 foo=42 works");
    ok (infovec_count (iv) == 1,
        "infovec_count returns 1");
    info = infovec_info (iv);
    ok (info != NULL,
        "infovec_info returns non-NULL");
    ok (strcmp (info[0].key, "foo") == 0,
        "key is set correctly");
    ok (info[0].value.type == PMIX_UINT32,
        "value type is set correctly");
    ok (info[0].value.data.uint32 == 42,
        "value data is set correctly");

    /* bool */
    ok (infovec_set_bool (iv, "bar", true) == 0,
        "infovec_set_bool bar=true works");
    ok (infovec_count (iv) == 2,
        "infovec_count returns 2");
    info = infovec_info (iv);
    ok (strcmp (info[1].key, "bar") == 0,
        "key is set correctly");
    ok (info[1].value.type == PMIX_BOOL,
        "value type is set correctly");
    ok (info[1].value.data.flag == true,
        "value data is set correctly");

    /* str */
    ok (infovec_set_str (iv, "baz", "oof") == 0,
        "infovec_set_str baz=oof works");
    ok (infovec_count (iv) == 3,
        "infovec_count returns 3");
    info = infovec_info (iv);
    ok (strcmp (info[2].key, "baz") == 0,
        "key is set correctly");
    ok (info[2].value.type == PMIX_STRING,
        "value type is set correctly");
    ok (info[2].value.data.string != NULL
        && strcmp (info[2].value.data.string, "oof") == 0,
        "value data is set correctly");

    /* rank */
    ok (infovec_set_rank (iv, "erg", PMIX_RANK_WILDCARD) == 0,
        "infovec_set_rank erg=PMIX_RANK_WILDCARD works");
    ok (infovec_count (iv) == 4,
        "infovec_count returns 4");
    info = infovec_info (iv);
    ok (strcmp (info[3].key, "erg") == 0,
        "key is set correctly");
    ok (info[3].value.type == PMIX_PROC_RANK,
        "value type is set correctly");
    ok (info[3].value.data.rank == PMIX_RANK_WILDCARD,
        "value data is set correctly");

    /* byte object */
    char * bo_bytes = strdup("blob");
    size_t bo_size = strlen(bo_bytes) + 1;
    ok (infovec_set_byte_object_new (iv, "blo", bo_bytes, bo_size) == 0,
        "infovec_set_byte_object_new blo=blog works");
    ok (infovec_count (iv) == 5,
        "infovec_count returns 5");
    info = infovec_info (iv);
    ok (strcmp (info[4].key, "blo") == 0,
        "key is set correctly");
    ok (info[4].value.type == PMIX_BYTE_OBJECT,
        "value type is set correctly");
    ok (info[4].value.data.bo.size == bo_size,
        "size is set correctly");
    ok (info[4].value.data.bo.bytes != NULL
        && memcmp (info[4].value.data.bo.bytes, bo_bytes, bo_size) == 0,
        "bytes are set correctly");

    infovec_destroy (iv);
}

void nested (void)
{
    struct infovec *iv1 = NULL;
    struct infovec *iv2 = NULL;
    struct infovec *iv3 = NULL;

    if (!(iv1 = infovec_create ())
        || !(iv2 = infovec_create ())
        || !(iv3 = infovec_create ()))
        BAIL_OUT ("infovec_create failed");

    if (infovec_set_str (iv3, "foo", "bar") < 0
        || infovec_set_str (iv3, "foo2", "bar2") < 0)
        BAIL_OUT ("infovec_set_str failed");

    ok (infovec_set_infovec_new (iv2, "iv3", iv3) == 0
        && infovec_set_infovec_new (iv1, "iv2", iv2) == 0,
        "infovec_set_infovec_new works");

    infovec_destroy (iv1);
}

void badarg (void)
{
    struct infovec *iv;

    if (!(iv = infovec_create ()))
        BAIL_OUT ("infovec_create failed");

    errno = 0;
    ok (infovec_set_u16 (NULL, "foo", 42) < 0 && errno == EINVAL,
        "infovec_set_u16 iv=NULL fails with EINVAL");
    errno = 0;
    ok (infovec_set_u16 (iv, NULL, 42) < 0 && errno == EINVAL,
        "infovec_set_u16 key=NULL fails with EINVAL");
    errno = 0;
    ok (infovec_set_u32 (NULL, "foo", 42) < 0 && errno == EINVAL,
        "infovec_set_u32 iv=NULL fails with EINVAL");
    errno = 0;
    ok (infovec_set_u32 (iv, NULL, 42) < 0 && errno == EINVAL,
        "infovec_set_u32 key=NULL fails with EINVAL");
    errno = 0;
    ok (infovec_set_bool (NULL, "foo", false) < 0 && errno == EINVAL,
        "infovec_set_bool iv=NULL fails with EINVAL");
    errno = 0;
    ok (infovec_set_bool (iv, NULL, true) < 0 && errno == EINVAL,
        "infovec_set_bool key=NULL fails with EINVAL");
    errno = 0;
    ok (infovec_set_rank (NULL, "foo", 2) < 0 && errno == EINVAL,
        "infovec_set_rank iv=NULL fails with EINVAL");
    errno = 0;
    ok (infovec_set_rank (iv, NULL, 2) < 0 && errno == EINVAL,
        "infovec_set_rank key=NULL fails with EINVAL");
    errno = 0;
    ok (infovec_set_str (NULL, "foo", "bar") < 0 && errno == EINVAL,
        "infovec_set_str iv=NULL fails with EINVAL");
    errno = 0;
    ok (infovec_set_str (iv, NULL, "bar") < 0 && errno == EINVAL,
        "infovec_set_str key=NULL fails with EINVAL");
    errno = 0;
    ok (infovec_set_str (iv, "foo", NULL) < 0 && errno == EINVAL,
        "infovec_set_str value=NULL fails with EINVAL");
    errno = 0;
    ok (infovec_set_infovec_new (NULL, "foo", iv) < 0 && errno == EINVAL,
        "infovec_set_infovec_new iv=NULL fails with EINVAL");
    errno = 0;
    ok (infovec_set_infovec_new (iv, NULL, iv) < 0 && errno == EINVAL,
        "infovec_set_infovec_new key=NULL fails with EINVAL");
    errno = 0;
    ok (infovec_set_infovec_new (iv, "foo", NULL) < 0 && errno == EINVAL,
        "infovec_set_infovec_new value=NULL fails with EINVAL");

    infovec_destroy (iv);
}

int main (int argc, char **argv)
{
    plan (NO_PLAN);

    basic ();
    nested ();
    expansion (32); // make it > INFOVEC_CHUNK (8)
    badarg ();

    done_testing ();
    return 0;
}

// vi:ts=4 sw=4 expandtab
