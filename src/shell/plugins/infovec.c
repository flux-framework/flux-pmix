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

#include "src/common/libutil/strlcpy.h"

#include "infovec.h"

#define INFOVEC_CHUNK 8

struct infovec {
    int length;
    int count;
    pmix_info_t *info;
};

static pmix_info_t *alloc_slot (struct infovec *iv)
{
    pmix_info_t *info;

    if (iv->count == iv->length) {
        int new_length = iv->length + INFOVEC_CHUNK;
        size_t new_size = sizeof (iv->info[0]) * new_length;
        pmix_info_t *new_info;

        if (!(new_info = realloc (iv->info, new_size)))
            return NULL;
        iv->info = new_info;
        iv->length = new_length;
    }
    info = &iv->info[iv->count++];
    memset (info, 0, sizeof (*info));
    return info;
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
    strlcpy (info->key, key, sizeof (info->key));
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
    strlcpy (info->key, key, sizeof (info->key));
    info->value.type = PMIX_UINT32;
    info->value.data.uint32 = value;
    return 0;
}

int infovec_set_u16 (struct infovec *iv, const char *key, uint16_t value)
{
    pmix_info_t *info;

    if (!iv || !key) {
        errno = EINVAL;
        return -1;
    }
    if (!(info = alloc_slot (iv)))
        return -1;
    strlcpy (info->key, key, sizeof (info->key));
    info->value.type = PMIX_UINT16;
    info->value.data.uint16 = value;
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
    strlcpy (info->key, key, sizeof (info->key));
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
    strlcpy (info->key, key, sizeof (info->key));
    info->value.type = PMIX_PROC_RANK;
    info->value.data.rank = value;
    return 0;
}

int infovec_set_infovec_new (struct infovec *iv,
                             const char *key,
                             struct infovec *val)
{
    pmix_info_t *info;

    if (!iv || !key || !val) {
        errno = EINVAL;
        return -1;
    }
    if (!(info = alloc_slot (iv)))
        return -1;
    strlcpy (info->key, key, sizeof (info->key));
    info->value.type = PMIX_DATA_ARRAY;
    if (!(info->value.data.darray = calloc (1, sizeof (pmix_data_array_t))))
        return -1;
    /* steal val->info and free val */
    info->value.data.darray->type = PMIX_INFO;
    info->value.data.darray->size = val->count;
    info->value.data.darray->array = val->info;
    free (val);

    return 0;
}

int infovec_set_blob (struct infovec *iv,
                      const char *key,
                      void *value,
                      size_t size)
{
    pmix_info_t *info;

    if (!iv || !key) {
        errno = EINVAL;
        return -1;
    }
    if (!(info = alloc_slot (iv)))
        return -1;
    strlcpy (info->key, key, sizeof (info->key));
    info->value.type = PMIX_BYTE_OBJECT;
    info->value.data.bo.bytes = value;
    info->value.data.bo.size = size;
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

static void destroy_info_array (pmix_info_t *info, int count)
{
    if (info) {
        for (int i = 0; i < count; i++) {
            pmix_value_t *value = &info[i].value;
            switch (value->type) {
                case PMIX_STRING:
                    free (value->data.string);
                    break;
                case PMIX_DATA_ARRAY:
                    if (value->data.darray) { // recurse
                        destroy_info_array (value->data.darray->array,
                                            value->data.darray->size);
                        free (value->data.darray);
                    }
                    break;
            }
        }
        free (info);
    }
}

void infovec_destroy (struct infovec *iv)
{
    if (iv) {
        int saved_errno = errno;
        destroy_info_array (iv->info, iv->count);
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
