/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _PX_INFOVEC_H
#define _PX_INFOVEC_H

#include <pmix.h>

struct infovec *infovec_create (void);
void infovec_destroy (struct infovec *iv);

int infovec_set_u32 (struct infovec *iv, const char *key, uint32_t val);
int infovec_set_u16 (struct infovec *iv, const char *key, uint16_t val);
int infovec_set_str (struct infovec *iv, const char *key, const char *str);
int infovec_set_str_new (struct infovec *iv, const char *key, char *str);
int infovec_set_bool (struct infovec *iv, const char *key, bool val);
int infovec_set_rank (struct infovec *iv, const char *key, pmix_rank_t val);
int infovec_set_infovec_new (struct infovec *iv,
                             const char *key,
                             struct infovec *val);

int infovec_count (struct infovec *iv);
pmix_info_t *infovec_info (struct infovec *iv);

#endif // _PX_INFOVEC_H

// vi:ts=4 sw=4 expandtab
