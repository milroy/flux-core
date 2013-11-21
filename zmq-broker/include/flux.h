#ifndef FLUX_H
#define FLUX_H

/* The flux_t handle and its operations works in multiple environments.
 * Not all environments will implement all operations.
 * If an unimplemented function is called, it will return an error
 * with errno == ENOSYS.
 */

typedef void (*FluxFreeFn)(void *arg);

typedef struct flux_handle_struct *flux_t;

#include <czmq.h>

#include "kvs.h"
#include "mrpc.h"

/* Flags for handle creation and flux_flags_set()/flux_flags_unset.
 */
enum {
    FLUX_FLAGS_TRACE = 1,   /* print 0MQ messages sent over the flux_t */
                            /*   handle on stdout. */
};

/* API users: create a flux_t handle using cmb_init().
 * Destroy it with flux_handle_destroy().
 */
void flux_handle_destroy (flux_t *hp);

/* A mechanism is provide for other modules to attach auxiliary state to
 * the flux_t handle by name.  The FluxFreeFn, if non-NULL, will be called
 * to destroy this state when the handle is destroyed.
 */
void *flux_aux_get (flux_t h, const char *name);
void flux_aux_set (flux_t h, const char *name, void *aux, FluxFreeFn destroy);

/* Set/clear FLUX_FLAGS_* on a flux_t handle.
 * Flags can also be set when the handle is created, e.g. in cmb_init_full().
 */
void flux_flags_set (flux_t h, int flags);
void flux_flags_unset (flux_t h, int flags);

/* Send/receive requests and responses.
 * - flux_request_sendmsg() expects a route delimiter (request envelope)
 * - flux_request_sendmsg()/flux_response_sendmsg() free '*zmsg' and set it
 *   to NULL on success.
 * - int-returning functions return 0 on success, -1 on failure with errno set.
 * - pointer-returning functions return NULL on failure with errno set.
 */
int flux_request_sendmsg (flux_t h, zmsg_t **zmsg);
zmsg_t *flux_request_recvmsg (flux_t h, bool nb);
int flux_response_sendmsg (flux_t h, zmsg_t **zmsg);
zmsg_t *flux_response_recvmsg (flux_t h, bool nb);
int flux_response_putmsg (flux_t h, zmsg_t **zmsg);
int flux_request_send (flux_t h, json_object *request, const char *fmt, ...);
json_object *flux_rpc (flux_t h, json_object *in, const char *fmt, ...);
int flux_response_recv (flux_t h, json_object **respp, char **tagp, bool nb);
int flux_respond (flux_t h, zmsg_t **request, json_object *response);
int flux_respond_errnum (flux_t h, zmsg_t **request, int errnum);

/* Send/receive events.
 * - an event consists of a tag frame and an optional JSON frame
 * - flux_event_sendmsg() frees '*zmsg' and sets it to NULL on success.
 * - int-returning functions return 0 on success, -1 on failure with errno set.
 * - pointer-returning functions return NULL on failure with errno set.
 * - topics are period-delimited strings following 0MQ subscription semantics
 */
int flux_event_sendmsg (flux_t h, zmsg_t **zmsg);
zmsg_t *flux_event_recvmsg (flux_t h, bool nonblock);
int flux_event_send (flux_t h, json_object *request, const char *fmt, ...);
int flux_event_subscribe (flux_t h, const char *topic);
int flux_event_unsubscribe (flux_t h, const char *topic);

/* Receive messages from the cmbd's snoop socket.
 * - int-returning functions return 0 on success, -1 on failure with errno set.
 * - pointer-returning functions return NULL on failure with errno set.
 * - topics are period-delimited strings following 0MQ subscription semantics
 */
zmsg_t *flux_snoop_recvmsg (flux_t h, bool nb);
int flux_snoop_subscribe (flux_t h, const char *topic);
int flux_snoop_unsubscribe (flux_t h, const char *topic);

/* Get information about this cmbd instance's position in the flux comms
 * session.
 */
int flux_info (flux_t h, int *rankp, int *sizep, bool *treerootp);
int flux_rank (flux_t h);
int flux_size (flux_t h);
bool flux_treeroot (flux_t h);

/* Manipulate and query cmb routing tables.
 */
int flux_route_add (flux_t h, const char *dst, const char *gw);
int flux_route_del (flux_t h, const char *dst, const char *gw);
json_object *flux_route_query (flux_t h);

/* Set/clear/test timeout callback arming.
 */
int flux_timeout_set (flux_t h, unsigned long msec);
int flux_timeout_clear (flux_t h);
bool flux_timeout_isset (flux_t h);

/* Accessors for zloop reactor and zeromq context.
 * N.B. The zctx_t is thread-safe but zeromq sockets, and therefore
 * flux_t handle operations are not.
 */
zloop_t *flux_get_zloop (flux_t h);
zctx_t *flux_get_zctx (flux_t h);

/* Ping plugin 'name'.
 * 'pad' is a string used to increase the size of the ping packet for
 * measuring RTT in comparison to rough message size.
 * 'seq' is a sequence number.
 * The pad and seq are echoed in the response, and any mismatch will result
 * in an error return with errno = EPROTO.
 * A string representation of the route taken is the return value on success
 * (caller must free).  On error, return NULL with errno set.
 */
char *flux_ping (flux_t h, const char *name, const char *pad, int seq);

/* Execute a barrier across 'nprocs' processes.
 * The 'name' must be unique across the comms session.
 */
int flux_barrier (flux_t h, const char *name, int nprocs);

/* Interface for logging via the comms' reduction network.
 */
void flux_log_set_facility (flux_t h, const char *facility);
int flux_vlog (flux_t h, int lev, const char *fmt, va_list ap);
int flux_log (flux_t h, int lev, const char *fmt, ...)
            __attribute__ ((format (printf, 3, 4)));

int flux_log_subscribe (flux_t h, int lev, const char *sub);
int flux_log_unsubscribe (flux_t h, const char *sub);
int flux_log_dump (flux_t h, int lev, const char *fac);

char *flux_log_decode (zmsg_t *zmsg, int *lp, char **fp, int *cp,
                    struct timeval *tvp, char **sp);

#endif /* !defined(FLUX_H) */

/*
 * vi:tabstop=4 shiftwidth=4 expandtab
 */