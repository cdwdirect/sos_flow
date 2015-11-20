#ifndef SOSD_H
#define SOSD_H

#include <pthread.h>
#include <signal.h>
#include <time.h>

#include "qhashtbl.h"


/*********************/
/* [mode]
 *    1 = Fork into a new ID/SESSION...
 *    0 = Run interactively, as launched. (Good for certain MPI+MPMD setups)
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
#define SOSD_pack_ack(__buffer, __len_var) {             \
        SOS_msg_header header;                           \
        memset(&header, '\0', sizeof(header));           \
        header.msg_size = -1;                            \
        header.msg_type = SOS_MSG_TYPE_ACK;              \
        header.msg_from = 0;                             \
        header.pub_guid = 0;                             \
        *__len_var = 0;                                  \
        *__len_var += SOS_buffer_pack(__buffer, "iill",  \
                                      header.msg_size,   \
                                      header.msg_type,   \
                                      header.msg_from,   \
                                      header.pub_guid);  \
        SOS_buffer_pack(__buffer, "i", *__len_var);      \
    }


typedef struct {
    char               *name;
    SOS_ring_queue     *ring;
    pthread_t          *extract_t;
    pthread_cond_t     *extract_cond;
    pthread_mutex_t    *extract_lock;
    pthread_t          *commit_t;
    pthread_cond_t     *commit_cond;
    pthread_mutex_t    *commit_lock;
    long               *commit_list;
    int                 commit_count;
    SOS_target          commit_target;
    SOS_val_snap_queue *val_intake;
    SOS_val_snap_queue *val_outlet;
} SOSD_pub_ring_mon;

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
    SOSD_runtime        daemon;
    SOSD_db             db;
    SOSD_net            net;
    SOS_uid            *guid;
    SOSD_pub_ring_mon  *local_sync;
    SOSD_pub_ring_mon  *cloud_sync;
    SOS_async_buf_pair *cloud_bp;
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
    extern int   SOSD_cloud_send(unsigned char *msg, int msg_len);
    extern void  SOSD_cloud_enqueue(unsigned char *msg, int msg_len);
    extern void  SOSD_cloud_fflush(void);
    extern int   SOSD_cloud_finalize(void);
    extern void  SOSD_cloud_shutdown_notice(void);
    extern void  SOSD_cloud_listen_loop(void);
    extern void* SOSD_THREAD_cloud_flush(void *params);
#endif

    void  SOSD_init(void);
    void  SOSD_setup_socket(void);

    void  SOSD_init_pub_ring_monitor(void);
    void* SOSD_THREAD_pub_ring_list_extractor(void *args);
    void* SOSD_THREAD_pub_ring_storage_injector(void *args);
    void  SOSD_pub_ring_monitor_init(SOSD_pub_ring_mon **mon_var,
                                     char *name_var,
                                     SOS_ring_queue *ring_var,
                                     SOS_val_snap_queue *source_vals,
                                     SOS_val_snap_queue *target_vals,
                                     SOS_target target);
    void  SOSD_pub_ring_monitor_destroy(SOSD_pub_ring_mon *mon_var);

    void  SOSD_listen_loop(void);
    void  SOSD_handle_register(unsigned char *msg_data, int msg_size);
    void  SOSD_handle_guid_block(unsigned char *msg_data, int msg_size);
    void  SOSD_handle_announce(unsigned char *msg_data, int msg_size);
    void  SOSD_handle_publish(unsigned char *msg_data, int msg_size);
    void  SOSD_handle_echo(unsigned char *msg_data, int msg_size);
    void  SOSD_handle_val_snaps(unsigned char *msg, int msg_size);
    void  SOSD_handle_shutdown(unsigned char *msg_data, int msg_size);
    void  SOSD_handle_unknown(unsigned char *msg_data, int msg_size);

    void  SOSD_claim_guid_block( SOS_uid *uid, int size, long *pool_from, long *pool_to );
    void  SOSD_apply_announce( SOS_pub *pub, unsigned char *msg, int msg_len );
    void  SOSD_apply_publish( SOS_pub *pub, unsigned char *msg, int msg_len );

    extern void SOS_uid_init( SOS_uid **uid, long from, long to);


#ifdef __cplusplus
}
#endif


#endif //SOS_SOSD_H
