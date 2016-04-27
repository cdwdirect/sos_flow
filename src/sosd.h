#ifndef SOSD_H
#define SOSD_H

#include <pthread.h>
#include <signal.h>
#include <time.h>

#include "sos.h"
#include "sos_types.h"


/*********************/
/* [SOSD_DAEMON_MODE]
 *    1 = Fork into a new ID/SESSION...
 *    0 = Run interactively, as launched. (Good for most MPI+MPMD setups)
 *
#define SOSD_DAEMON_MODE             0
 *********************/

#define SOSD_DAEMON_NAME             "sosd"
#define SOSD_DEFAULT_DIR             "/tmp"
#define SOSD_DEFAULT_LOCK_FILE       "sosd.lock"
#define SOSD_DEFAULT_LOG_FILE        "sosd.log"
#define SOSD_RING_QUEUE_TRIGGER_PCT  0.7

#define SOSD_PUB_ANN_DIRTY           66
#define SOSD_PUB_ANN_LOCAL           77
#define SOSD_PUB_ANN_CLOUD           88

#define SOSD_check_sync_saturation(__pub_mon) (((double) __pub_mon->ring->elem_count / (double) __pub_mon->ring->elem_max) > SOSD_RING_QUEUE_TRIGGER_PCT) ? 1 : 0

#define SOSD_PACK_ACK(__buffer) {                        \
        SOS_msg_header header;                           \
        int offset;                                      \
        memset(&header, '\0', sizeof(header));           \
        header.msg_size = -1;                            \
        header.msg_type = SOS_MSG_TYPE_ACK;              \
        header.msg_from = 0;                             \
        header.pub_guid = 0;                             \
        offset = 0;                                      \
        SOS_buffer_pack(__buffer, &offset, "iigg",       \
                                      header.msg_size,   \
                                      header.msg_type,   \
                                      header.msg_from,   \
                                      header.pub_guid);  \
        header.msg_size = offset;                        \
        offset = 0;                                      \
        SOS_buffer_pack(__buffer, &offset, "i",          \
                        header.msg_size);                \
    }



typedef struct {
    int                 server_socket_fd;
    int                 client_socket_fd;
    int                 port_number;
    char               *server_port;
    int                 buffer_len;
    int                 listen_backlog;
    int                 client_len;
    struct addrinfo     server_hint;
    struct addrinfo    *server_addr;
    char               *client_host;
    char               *client_port;
    struct addrinfo    *result;
    struct sockaddr_storage   peer_addr;
    socklen_t           peer_addr_len;
} SOSD_net;

typedef struct {
    char               *work_dir;
    char               *lock_file;
    char               *log_file;
    char               *name;
    int                 running;
    char                pid_str[256];
    int                *cloud_sync_target_set;
    int                 cloud_sync_target_count;
    int                 cloud_sync_target;
} SOSD_runtime;

typedef struct {
    char               *file;
    int                 ready;
    pthread_mutex_t    *lock;
} SOSD_db;

typedef struct {
    void                *sos_context;
    SOS_pipe            *queue;
    pthread_t           *handler;
    pthread_mutex_t     *lock;
    pthread_cond_t      *cond;
} SOSD_sync_context;

typedef struct {
    SOSD_sync_context    local;
    SOSD_sync_context    cloud;
} SOSD_sync_set;

typedef struct {
    SOS_runtime        *sos_context;
    SOSD_runtime        daemon;
    SOSD_db             db;
    SOSD_net            net;
    SOS_uid            *guid;
    SOSD_sync_set       sync;
    qhashtbl_t         *pub_table;
} SOSD_global;

/* ----------
 *
 *  Daemon root 'global' data structure:
 */
SOSD_global SOSD;


/* Required if included by C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef SOSD_CLOUD_SYNC
    /* All cloud_sync modules must have the following signatures: */
    extern int   SOSD_cloud_init(int *argc, char ***argv);
    extern int   SOSD_cloud_send(SOS_buffer *buffer);
    extern void  SOSD_cloud_enqueue(SOS_buffer *buffer);
    extern void  SOSD_cloud_fflush(void);
    extern int   SOSD_cloud_finalize(void);
    extern void  SOSD_cloud_shutdown_notice(void);
    extern void  SOSD_cloud_listen_loop(void);
#endif

    void  SOSD_init(void);
    void  SOSD_setup_socket(void);

    void  SOSD_sync_context_init(SOS_runtime *sos_context,
                                 SOSD_sync_context *sync_context,
                                 size_t elem_size,
                                 void* (*thread_func)(void *thread_param));

    void* SOSD_THREAD_local_sync(void *args);
    void* SOSD_THREAD_cloud_sync(void *args);

    void  SOSD_listen_loop(void);
    void  SOSD_handle_register(SOS_buffer *buffer);
    void  SOSD_handle_guid_block(SOS_buffer *buffer);
    void  SOSD_handle_announce(SOS_buffer *buffer);
    void  SOSD_handle_publish(SOS_buffer *buffer);
    void  SOSD_handle_echo(SOS_buffer *buffer);
    void  SOSD_handle_val_snaps(SOS_buffer *buffer);
    void  SOSD_handle_shutdown(SOS_buffer *buffer);
    void  SOSD_handle_check_in(SOS_buffer *buffer);
    void  SOSD_handle_unknown(SOS_buffer *buffer);

    void  SOSD_claim_guid_block( SOS_uid *uid, int size, long *pool_from, long *pool_to );
    void  SOSD_apply_announce( SOS_pub *pub, SOS_buffer *buffer );
    void  SOSD_apply_publish( SOS_pub *pub, SOS_buffer *buffer );

    /* Private functions... see: sos.c */
    extern void SOS_uid_init( SOS_runtime *sos_context, SOS_uid **uid, SOS_guid from, SOS_guid to);
    extern SOS_runtime* SOS_init_with_runtime(int *argc, char ***argv, SOS_role role, SOS_layer layer, SOS_runtime *extant_sos_runtime);


#ifdef __cplusplus
}
#endif


#endif //SOS_SOSD_H
