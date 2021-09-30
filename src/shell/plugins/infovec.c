/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* infovec.c - helper class for working with pmix_info_t arrays
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <errno.h>
#include <pmix.h>

#include "infovec.h"

#define INFOVEC_CHUNK 8

struct infovec {
    int length;
    int count;
    pmix_info_t *info;
};

static pmix_info_t *alloc_slot (struct infovec *iv)
{
    if (iv->count == iv->length) {
        int new_length = iv->length + INFOVEC_CHUNK;
        size_t new_size = sizeof (iv->info[0]) * new_length;
        pmix_info_t *new_info;

        if (!(new_info = realloc (iv->info, new_size)))
            return NULL;
        iv->info = new_info;
        iv->length = new_length;
    }
    return &iv->info[iv->count++];
}

int infovec_set_str_new (struct infovec *iv, const char *key, char *val)
{
    pmix_info_t *info;

    if (!iv || !key || !val) {
        errno = EINVAL;
        return -1;
    }
    if (!(info = alloc_slot (iv)))
        return -1;
    strncpy (info->key, key, PMIX_MAX_KEYLEN);
    info->value.type = PMIX_STRING;
    info->value.data.string = val;
    return 0;
}

int infovec_set_str (struct infovec *iv, const char *key, const char *val)
{
    char *cpy;

    if (!iv || !key || !val) {
        errno = EINVAL;
        return -1;
    }
    if (!(cpy = strdup (val))
        || infovec_set_str_new (iv, key, cpy) < 0) {
        int saved_errno = errno;
        free (cpy);
        errno = saved_errno;
        return -1;
    }
    return 0;
}

int infovec_set_u32 (struct infovec *iv, const char *key, uint32_t value)
{
    pmix_info_t *info;

    if (!iv || !key) {
        errno = EINVAL;
        return -1;
    }
    if (!(info = alloc_slot (iv)))
        return -1;
    strncpy (info->key, key, PMIX_MAX_KEYLEN);
    info->value.type = PMIX_UINT32;
    info->value.data.uint32 = value;
    return 0;
}

int infovec_set_bool (struct infovec *iv, const char *key, bool value)
{
    pmix_info_t *info;

    if (!iv || !key) {
        errno = EINVAL;
        return -1;
    }
    if (!(info = alloc_slot (iv)))
        return -1;
    strncpy (info->key, key, PMIX_MAX_KEYLEN);
    info->value.type = PMIX_BOOL;
    info->value.data.flag = value;
    return 0;
}

int infovec_set_rank (struct infovec *iv,
                      const char *key,
                      pmix_rank_t value)
{
    pmix_info_t *info;

    if (!iv || !key) {
        errno = EINVAL;
        return -1;
    }
    if (!(info = alloc_slot (iv)))
        return -1;
    strncpy (info->key, key, PMIX_MAX_KEYLEN);
    info->value.type = PMIX_PROC_RANK;
    info->value.data.rank = value;
    return 0;
}

int infovec_count (struct infovec *iv)
{
    return iv->count;
}

pmix_info_t *infovec_info (struct infovec *iv)
{
    return iv->info;
}

void infovec_destroy (struct infovec *iv)
{
    if (iv) {
        int saved_errno = errno;
        for (int i = 0; i < iv->count; i++) {
            pmix_value_t *value = &iv->info[i].value;
            switch (value->type) {
                case PMIX_STRING:
                    free (value->data.string);
                    break;
            }
        }
        free (iv);
        errno = saved_errno;
    }
}

struct infovec *infovec_create (void)
{
    struct infovec *iv;
    if (!(iv = calloc (1, sizeof (*iv))))
        return NULL;
    return iv;
}

// vi:tabstop=4 shiftwidth=4 expandtab
