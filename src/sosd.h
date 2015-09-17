#ifndef SOS_SOSD_H
#define SOS_SOSD_H

#include <pthread.h>
#include <signal.h>
#include <time.h>

#include "qhashtbl.h"


typedef struct {
    int   server_socket_fd;
    int   client_socket_fd;
    int   port_number;
    char *server_port;
    int   buffer_len;
    int   listen_backlog;
    int   client_len;
    struct addrinfo  server_hint;
    struct addrinfo *server_addr;
    char            *client_host;
    char            *client_port;
    struct addrinfo *result;
    struct sockaddr_storage   peer_addr;
    socklen_t   peer_addr_len;
} SOSD_net;

typedef struct {
    char   *work_dir;
    char   *db_file;
    int     db_ready;
    int     daemon_running;
    char    daemon_pid_str[256];
    double  time_now;
    SOS_uid         *guid;
    qhashtbl_t      *pub_table;
    SOS_ring_queue  *pub_ring;
    pthread_t       *pub_ring_t;
    pthread_cond_t  *pub_ring_ready;
    SOSD_net         net;
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

    void  SOSD_init();
    void  SOSD_setup_socket();
    void  SOSD_init_database();
    void  SOSD_init_pub_ring_monitor();
    void* SOSD_THREAD_pub_ring(void *args);
    static void SOSD_THREAD_pub_ring_timer(int sig, siginfo_t *sig_info, void *uc);

    void  SOSD_listen_loop();
    void  SOSD_handle_register(char *msg_data, int msg_size);
    void  SOSD_handle_announce(char *msg_data, int msg_size);
    void  SOSD_handle_publish(char *msg_data, int msg_size);
    void  SOSD_handle_echo(char *msg_data, int msg_size);
    void  SOSD_handle_shutdown(char *msg_data, int msg_size);
    void  SOSD_handle_unknown(char *msg_data, int msg_size);
    
    void  SOSD_apply_announce( SOS_pub *pub, char *msg, int msg_len );
    void  SOSD_apply_publish( SOS_pub *pub, char *msg, int msg_len );


#ifdef __cplusplus
}
#endif


#endif //SOS_SOSD_H
