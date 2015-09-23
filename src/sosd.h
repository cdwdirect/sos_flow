#ifndef SOSD_H
#define SOSD_H

#include <pthread.h>
#include <signal.h>
#include <time.h>

#include "qhashtbl.h"

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
    SOS_role            commit_target;
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
    char               *db_file;
    int                 db_ready;
    int                 daemon_running;
    char                daemon_pid_str[256];
    double              time_now;
    SOS_uid            *guid;
    qhashtbl_t         *pub_table;
    SOSD_pub_ring_mon  *local_sync;
    SOSD_pub_ring_mon  *cloud_sync;
    SOSD_net            net;
} SOSD_runtime;


/* ----------
 *
 *  Daemon root 'global' data structure:
 */
SOSD_runtime SOSD;


/* Required if included by C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

#ifdef SOSD_CLOUD_SYNC
    /* All cloud_sync modules must have the following signatures: */
    extern int SOSD_cloud_init(int *argc, char ***argv);
    extern int SOSD_cloud_send(char *msg, int msg_len);
    extern int SOSD_cloud_finalize();
    /* TODO:{ CLOUD_SYNC } Add signature for queries / callbacks.  This is more advanced, so ... do last. */
#endif

    void  SOSD_init();
    void  SOSD_setup_socket();
    void  SOSD_init_database();

    void  SOSD_init_pub_ring_monitor();
    void* SOSD_THREAD_pub_ring_list_extractor(void *args);
    void* SOSD_THREAD_pub_ring_storage_injector(void *args);
    void  SOSD_pub_ring_monitor_init(SOSD_pub_ring_mon **mon_var, char *name_var, SOS_ring_queue *ring_var, SOS_role target_var);
    void  SOSD_pub_ring_monitor_destroy(SOSD_pub_ring_mon *mon_var);

    void  SOSD_listen_loop();
    void  SOSD_handle_register(char *msg_data, int msg_size);
    void  SOSD_handle_guid_block(char *msg_data, int msg_size);
    void  SOSD_handle_announce(char *msg_data, int msg_size);
    void  SOSD_handle_publish(char *msg_data, int msg_size);
    void  SOSD_handle_echo(char *msg_data, int msg_size);
    void  SOSD_handle_shutdown(char *msg_data, int msg_size);
    void  SOSD_handle_unknown(char *msg_data, int msg_size);

    void  SOSD_claim_guid_block( SOS_uid *uid, int size, long *pool_from, long *pool_to );
    void  SOSD_apply_announce( SOS_pub *pub, char *msg, int msg_len );
    void  SOSD_apply_publish( SOS_pub *pub, char *msg, int msg_len );

    extern void SOS_uid_init( SOS_uid **uid, long from, long to);


#ifdef __cplusplus
}
#endif


#endif //SOS_SOSD_H
