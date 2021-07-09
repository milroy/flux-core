/************************************************************\
 * Copyright 2021 Lawrence Livermore National Security, LLC
 * (c.f. AUTHORS, NOTICE.LLNS, COPYING)
 *
 * This file is part of the Flux resource manager framework.
 * For details, see https://github.com/flux-framework.
 *
 * SPDX-License-Identifier: LGPL-3.0
\************************************************************/

/* flux-kube.c - If attributes.system.R exists in jobspec, then
 *  bypass scheduler alloc protocol and use R directly (for instance
 *  owner use only)
 */


#include <unistd.h>
#include <sys/types.h>

#include <jansson.h>
#include <flux/core.h>
#include <flux/jobtap.h>

#include "src/common/librlist/rlist.h"

static void alloc_continuation (flux_future_t *f, void *arg)
{
    flux_plugin_t *p = arg;
    flux_jobid_t *idptr = flux_future_aux_get (f, "jobid");

    if (flux_future_get (f, NULL) < 0) {
        flux_jobtap_raise_exception (p,
                                     *idptr,
                                     "alloc", 0,
                                     "failed to commit R to kvs: %s",
                                      strerror (errno));
        goto done;
    }
    if (flux_jobtap_event_post_pack (p,
                                     *idptr,
                                     "alloc",
                                     "{s:b}",
                                     "bypass", true) < 0)
        flux_jobtap_raise_exception (p,
                                     *idptr,
                                     "alloc", 0,
                                     "failed to post alloc event: %s",
                                     strerror (errno));

    /*  Set "needs-free" so that flux-kube knows that a "free"
     *   event needs to be emitted for this node.
     */
    if (flux_jobtap_job_aux_set (p, *idptr, "flux-kube::free", p, NULL) < 0)
        flux_log_error (flux_jobtap_get_flux (p),
                        "id=%ju: Failed to set flux-kube::free",
                        *idptr);

done:
    flux_future_destroy (f);
}

static int alloc_start (flux_plugin_t *p,
                        flux_jobid_t id,
                        const char *R)
{
    flux_t *h;
    char key[64];
    flux_future_t *f = NULL;
    flux_kvs_txn_t *txn = NULL;
    flux_jobid_t *idptr = NULL;

    if (!(h = flux_jobtap_get_flux (p))
        || flux_job_kvs_key (key, sizeof (key), id, "R") < 0
        || !(txn = flux_kvs_txn_create ())
        || flux_kvs_txn_put (txn, 0, key, R) < 0
        || !(f = flux_kvs_commit (h, NULL, 0, txn))
        || flux_future_then (f, -1, alloc_continuation, p)
        || !(idptr = calloc (1, sizeof (*idptr)))
        || flux_future_aux_set (f, "jobid", idptr, free) < 0) {
        flux_kvs_txn_destroy (txn);
        flux_future_destroy (f);
        free (idptr);
        return -1;
    }
    *idptr = id;
    flux_kvs_txn_destroy (txn);
    return 0;
}


static int sched_cb (flux_plugin_t *p,
                     const char *topic,
                     flux_plugin_arg_t *args,
                     void *arg)
{
    const char *R = NULL;
    flux_jobid_t id;

    /*  If flux-kube::R set on this job then commit R to KVS
     *   and set flux-kube flag
     */
    if (!(R = flux_jobtap_job_aux_get (p,
                                       FLUX_JOBTAP_CURRENT_JOB,
                                       "flux-kube::R")))
        return 0;


    if (flux_plugin_arg_unpack (args,
                                FLUX_PLUGIN_ARG_IN,
                                "{s:I}",
                                "id", &id) < 0) {
        flux_jobtap_raise_exception (p, FLUX_JOBTAP_CURRENT_JOB,
                                     "alloc", 0,
                                     "flux-kube: %s: unpack: %s",
                                     topic,
                                     flux_plugin_arg_strerror (args));
        return -1;
    }

    if (alloc_start (p, id, R) < 0)
        flux_jobtap_raise_exception (p, id, "alloc", 0,
                                     "failed to commit R to kvs");

    if (flux_jobtap_job_set_flag (p,
                                  FLUX_JOBTAP_CURRENT_JOB,
                                  "alloc-bypass") < 0)
        return flux_jobtap_raise_exception (p, FLUX_JOBTAP_CURRENT_JOB,
                                            "alloc", 0,
                                            "Failed to set alloc-bypass: %s",
                                            strerror (errno));
    return 0;
}

static int cleanup_cb (flux_plugin_t *p,
                       const char *topic,
                       flux_plugin_arg_t *args,
                       void *arg)
{
    /*  If flux-kube::free is set on this job, then this plugin
     *   sent an "alloc" event, so a "free" event needs to be sent now.
     */
    if (flux_jobtap_job_aux_get (p,
                                 FLUX_JOBTAP_CURRENT_JOB,
                                 "alloc-bypass::free")) {
        if (flux_jobtap_event_post_pack (p,
                                         FLUX_JOBTAP_CURRENT_JOB,
                                         "free",
                                         NULL) < 0)
             flux_log_error (flux_jobtap_get_flux (p),
                             "flux-kube: failed to post free event");
    }
    return 0;
}

static int validate_cb (flux_plugin_t *p,
                        const char *topic,
                        flux_plugin_arg_t *args,
                        void *arg)
{
    json_t *R = NULL;
    char *s;
    struct rlist *rl;
    json_error_t error;
    uint32_t userid = (uint32_t) -1;

    if (flux_plugin_arg_unpack (args,
                                FLUX_PLUGIN_ARG_IN,
                                "{s:i s:{s:{s:{s?{s?o}}}}}",
                                "userid", &userid,
                                "jobspec",
                                 "attributes",
                                  "system",
                                   "flux-kube",
                                    "R", &R) < 0) {
        return flux_jobtap_reject_job (p,
                                       args,
                                       "invalid system.flux-kube.R: %s",
                                       flux_plugin_arg_strerror (args));
    }

    /*  Nothing to do if no R provided
     */
    if (R == NULL)
        return 0;

    if (userid != getuid ())
        return flux_jobtap_reject_job (p,
                                       args,
                                       "Guest user cannot use alloc bypass");

    /*  Sanity check R for validity
     */
    if (!(rl = rlist_from_json (R, &error)))
        return flux_jobtap_reject_job (p,
                                       args,
                                       "flux-kube: invalid R: %s",
                                       error.text);
    rlist_destroy (rl);

    /*  Store R string in job structure to avoid re-fetching from plugin args
     *   in job.state.sched callback.
     */
    if (!(s = json_dumps (R, 0))
        || flux_jobtap_job_aux_set (p,
                                    FLUX_JOBTAP_CURRENT_JOB,
                                    "flux-kube::R",
                                    s,
                                    free) < 0) {
        free (s);
        return flux_jobtap_reject_job (p,
                                       args,
                                       "failed to capture flux-kube R: %s",
                                       strerror (errno));
    }
    return 0;
}

static const struct flux_plugin_handler tab[] = {
    { "job.state.sched",   sched_cb,    NULL },
    { "job.state.cleanup", cleanup_cb,  NULL },
    { "job.validate",      validate_cb, NULL },
    { 0 }
};


int flux_plugin_init (flux_plugin_t *p)
{
    return flux_plugin_register (p, "flux-kube", tab);
}
