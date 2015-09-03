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

#include <sys/socket.h>
#include <netdb.h>

#define DAEMON_LOG     1

#include "sos.h"
#include "sos_debug.h"

#define USAGE "usage:   $ sosd --port <number> --buffer_len <bytes> --listen_backlog <len> [--work_dir <path>]"
#define DAEMON_NAME    "sosd"
#define DEFAULT_DIR    "/tmp"
#define LOCK_FILE      "sosd.lock"
#define LOG_FILE       "sosd.log"

#define dlog(level, ...);                                               \
    if ((SOS_DEBUG >= level) && DAEMON_LOG) {                           \
        fprintf(log_fptr, __VA_ARGS__);                                 \
        fflush(log_fptr);                                               \
    }

#define GET_TIME(now)  { struct timeval t; gettimeofday(&t, NULL); now = t.tv_sec + t.tv_usec/1000000.0; }

void signal_handler(int signal);

void daemon_init();
void daemon_setup_socket();
void daemon_listen_loop();
void daemon_handle_register(char *msg_data, int msg_size);
void daemon_handle_announce(char *msg_data, int msg_size);
void daemon_handle_publish(char *msg_data, int msg_size);
void daemon_handle_echo(char *msg_data, int msg_size);
void daemon_handle_shutdown(char *msg_data, int msg_size);




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
char   *server_port;
int     buffer_len;
int     listen_backlog;
int     client_len;
struct addrinfo           server_hint;
struct addrinfo          *server_addr;
char                     *client_host;
char                     *client_port;
struct addrinfo          *result;
struct sockaddr_storage   peer_addr;
socklen_t                 peer_addr_len;


int main(int argc, char *argv[])  {
    int elem, next_elem;

    WORK_DIR = &DEFAULT_DIR;
    SOS.role = SOS_ROLE_DAEMON;

    /*
     *  TODO:{ CHAD, INIT } Consider adding a --interactive mode that doesn't
     *  run as a daemon, displays statistics in real-time, and allows commands.
     */


    /* Process command-line arguments */
    if ( argc < 7 ) { fprintf(stderr, "%s\n", USAGE); exit(1); }
    port_number    = -1;
    buffer_len     = -1;
    listen_backlog = -1;
    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) { fprintf(stderr, "%s\n", USAGE); exit(1); }
        if (      strcmp(argv[elem], "--port"            ) == 0) { server_port    = argv[next_elem];       }
        else if ( strcmp(argv[elem], "--buffer_len"      ) == 0) { buffer_len     = atoi(argv[next_elem]); }
        else if ( strcmp(argv[elem], "--listen_backlog"  ) == 0) { listen_backlog = atoi(argv[next_elem]); }
        else if ( strcmp(argv[elem], "--work_dir"        ) == 0) { WORK_DIR       = argv[next_elem];       } /* optional */
        else    { fprintf(stderr, "Unknown flag: %s %s\n", argv[elem], argv[next_elem]); }
        elem = next_elem + 1;
    }
    port_number = atoi(server_port);
    if ( (port_number < 1)
         || (buffer_len < 1)
         || (listen_backlog < 1) )
        { fprintf(stderr, "%s\n", USAGE); exit(1); }

    memset(&daemon_pid_str, '\0', 256);

    /* System logging initialize */
    setlogmask(LOG_UPTO(LOG_ERR));
    openlog(DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Starting daemon: %s", DAEMON_NAME);
    if (DAEMON_LOG) { log_fptr = fopen(LOG_FILE, "w"); }

    printf(0, "Calling daemon_init()...\n");
    daemon_init();

    printf("Calling SOS_init...\n");
    SOS_init( &argc, &argv, SOS.role );
    
    SOS_SET_WHOAMI(whoami, "main");
    dlog(0, "[%s]: Returned from SOS_init();\n", whoami);

    dlog(0, "[%s]: Calling daemon_setup_socket()...\n", whoami);
    daemon_setup_socket();
    dlog(0, "[%s]: Calling daemon_listen_loop()...\n", whoami);
    daemon_listen_loop();

  
    //[cleanup]
    SOS_finalize();
    dlog(0, "[%s]: Exiting main() beneath the infinite loop.\n", whoami);
    closelog();
    if (DAEMON_LOG) { fclose(log_fptr); }
  
    return(EXIT_SUCCESS);
} //end: main()


/* -------------------------------------------------- */
void daemon_listen_loop() {
    SOS_SET_WHOAMI(whoami, "daemon_listen_loop");
    
    SOS_msg_header header;
    int      i, byte_count;
    char    *buffer;

    buffer = (char *) malloc(sizeof(char) * buffer_len);

    dlog(0, "[%s]: Entering main loop.   while(daemon_running) { ... }\n", whoami);


    while (daemon_running) {
        memset(buffer, '\0', buffer_len);
        memset(&header, '\0', sizeof(SOS_msg_header));
        byte_count = 0;
        
        peer_addr_len = sizeof(peer_addr);
        client_socket_fd = accept(server_socket_fd, (struct sockaddr *) &peer_addr, &peer_addr_len);
        i = getnameinfo((struct sockaddr *) &peer_addr, peer_addr_len, client_host, NI_MAXHOST, client_port, NI_MAXSERV, NI_NUMERICSERV);
        if (i != 0) { dlog(0, "[%s]: Error calling getnameinfo() on client connection.  (%s)\n", whoami, strerror(errno)); exit(EXIT_FAILURE); }

        byte_count = recvfrom( client_socket_fd, (void *) buffer, buffer_len, MSG_WAITALL, NULL, NULL); //(struct sockaddr *) &peer_addr, &peer_addr_len );
        if (byte_count < 1) continue;
        
        dlog(5, "[%s]: Received connection.\n", whoami);
        dlog(5, "[%s]:   ... byte_count = %d\n", whoami, byte_count);

        if (byte_count >= sizeof(SOS_msg_header)) {
            memcpy(&header, buffer, sizeof(SOS_msg_header));
        } else {
            dlog(0, "[%s]:   ... this appears to be a malformed message.  (Too short, discard)\n", whoami);  continue;
        }

        switch (header.msg_type) {
        case SOS_MSG_TYPE_REGISTER:
            dlog(5, "[%s]:   ... msg_type = REGISTER (%d)\n", whoami, header.msg_type);
            daemon_handle_register(buffer, byte_count); break; 

        case SOS_MSG_TYPE_ANNOUNCE:
            dlog(5, "[%s]:   ... msg_type = ANNOUNCE (%d)\n", whoami, header.msg_type);
            daemon_handle_announce(buffer, byte_count); break;
            
        case SOS_MSG_TYPE_PUBLISH:
            dlog(5, "[%s]:   ... msg_type = PUBLISH (%d)\n", whoami, header.msg_type);
            daemon_handle_publish(buffer, byte_count); break;
            
        case SOS_MSG_TYPE_ECHO:
            dlog(5, "[%s]:   ... msg_type = ECHO (%d)\n", whoami, header.msg_type);
            daemon_handle_echo(buffer, byte_count); break;

        case SOS_MSG_TYPE_SHUTDOWN:
            dlog(5, "[%s]:   ... msg_type = SHUTDOWN (%d)\n", whoami, header.msg_type);
            daemon_handle_shutdown (buffer, byte_count); break;

        default:
            dlog(1, "[%s]:   ... msg_type = UNKNOWN (%d)  (doing nothing)\n", whoami, header.msg_type); break;
        }

        close( client_socket_fd );
        
    }

    free(buffer);
   
    return;
}
/* -------------------------------------------------- */




void daemon_handle_echo(char *msg, int msg_size) { 
    SOS_SET_WHOAMI(whoami, "daemon_handle_echo");
    SOS_msg_header header;
    int ptr = 0;
    int i   = 0;

    dlog(5, "[%s]: header.msg_type = SOS_MSG_TYPE_ECHO\n", whoami);
    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);
    i = sendto( client_socket_fd, (void *) (msg + ptr), (msg_size - sizeof(SOS_msg_header)), NULL, NULL, NULL); //(struct sockaddr *) &peer_addr, peer_addr_len);
    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }
        
    return;
}



void daemon_handle_register(char *msg, int msg_size) {
    SOS_SET_WHOAMI(whoami, "daemon_handle_register");
    SOS_msg_header header;
    int ptr = 0;
    int i   = 0;

    dlog(5, "[%s]: header.msg_type = SOS_MSG_TYPE_REGISTER\n", whoami);
    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);

    char response[] = "SOS daemon got your register!";
    i = sendto( client_socket_fd, (void *) response, strlen(response), NULL, NULL, NULL);
    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }

    return;
}



void daemon_handle_announce(char *msg, int msg_size) {
    SOS_SET_WHOAMI(whoami, "daemon_handle_announce");
    SOS_msg_header header;
    int ptr = 0;
    int i   = 0;

    SOS_pub *new_pub;
    new_pub = SOS_new_post("...");
    
    dlog(5, "[%s]: header.msg_type = SOS_MSG_TYPE_ANNOUNCE\n", whoami);
    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);

    dlog(5, "[%s]: calling SOS_apply_announce() ...\n", whoami);
    SOS_apply_announce(new_pub, (msg + ptr), (msg_size - ptr));
    dlog(5, "[%s]:   ... new_pub->elem_count = %d\n", whoami, new_pub->elem_count);


    /* TODO:{ HANDLE_ANNOUNCE } Here we need to build a reply featuring the GUIDs assigned. */

    char response[] = "SOS daemon got your announcement!";
    i = sendto( client_socket_fd, (void *) response, strlen(response), NULL, NULL, NULL);
    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }

    
    return;
}



void daemon_handle_publish(char *msg, int msg_size)  {
    SOS_SET_WHOAMI(whoami, "daemon_handle_publish");
    SOS_msg_header header;
    int ptr = 0;
    int i   = 0;

    dlog(5, "[%s]: header.msg_type = SOS_MSG_TYPE_PUBLISH\n", whoami);
    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);

    char response[] = "SOS daemon got your publish!";
    i = sendto( client_socket_fd, (void *) response, strlen(response), NULL, NULL, NULL);
    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }



    return;
}


/*

12
11  
10  
9
8:30 **
7
6:30

 */





void daemon_handle_shutdown(char *msg, int msg_size) {
    SOS_SET_WHOAMI(whoami, "daemon_handle_shutdown");
    SOS_msg_header header;
    int ptr = 0;
    int i   = 0;

    dlog(1, "[%s]: header.msg_type = SOS_MSG_TYPE_SHUTDOWN\n", whoami);
    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);

    char response[] = "SOS daemon is shutting down.";    
    i = sendto( client_socket_fd, (void *) response, strlen(response), NULL, NULL, NULL);
    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }

    daemon_running = 0;

    return;
}



void daemon_setup_socket() {
    SOS_SET_WHOAMI(whoami, "daemon_setup_socket");
    int i;
    int yes;
    int opts;

    yes = 1;

    memset(&server_hint, '\0', sizeof(struct addrinfo));
    server_hint.ai_family     = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    server_hint.ai_socktype   = SOCK_STREAM;   /* SOCK_STREAM vs. SOCK_DGRAM vs. SOCK_RAW */
    server_hint.ai_flags      = AI_PASSIVE;    /* For wildcard IP addresses */
    server_hint.ai_protocol   = 0;             /* Any protocol */
    server_hint.ai_canonname  = NULL;
    server_hint.ai_addr       = NULL;
    server_hint.ai_next       = NULL;

    i = getaddrinfo(NULL, server_port, &server_hint, &result);
    if (i != 0) { dlog(0, "[%s]: Error!  getaddrinfo() failed. (%s) Exiting daemon.\n", whoami, strerror(errno)); exit(EXIT_FAILURE); }

    for ( server_addr = result ; server_addr != NULL ; server_addr = server_addr->ai_next ) {
        dlog(1, "[%s]: Trying an address...\n", whoami);

        server_socket_fd = socket(server_addr->ai_family, server_addr->ai_socktype, server_addr->ai_protocol );
        if ( server_socket_fd < 1) {
            dlog(0, "[%s]:   ... failed to get a socket.  (%s)\n", whoami, strerror(errno));
            continue;
        }

        /*
         *  Allow this socket to be reused by other tasks (not always good, can mask zombie daemons)
         *
        if ( setsockopt( server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            dlog(0, "[%s]:   ... could not set socket options.  (%s)\n", whoami, strerror(errno));
            continue;
        }
         *
         */

        if ( bind( server_socket_fd, server_addr->ai_addr, server_addr->ai_addrlen ) == -1 ) {
            dlog(0, "[%s]:   ... failed to bind to socket.  (%s)\n", whoami, strerror(errno));
            close( server_socket_fd );
            continue;
        } 

        /* If we get here, we're good to go! */
        break;
    }

    if ( server_socket_fd == NULL ) {
        dlog(0, "[%s]:   ... could not socket/setsockopt/bind to anything in the result set.  last errno = (%d:%s)\n", whoami, errno, strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        dlog(0, "[%s]:   ... got a socket, and bound to it!\n", whoami);
    }

    freeaddrinfo(result);

    /*
     *   Enforce that this is a BLOCKING socket:
     */

    opts = fcntl(server_socket_fd, F_GETFL);
    if (opts < 0) { dlog(0, "[%s]: ERROR!  Cannot call fcntl() on the server_socket_fd to get its options.  Expect strangeness.\n", whoami); }
    
    opts = opts & !(O_NONBLOCK);
    i    = fcntl(server_socket_fd, F_SETFL, opts);
    if (i < 0) { dlog(0, "[%s]: ERROR!  Cannot use fcntl() to set the server_socket_fd to BLOCKING more.  Expect strangeness.\n", whoami); }

    /*
     *
     */

    listen( server_socket_fd, listen_backlog );
    dlog(0, "[%s]: Listening on socket.\n", whoami);

    return;
}
 


void daemon_init() {
    SOS_SET_WHOAMI(whoami, "daemon_init");
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
    #ifndef DEBUG
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    #endif

    /* [signals]
     *     register the signals we care to trap
     */

    signal(SIGCHLD, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP, signal_handler);
    signal(SIGTERM, signal_handler);

    daemon_running = 1;
    return;
} //end: daemon_init



void signal_handler(int signal) {
    SOS_SET_WHOAMI(whoami, "signal_handler");

    switch (signal) {
    case SIGHUP:
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
        dlog(0, "[%s]: Caught SIGTERM, shutting down.\n", whoami);
        break;

    }

} //end: signal_handler

