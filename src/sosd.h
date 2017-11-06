#ifndef SOSD_H
#define SOSD_H

#include <pthread.h>
#include <signal.h>
#include <time.h>

#ifdef SOSD_CLOUD_SYNC_WITH_MPI
#include <mpi.h>
#endif

#ifdef SOSD_CLOUD_SYNC_WITH_EVPATH
#include "evpath.h"
#endif

#ifdef USE_MUNGE
#include "munge.h"
#endif

#include "sos.h"
#include "sos_types.h"
#include "sosa.h"

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

//TODO: Remove unused define's...
#define SOSD_DEFAULT_K_MEAN_CENTERS  24
#define SOSD_DEFAULT_CENTROID_COUNT  12

#define SOSD_PUB_ANN_DIRTY           66
#define SOSD_PUB_ANN_LOCAL           77
#define SOSD_PUB_ANN_CLOUD           88

#define SOSD_LOCAL_SYNC_WAIT_SEC     0
#define SOSD_CLOUD_SYNC_WAIT_SEC     0
#define SOSD_DB_SYNC_WAIT_SEC        0
#define SOSD_SYSTEM_MONITOR_WAIT_SEC 1
#define SOSD_FEEDBACK_SYNC_WAIT_SEC  0

/* 0.05 seconds: 50000000, default for cloud/db=5000 */
#define SOSD_LOCAL_SYNC_WAIT_NSEC    1
#define SOSD_CLOUD_SYNC_WAIT_NSEC    1
#define SOSD_DB_SYNC_WAIT_NSEC       1
#define SOSD_FEEDBACK_SYNC_WAIT_NSEC 1



typedef struct {
    SOS_msg_type        type;
    void               *ref;
} SOSD_db_task;

typedef struct {
    SOS_feedback_type   type;
    void               *ref;
} SOSD_feedback_task;

typedef struct {
    char *handle;
    int   size;
    void *data;
} SOSD_feedback_payload;

typedef struct {
    SOS_query_state     state;
    SOS_guid            reply_to_guid;
    char               *query_sql;
    SOS_guid            query_guid;
    char               *reply_host;
    int                 reply_port;
    void               *results;
} SOSD_query_handle;

typedef struct {
    SOS_guid            reply_to_guid;
    char               *val_name;
    SOS_guid            req_guid;
    char               *reply_host;
    int                 reply_port;
    void               *results;
} SOSD_peek_handle;


typedef struct {
    SOS_guid            guid;
    char               *sense_handle;
    SOS_guid            client_guid;
    char               *remote_host;
    int                 remote_port;
    SOS_socket         *target;
    void               *next_entry;
} SOSD_sensitivity_entry;

typedef struct {
    pthread_mutex_t    *lock_stats;   
    uint64_t            thread_local_wakeup;   
    uint64_t            thread_cloud_wakeup;   
    uint64_t            thread_db_wakeup;
    uint64_t            thread_feedback_wakeup;
    uint64_t            feedback_checkin_messages;   
    uint64_t            socket_messages;       
    uint64_t            socket_bytes_recv;         
    uint64_t            socket_bytes_sent;        
    uint64_t            mpi_sends;            
    uint64_t            mpi_bytes;           
    uint64_t            db_transactions;      
    uint64_t            db_insert_announce;     
    uint64_t            db_insert_announce_nop;
    uint64_t            db_insert_publish;      
    uint64_t            db_insert_publish_nop;  
    uint64_t            db_insert_val_snaps;     
    uint64_t            db_insert_val_snaps_nop;  
    uint64_t            buffer_creates;      
    uint64_t            buffer_bytes_on_heap;   
    uint64_t            buffer_destroys;      
    uint64_t            pipe_creates;        
    uint64_t            pub_handles;          
} SOSD_counts;


typedef struct {
    SOS_guid            guid;
    double              x;
    double              y;
    void               *next;
} SOSD_km2d_point;


#ifdef SOSD_CLOUD_SYNC_WITH_EVPATH
typedef struct {
    bool                active;
    char                name[256];
    CManager            cm;
    char               *contact_string;
    attr_list           contact_list;
    EVsource            src;
    EVstone             out_stone;
    EVstone             rmt_stone;
} SOSD_evpath_node;

typedef struct {
    char               *instance_name;
    char               *instance_role;
    char               *meetup_path;
    int                 is_master;
    int                 node_count;
    SOSD_evpath_node  **node;
    char               *contact_string;
    attr_list           contact_list;
    SOSD_evpath_node    send;
    SOSD_evpath_node    recv;
} SOSD_evpath;
#endif

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
    int                 listener_count;
    int                 aggregator_count;
    SOSD_counts         countof;
#ifdef SOSD_CLOUD_SYNC_WITH_MPI
    MPI_Comm            comm;
#endif
#ifdef SOSD_CLOUD_SYNC_WITH_EVPATH
    SOSD_evpath         evpath;
#endif
} SOSD_runtime;

typedef struct {
    char               *file;
    int                 ready;
    pthread_mutex_t    *lock;
    SOS_pipe           *snap_queue;
} SOSD_db;

typedef struct {
    void                *sos_context;
    SOS_pipe            *queue;
    pthread_t           *handler;
    pthread_mutex_t     *lock;
    pthread_cond_t      *cond;
} SOSD_sync_context;

typedef struct {
    SOS_guid             guid;
    SOSD_km2d_point     *point_head;
    long                 point_count;
    SOSD_km2d_point     *centroid_head;
    int                  centroid_count;
} SOSD_km2d_tracker;


typedef struct {
    SOSD_sync_context    local;
    SOSD_sync_context    cloud_send;
    SOSD_sync_context    cloud_recv;
    SOSD_sync_context    db;
    SOSD_sync_context    system_monitor;
    SOSD_sync_context    feedback;
    qhashtbl_t          *km2d_table;
    pthread_mutex_t         *sense_list_lock;
    SOSD_sensitivity_entry  *sense_list_head;
} SOSD_sync_set;



typedef struct {
    SOS_runtime        *sos_context;
    SOSD_runtime        daemon;
    SOSD_db             db;
    SOS_socket         *net;
    SOS_uid            *guid;
    SOSD_sync_set       sync;
    qhashtbl_t         *pub_table;
    int                 system_monitoring;
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
    extern int   SOSD_cloud_start(void);
    extern int   SOSD_cloud_send(SOS_buffer *buffer, SOS_buffer *reply);
    extern void  SOSD_cloud_enqueue(SOS_buffer *buffer);
    extern void  SOSD_cloud_fflush(void);
    extern int   SOSD_cloud_finalize(void);
    extern void  SOSD_cloud_shutdown_notice(void);
    extern void  SOSD_cloud_listen_loop(void);
#endif

    void  SOSD_init(void);
    void  SOSD_setup_socket(void);

    void  SOSD_sync_context_init(SOS_runtime *sos_context,
            SOSD_sync_context *sync_context, size_t elem_size,
            void* (*thread_func)(void *thread_param));

    void* SOSD_THREAD_local_sync(void *args);
    void* SOSD_THREAD_cloud_send(void *args);
    void* SOSD_THREAD_cloud_recv(void *args);
    void* SOSD_THREAD_db_sync(void *args);
    void* SOSD_THREAD_system_monitor(void *args);
    void* SOSD_THREAD_feedback_sync(void *args);

    void  SOSD_listen_loop(void);
    void  SOSD_send_to_self(SOS_buffer *msg, SOS_buffer *reply);

    void  SOSD_handle_register(SOS_buffer *buffer);
    void  SOSD_handle_unregister(SOS_buffer *buffer);
    void  SOSD_handle_guid_block(SOS_buffer *buffer);
    void  SOSD_handle_announce(SOS_buffer *buffer);
    void  SOSD_handle_publish(SOS_buffer *buffer);
    void  SOSD_handle_echo(SOS_buffer *buffer);
    void  SOSD_handle_val_snaps(SOS_buffer *buffer);
    void  SOSD_handle_shutdown(SOS_buffer *buffer);
    void  SOSD_handle_check_in(SOS_buffer *buffer);
    void  SOSD_handle_probe(SOS_buffer *buffer);
    void  SOSD_handle_query(SOS_buffer *buffer);
    void  SOSD_handle_peek(SOS_buffer *buffer);
    void  SOSD_handle_sensitivity(SOS_buffer *buffer);
    void  SOSD_handle_desensitize(SOS_buffer *buffer);
    void  SOSD_handle_triggerpull(SOS_buffer *buffer);
    void  SOSD_handle_kmean_data(SOS_buffer *buffer);
    void  SOSD_handle_unknown(SOS_buffer *buffer);

    void  SOSD_claim_guid_block( SOS_uid *uid, int size,
            SOS_guid *pool_from, SOS_guid *pool_to );

    void  SOSD_apply_announce( SOS_pub *pub, SOS_buffer *buffer );
    void  SOSD_apply_publish( SOS_pub *pub, SOS_buffer *buffer );

    /* Private functions... see: sos.c */
    extern void SOS_uid_init( SOS_runtime *sos_context,
            SOS_uid **uid, SOS_guid from, SOS_guid to);

    
    /* Functions for monitoring system health */
    void SOSD_setup_system_data(void);
    void SOSD_read_system_data(void);
    void SOSD_add_pid_to_track(SOS_pub* pub_pid);



#ifdef __cplusplus
}
#endif


#define SOSD_countof(__stat__plus_or_minus__value) {                    \
        if (SOS_DEBUG > 0) {                                            \
            pthread_mutex_lock(SOSD.daemon.countof.lock_stats);         \
        }                                                               \
        SOSD.daemon.countof.__stat__plus_or_minus__value;               \
        if (SOS_DEBUG > 0) {                                            \
            pthread_mutex_unlock(SOSD.daemon.countof.lock_stats);       \
        }                                                               \
    }


#define SOSD_check_sync_saturation(__pub_mon) (((double) __pub_mon->ring->elem_count / (double) __pub_mon->ring->elem_max) > SOSD_RING_QUEUE_TRIGGER_PCT) ? 1 : 0

#define SOSD_PACK_ACK(__buffer) {                                       \
        if (__buffer == NULL) {                                         \
            dlog(0, "ERROR: You called SOSD_PACK_ACK() on a NULL buffer!  Terminating.\n"); \
            exit(EXIT_FAILURE);                                         \
        }                                                               \
        SOS_msg_header header;                           \
        int offset;                                      \
        dlog(7, "SOSD_PACK_ACK used to assemble a reply.\n");   \
        memset(&header, '\0', sizeof(SOS_msg_header));   \
        header.msg_size = -1;                            \
        header.msg_type = SOS_MSG_TYPE_ACK;              \
        header.msg_from = 0;                             \
        header.ref_guid = 0;                             \
        offset = 0;                                      \
        SOS_msg_zip(__buffer, header, 0, &offset);       \
        header.msg_size = offset;                        \
        offset = 0;                                      \
        SOS_msg_zip(__buffer, header, 0, &offset);       \
    }






#endif //SOS_SOSD_H
