/*
 *  sosd.c (daemon)
 *
 *
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>

#include "sos.h"


#define USAGE "usage:   $ sosd --port <number> --buffer_len <bytes> --listen_backlog <len> [--work_dir <path>]"
#define DAEMON_NAME    "sosd"
#define DEFAULT_DIR    "/tmp"
#define LOCK_FILE      "sosd.lock"
#define LOG_FILE       "sosd.log"
#define DAEMON_LOG     1
#define GET_TIME(now)  { struct timeval t; gettimeofday(&t, NULL); now = t.tv_sec + t.tv_usec/1000000.0; }
#define dlog(msg)                                                       \
    if (DAEMON_LOG) {                                                   \
        GET_TIME(time_now);                                             \
        fprintf(log_fptr, "[%s:%s @ %f] %s",                            \
                DAEMON_NAME, daemon_pid_str, time_now, msg);            \
        fflush(log_fptr);                                               \
    }


void daemon_init();
void signal_handler(int signal);


/*--- Daemon management variables ---*/
char   *WORK_DIR;
int     daemon_running = 0;
int     lock_fptr;
FILE*   log_fptr;
char    daemon_pid_str[256];
double  time_now = 0.0;

/*--- Networking variables ---*/
int     server_socket_fd;
int     client_socket_fd;
int     port_number;
char   *buffer;
int     buffer_len;
int     listen_backlog;
int     client_len;
struct sockaddr_in server_addr;
struct sockaddr_in client_addr;

int main(int argc, char *argv[]) {
    int i, elem, next_elem;

    WORK_DIR = &DEFAULT_DIR;

    /* Process command-line arguments */
    port_number    = -1;
    buffer_len     = -1;
    listen_backlog = -1;
    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) { fprintf(stderr, "%s\n", USAGE); exit(1); }
        if (      strcmp(argv[elem], "--port"            ) == 0) { port_number    = atoi(argv[next_elem]); }
        else if ( strcmp(argv[elem], "--buffer_len"      ) == 0) { buffer_len     = atoi(argv[next_elem]); }
        else if ( strcmp(argv[elem], "--listen_backlog"  ) == 0) { listen_backlog = atoi(argv[next_elem]); }
        else if ( strcmp(argv[elem], "--work_dir"        ) == 0) { WORK_DIR       = argv[next_elem];       } /* optional */
        else    { fprintf(stderr, "Unknown flag: %s %s\n", argv[elem], argv[next_elem]); }
        elem = next_elem + 1;
    }
    if ( (port_number < 1)
         || (buffer_len < 1)
         || (listen_backlog < 1) )
        { fprintf(stderr, "%s\n", USAGE); exit(1); }

    memset(&daemon_pid_str, '\0', 256);

    daemon_init();
    daemon_running = 1;
     

    /* System logging initialize */
    setlogmask(LOG_UPTO(LOG_ERR));
    openlog(DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Starting daemon: %s", DAEMON_NAME);
    if (DAEMON_LOG) { log_fptr = fopen(LOG_FILE, "w"); }
    dlog("Daemon logging is on-line.\n");





    /* ----- DAEMON CODE BEGINS HERE --------------------- */
        
    server_socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family       = AF_INET;
    server_addr.sin_addr.s_addr  = INADDR_ANY;
    server_addr.sin_port         = htons(port_number);
    if (bind(server_socket_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0) {
        syslog(LOG_INFO, "ERROR attempting to bind socket (%d).\n", port_number); closelog();
        dlog("ERROR attempting to bind socket.\n");
        exit(1);
    }
    listen(server_socket_fd, listen_backlog);
    client_len = sizeof(client_addr);
    buffer = (char *) malloc(sizeof(char) * buffer_len);

    dlog("Socket open.  Listening....\n");    

    while (daemon_running) {

        /* ACCEPT CLIENT */
        client_socket_fd = accept(server_socket_fd, (struct sockaddr *) &client_addr, &client_len);
        if ( client_socket_fd < 0 ) {
            syslog(LOG_INFO, "ERROR attempting to accept client on server socket.\n");
            dlog("ERROR attempting to accept client on server socket.\n"); continue; }

        /* READ MESSAGE */
        memset(buffer, '\0', buffer_len);
        if ( read(client_socket_fd, buffer, buffer_len) < 0 ) {
            syslog(LOG_INFO, "ERROR attempting to read from client socket.\n");
            dlog("ERROR attempting to read from client socket.\n"); continue; }

        dlog(buffer);  /* Log what we received. */

        /* WRITE REPLY */
        if ( write(client_socket_fd, "ACK\0", 4) < 0 ) {
            syslog(LOG_INFO, "ERROR attempting to write to client socket.\n");
            dlog("ERROR attempting to write to client socket.\n"); }

        close(client_socket_fd);
    }

    close(server_socket_fd);
    free(buffer);

  
   
    /* -------------------------------------------------- */




  
    //[cleanup]
    dlog("Exiting main() beneath the infinite loop.\n");
    closelog();
    if (DAEMON_LOG) { fclose(log_fptr); }
  
    return(EXIT_SUCCESS);
} //end: main()




void daemon_init() {
    pid_t pid, sid;

    /* [fork]
     *     split off from the parent process (& terminate parent)
     */
    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Unable to start daemon (%s): Could not fork() off parent process.\n", DAEMON_NAME);
        exit(EXIT_FAILURE);
    }
    if (pid > 0) { exit(EXIT_SUCCESS); } //close the parent

    sprintf(daemon_pid_str, "%d", getpid());
    fprintf(stderr, "Starting daemon (%s) with PID = %s\n", DAEMON_NAME, daemon_pid_str);

    /* [child session]
     *     create/occupy independent session from parent process
     */
    umask(0);
    sid = setsid();
    if (sid < 0) {
        fprintf(stderr, "Unable to start daemon (%s): Could not acquire a session id.\n", DAEMON_NAME); 
        exit(EXIT_FAILURE);
    }
    if ((chdir(WORK_DIR)) < 0) {
        fprintf(stderr, "Unable to start daemon (%s): Could not change to working directory: %s\n", \
                DAEMON_NAME, WORK_DIR);
        exit(EXIT_FAILURE);
    }

    /* [lock file]
     *     create and hold lock file to prevent multiple daemon spawn
     */
    lock_fptr = open(LOCK_FILE, O_RDWR | O_CREAT, 0640);
    if (lock_fptr < 0) { 
        fprintf(stderr, "\nUnable to start daemon (%s): Could not access lock file %s in directory %s\n", \
                DAEMON_NAME, LOCK_FILE, WORK_DIR);
        exit(EXIT_FAILURE);
    }
    if (lockf(lock_fptr, F_TLOCK, 0) < 0) {
        fprintf(stderr, "\nUnable to start daemon (%s): An instance is already running.\n", DAEMON_NAME);
        exit(EXIT_FAILURE);
    }
    write(lock_fptr, daemon_pid_str, strlen(daemon_pid_str));


    /* [file handles]
     *     close unused IO handles
     */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /* [signals]
     *     register the signals we care to trap
     */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);

    return;
} //end: daemon_init



void signal_handler(int signal) {
    switch (signal) {

    case SIGHUP:
        /*  Attempt to refresh network connections
         *  NOTE: some systems use SIGHUP to reset servers...
         */
        syslog(LOG_DEBUG, "SIGHUP signal caught.");
        break;

    case SIGTERM:
        /* Future-proofing for non-blocking socket accept calls... */
        daemon_running = 0;

        /* [shutdown]
         *     close logs to write them to disk.
         */
        syslog(LOG_DEBUG, "SIGTERM signal caught.");
        syslog(LOG_INFO, "Shutting down.\n");
        closelog();
        dlog("Caught SIGTERM, shutting down.\n");
        if (DAEMON_LOG) { fclose(log_fptr); }

        /* TODO: { NON-BLOCKING ACCEPT = remove exit(0); } */
        exit(0);
        break;

    }
} //end: signal_handler






/*
 * // [MPI NOTES]
 *
 * // ----- [MPI Templates]:

 int MPI_Send(void* MSG_BUF, int MSG_SIZE, MPI_Datatype MSG_TYPE, int DEST, int TAG, MPI_Comm COMM);
 int MPI_Recv(void* MSG_BUF, int BUF_SIZE, MPI_Datatype MSG_TYPE, int SOURCE, int TAG, MPI_Comm COMM, MPI_Status* STATUS);


 // ----- [MPI Types]:
 //  MPI_CHAR   MPI_SHORT   MPI_INT   MPI_LONG   MPI_LONG_LONG MPI_UNSIGNED[_CHAR, _SHORT, _LONG]
 //  MPI_FLOAT  MPI_DOUBLE  MPI_LONG_DOUBLE MPI_BYTE   MPI_PACKED

 // ----- [Manually buffered MPI]:

 char buffer[1000];
 char *buf;
 int buf_size;
 MPI_Buffer_attach(buffer, 1000);      //Only one buffer can be attached at a time.
 // ...
 // Calls to MPI_BSend() ...
 // ...
 MPI_Buffer_detach(&buf, &buf_size);

 // ----- [Calculate buffer size]:

 int data_size;
 int message_size;
 int max_rapid_messages;
 int bcast_buffer_size;
 int generous_buf_size;

 MPI_Pack_size(1, MPI_INT, comm, &data_size);
 message_size = data_size + MPI_BSEND_OVRHEAD;
 bcast_buf_size = (comm_sz - 1) * message_size;
 generous_buf_size = bcast_buf_size * max_rapid_messages;

 *
 */
