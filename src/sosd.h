#ifndef SOS_SOSD_H
#define SOS_SOSD_H


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
} SOS_daemon_net;


typedef struct {
    char   *work_dir;
    char   *db_file;
    int     daemon_running;
    char    daemon_pid_str[256];
    double  time_now;
    qhashtbl_t     *tbl;
    SOS_uid        *guid;
    SOS_daemon_net  net;
} SOS_daemon_runtime;


/* ----------
 *
 *  Daemon root 'global' data structure:
 */
SOS_daemon_runtime SOSD;


/* Required if included by C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

void SOS_daemon_init();
void SOS_daemon_setup_socket();
void SOS_daemon_init_database();
void SOS_daemon_listen_loop();
void SOS_daemon_handle_register(char *msg_data, int msg_size);
void SOS_daemon_handle_announce(char *msg_data, int msg_size);
void SOS_daemon_handle_publish(char *msg_data, int msg_size);
void SOS_daemon_handle_echo(char *msg_data, int msg_size);
void SOS_daemon_handle_shutdown(char *msg_data, int msg_size);
void SOS_daemon_handle_unknown(char *msg_data, int msg_size);

#ifdef __cplusplus
}
#endif


#endif //SOS_SOSD_H
