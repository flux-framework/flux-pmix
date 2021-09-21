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

typedef void (*exchange_f)(struct exchange *xcg, void *arg);

/* Perform one exchange across all shell ranks.
 * 'dict' is the input from this  shell.  Once the the result of the exchange
 * is availbale, 'cb' is invoked.
 */
int exchange (struct exchange *xcg, json_t *dict, exchange_f cb, void *arg);

/* Accessors may be called only from exchange_f callback.
 * exchange_get_dict() returns a json object that is invalidated when
 * the callback returns.
 */
bool exchange_has_error (struct exchange *xcg);
json_t *exchange_get_dict (struct exchange *xcg);

#endif // _PX_EXCHANGE_H

// vi: ts=4 sw=4 expandtab

