
#include "sos.h"
#include "sos_debug.h"
#include "sos_error.h"
#include "sosd.h"
#include "sosd_cloud_evpath.h"

/* name.........: SOSD_cloud_init
 * parameters...: argc, argv (passed in by address)
 * return val...: 0 if no errors
 * description..:
 *     This routine stands up the off-node transport services for the daemon
 *     and launches any particular threads it needs to in order to do that.
 *
 *     In the MPI-version, this function is responsible for populating the
 *     following global values.  Some reasonable values will at least need
 *     to be plugged into the SOS->config.* variables.
 *
 *        SOS->config.comm_rank
 *        SOS->config.comm_size
 *        SOS->config.comm_support = MPI_THREAD_*
 *        SOSD.daemon.cloud_sync_target_set[n]  (int: rank)
 *        SOSD.daemon.cloud_sync_target_count
 *        SOSD.daemon.cloud_sync_target
 *
 *    The SOSD.daemon.cloud_sync stuff can likely change here, if EVPATH
 *    is going to handle it's business differently.  The sync_target refers
 *    to the centralized store (here, stone?) that this daemon is pointing to
 *    for off-node transport.  The general system allows for multiple "back-
 *    plane stores" launched alongside the daemons, to provide reasonable
 *    scalability and throughput.
 */
int SOSD_cloud_init(int *argc, char ***argv) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_init.EVPATH");

    dlog(5, "This is an example of a debugging message.\n");

    return 0;
}


/* name.......: SOSD_cloud_start
 * description: In the event that initialization and activation are not
 *    necessarily the same, when this function returns the communication
 *    between sosd instances is active, and all cloud functions are
 *    operating.
 */
int SOSD_cloud_start(void) {
    return 0;
}



/* name.......: SOSD_cloud_send
 * description: Actually send a message off-node.  (blocking)
 */
int SOSD_cloud_send(SOS_buffer *buffer, SOS_buffer *reply) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_send.EVPATH");

    return 0;
}


/* name.......: SOSD_cloud_enqueue
 * description: Accept a message into the async send-queue.  (non-blocking)
 */
void  SOSD_cloud_enqueue(SOS_buffer *buffer) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_enqueue.EVPATH");

    return;
}


/* name.......: SOSD_cloud_fflush
 * description: Force the send-queue to flush and transmit.
 * note.......: With EVPath, this might be totally unnecessary.  (i.e. "Let EVPath handle it...")
 */
void  SOSD_cloud_fflush(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_fflush.EVPATH");
    return;
}


/* name.......: SOSD_cloud_finalize
 * description: Shut down the cloud operation, flush / close files, etc.
 */
int   SOSD_cloud_finalize(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_finalize.EVPATH");

    return 0;
}


/* name.......: SOSD_cloud_shutdown_notice
 * description: Send notifications to any daemon ranks that are not in the
 *              business of listening to the node on the SOS_CMD_PORT socket.
 *              Only certain daemon ranks participate/call this function.
 */
void  SOSD_cloud_shutdown_notice(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_shutdown_notice");
    return;
}


/* name.......: SOSD_cloud_listen_loop
 * description: When there is a feedback/control mechanism in place
 *              between the daemons and a heirarchical authority / policy
 *              enactment chain, this will be the loop that is monitoring
 *              incoming messages from other sosd daemon instances.
 */
void  SOSD_cloud_listen_loop(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_cloud_listen_loop");

    return;
}


