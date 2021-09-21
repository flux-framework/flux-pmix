/************************************************************\
 * Copyright 2020 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

#ifndef _PX_EXCHANGE_H
#define _PX_EXCHANGE_H

/* Create handle for performing multiple sequential exchanges.
 * 'k' is the tree fanout (k=0 selects internal default).
 */
struct exchange *exchange_create (flux_shell_t *shell, int k);
void exchange_destroy (struct exchange *xcg);

typedef void (*exchange_exit_f)(struct exchange *xcg, void *arg);

/* Perform one exchange across all shell ranks.
 * 'data' is the input from this shell, in the form of a base64 json string.
 * Once the the result of the exchange is available, 'exit_cb' is invoked.
 */
int exchange_enter_base64_string (struct exchange *xcg,
                                  json_t *data,
                                  exchange_exit_f exit_cb,
                                  void *exit_cb_arg);

/* This may be called from the exchange_exit_f callback
 * to determine whether or not the exchange was successful.
 */
bool exchange_has_error (struct exchange *xcg);

/* Accessor to be called only from exchange_exit_f callback.
 * The caller must free the 'data' result, if successful.
 * 'data' is built by decoding the base64-encoded blobs collected from
 * each shell and concatenating them in random order.
 * This is consistent with the semantics of the fence_nb server callback.
 */
int exchange_get_data (struct exchange *xcg, void **data, size_t *size);

#endif // _PX_EXCHANGE_H

// vi: ts=4 sw=4 expandtab

