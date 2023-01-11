/************************************************************\
 * Copyright 2023 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* pmix.c - Flux "universal" pmi client plugin
 *
 * This implements a pmix client plugin that can be used by flux-broker(1)
 * for bootstrapping in a foreign environment or by flux-pmi(1) for testing.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdarg.h>
#include <pmix.h>
#include <flux/core.h>

#include "src/common/libutil/strlcpy.h"

static const char *plugin_name = "pmix";
static pmix_proc_t myproc;

/* This function was borrowed from flux-core src/common/libpmi/upmi.c.
 * External plugins use the public flux "standard plugin" interface only.
 */
static void setverror (flux_plugin_t *p,
                       flux_plugin_arg_t *args,
                       const char *fmt,
                       va_list ap)
{
    char buf[128];

    (void)vsnprintf (buf, sizeof (buf), fmt, ap);
    (void)flux_plugin_arg_pack (args,
                                FLUX_PLUGIN_ARG_OUT,
                                "{s:s}",
                                "errmsg", buf);
}

__attribute__ ((format (printf, 3, 4)))
static int seterror (flux_plugin_t *p,
                     flux_plugin_arg_t *args,
                     const char *fmt,
                     ...)
{
    va_list ap;

    va_start (ap, fmt);
    setverror (p, args, fmt, ap);
    va_end (ap);
    return -1;
}

/* op_put - store key+value in the kvs
 * commit is deferred until op_barrier
 */
static int op_put (flux_plugin_t *p,
                   const char *topic,
                   flux_plugin_arg_t *args,
                   void *data)
{
    pmix_status_t rc;
    pmix_value_t val = { .type = PMIX_STRING };
    const char *key;

    if (flux_plugin_arg_unpack (args,
                                FLUX_PLUGIN_ARG_IN,
                                "{s:s s:s}",
                                "key", &key,
                                "value", &val.data.string) < 0)
        return seterror (p, args, "error unpacking put arguments");

    rc = PMIx_Put (PMIX_GLOBAL, key, &val);
    if (rc != PMIX_SUCCESS)
        return seterror (p, args, "PMIx_Put: %s", PMIx_Error_string (rc));

    return 0;
}

/* op_get - retrieve a key+value from kvs
 * The 'rank' specifies the rank that stored the key, or -1 for keys that
 * were pre-put by the pmix server.
 */
static int op_get (flux_plugin_t *p,
                   const char *topic,
                   flux_plugin_arg_t *args,
                   void *data)
{
    pmix_proc_t proc;
    const char *key;
    int rank;
    pmix_status_t rc;
    pmix_value_t *val;

    if (flux_plugin_arg_unpack (args,
                                FLUX_PLUGIN_ARG_IN,
                                "{s:s s:i}",
                                "key", &key,
                                "rank", &rank) < 0)
        return seterror (p, args, "error unpacking get arguments");

    strlcpy (proc.nspace, myproc.nspace, sizeof (proc.nspace));
    proc.rank = rank < 0 ? PMIX_RANK_WILDCARD : rank;

    rc = PMIx_Get (&proc, key, NULL, 0, &val);
    if (rc != PMIX_SUCCESS)
        return seterror (p, args, "PMIx_Get: %s", PMIx_Error_string (rc));
    if (val->type != PMIX_STRING || !val->data.string) {
        PMIX_VALUE_RELEASE (val);
        return seterror (p, args, "PMIx_Get: wrong value type");
    }
    if (flux_plugin_arg_pack (args,
                              FLUX_PLUGIN_ARG_OUT,
                              "{s:s}",
                              "value", val->data.string) < 0) {
        PMIX_VALUE_RELEASE (val);
        return -1;
    }
    PMIX_VALUE_RELEASE (val);
    return 0;
}

/* op_barrier - perform exchange of keys put by op_put
 */
static int op_barrier (flux_plugin_t *p,
                       const char *topic,
                       flux_plugin_arg_t *args,
                       void *data)
{
    pmix_status_t rc;
    pmix_info_t info;

    rc = PMIx_Commit ();
    if (rc != PMIX_SUCCESS)
        return seterror (p, args, "PMIx_Commit: %s", PMIx_Error_string (rc));

    memset (&info, 0, sizeof (info));
    strlcpy (info.key, PMIX_COLLECT_DATA, sizeof (info.key));
    info.value.type = PMIX_BOOL;
    info.value.data.flag = true;

    rc = PMIx_Fence (NULL, 0, &info, 1);
    if (rc!= PMIX_SUCCESS)
        return seterror (p, args, "PMIx_Fence: %s", PMIx_Error_string (rc));

    return 0;
}

/* op_initialize - initialize pmix library (e.g. establish connections, start
 * internal progress thread) and obtain basic parallel program parameters.
 */
static int op_initialize (flux_plugin_t *p,
                          const char *topic,
                          flux_plugin_arg_t *args,
                          void *data)
{
    pmix_status_t rc;
    pmix_proc_t proc;
    pmix_value_t *val;

    rc = PMIx_Init (&myproc, NULL, 0);
    if (rc != PMIX_SUCCESS)
        return seterror (p, args, "PMIx_Init: %s", PMIx_Error_string (rc));

    strlcpy (proc.nspace, myproc.nspace, sizeof (proc.nspace));
    proc.rank = PMIX_RANK_WILDCARD;

    rc = PMIx_Get (&proc, PMIX_JOB_SIZE, NULL, 0, &val);
    if (rc != PMIX_SUCCESS)
        return seterror (p, args, "PMIx_Get size: %s", PMIx_Error_string (rc));
    if (val->type != PMIX_UINT32) {
        PMIX_VALUE_RELEASE (val);
        return seterror (p, args, "PMIx_Get size: wrong type");
    }

    if (flux_plugin_arg_pack (args,
                              FLUX_PLUGIN_ARG_OUT,
                              "{s:i s:s s:i}",
                              "rank", myproc.rank,
                              "name", myproc.nspace,
                              "size", val->data.uint32) < 0) {
        PMIX_VALUE_RELEASE (val);
        return -1;
    }
    PMIX_VALUE_RELEASE (val);
    return 0;
}

/* op_finalize - finalize the pmix library (drop connections, stop thread)
 */
static int op_finalize (flux_plugin_t *p,
                        const char *topic,
                        flux_plugin_arg_t *args,
                        void *data)
{
    pmix_status_t rc;

    rc = PMIx_Finalize (NULL, 0);
    if (rc != PMIX_SUCCESS)
        return seterror (p, args, "%s",  PMIx_Error_string (rc));

    return 0;
}

/* op_preinit - probe the environment for clues that pmix should be used.
 * N.B. we could attach a local context to the flux_plugin_t object here if
 * needed.  For now 'myproc' is global.
 */
static int op_preinit (flux_plugin_t *p,
                       const char *topic,
                       flux_plugin_arg_t *args,
                       void *data)
{
    if (!getenv ("PMIX_SERVER_URI") && !getenv ("PMIX_SERVER_URI2"))
        return seterror (p, args, "PMIX_SERVER_URI* not found in environment");
    return 0;
}

static const struct flux_plugin_handler optab[] = {
    { "upmi.put",           op_put,         NULL },
    { "upmi.get",           op_get,         NULL },
    { "upmi.barrier",       op_barrier,     NULL },
    { "upmi.initialize",    op_initialize,  NULL },
    { "upmi.finalize",      op_finalize,    NULL },
    { "upmi.preinit",       op_preinit,     NULL },
    { 0 },
};

int flux_plugin_init (flux_plugin_t *p)
{
    if (flux_plugin_register (p, plugin_name, optab) < 0)
        return -1;
    return 0;
}

// vi:ts=4 sw=4 expandtab
