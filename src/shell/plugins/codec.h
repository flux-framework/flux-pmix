/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#include <jansson.h>
#include <pmix_server.h>

#ifndef _PX_CODEC_H
#define _PX_CODEC_H

json_t *codec_pointer_encode (void *ptr);
int codec_pointer_decode (json_t *o, void **ptr);

json_t *codec_data_encode (const void *data, size_t length);
int codec_data_decode (json_t *o, void **data, size_t *length);
ssize_t codec_data_length (json_t *o);

ssize_t codec_data_decode_bufsize (json_t *o);
ssize_t codec_data_decode_tobuf (json_t *o, void *data, size_t buflen);

json_t *codec_value_encode (const pmix_value_t *value);
int codec_value_decode (json_t *o, pmix_value_t *value); // allocs internal mem
void codec_value_release (pmix_value_t *value); // free internal mem from decode

json_t *codec_info_encode (const pmix_info_t *info);
int codec_info_decode (json_t *o, pmix_info_t *info); // allocs internal mem
void codec_info_release (pmix_info_t *info); // free internal mem from decode

json_t *codec_proc_encode (const pmix_proc_t *proc);
int codec_proc_decode (json_t *o, pmix_proc_t *proc);

json_t *codec_proc_array_encode (const pmix_proc_t *procs, size_t nprocs);
int codec_proc_array_decode (json_t *o, pmix_proc_t **procs, size_t *nprocs);

json_t *codec_info_array_encode (const pmix_info_t *info, size_t ninfo);
int codec_info_array_decode (json_t *o, pmix_info_t **info, size_t *ninfo);
void codec_info_array_destroy (pmix_info_t *info, size_t ninfo);

#endif // _PX_CODEC_H

// vi:tabstop=4 shiftwidth=4 expandtab
