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

#include "sos.h"
#include "sos_debug.h"
#include "sos_error.h"

#include "sosd.h"
#include "sosd_db_sqlite.h"
#include "qhashtbl.h"


#define USAGE "usage:   $ sosd --port <number> --buffer_len <bytes> --listen_backlog <len> [--work_dir <path>]"
#define DAEMON_NAME    "sosd"
#define DEFAULT_DIR    "/tmp"
#define LOCK_FILE      "sosd.lock"
#define LOG_FILE       "sosd.log"

#define GET_TIME(now)  { struct timeval t; gettimeofday(&t, NULL); now = t.tv_sec + t.tv_usec/1000000.0; }



int main(int argc, char *argv[])  {
    int elem, next_elem;
    int retval;

    SOSD.work_dir = (char *) &DEFAULT_DIR;
    SOS.role = SOS_ROLE_DAEMON;

    /*
     *  TODO:{ CHAD, INIT } Consider adding a --interactive mode that doesn't
     *  run as a daemon, displays statistics in real-time, and allows commands.
     */


    /* Process command-line arguments */
    if ( argc < 7 ) { fprintf(stderr, "%s\n", USAGE); exit(1); }
    SOSD.net.port_number    = -1;
    SOSD.net.buffer_len     = -1;
    SOSD.net.listen_backlog = -1;
    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) { fprintf(stderr, "%s\n", USAGE); exit(1); }
        if (      strcmp(argv[elem], "--port"            ) == 0) { SOSD.net.server_port    = argv[next_elem];       }
        else if ( strcmp(argv[elem], "--buffer_len"      ) == 0) { SOSD.net.buffer_len     = atoi(argv[next_elem]); }
        else if ( strcmp(argv[elem], "--listen_backlog"  ) == 0) { SOSD.net.listen_backlog = atoi(argv[next_elem]); }
        else if ( strcmp(argv[elem], "--work_dir"        ) == 0) { SOSD.work_dir           = argv[next_elem];       } /* optional */
        else    { fprintf(stderr, "Unknown flag: %s %s\n", argv[elem], argv[next_elem]); }
        elem = next_elem + 1;
    }
    SOSD.net.port_number = atoi(SOSD.net.server_port);
    if ( (SOSD.net.port_number < 1)
         || (SOSD.net.buffer_len < 1)
         || (SOSD.net.listen_backlog < 1) )
        { fprintf(stderr, "%s\n", USAGE); exit(1); }

    memset(&SOSD.daemon_pid_str, '\0', 256);

    /* System logging initialize */

    setlogmask(LOG_UPTO(LOG_ERR));
    openlog(DAEMON_NAME, LOG_CONS | LOG_NDELAY | LOG_PERROR | LOG_PID, LOG_DAEMON);
    syslog(LOG_INFO, "Starting daemon: %s", DAEMON_NAME);
    if (DAEMON_LOG) { sos_daemon_log_fptr = fopen(LOG_FILE, "w"); }

    printf("Calling daemon_init()...\n");
    SOS_daemon_init();
    printf("Calling SOS_init...\n");
    SOS_init( &argc, &argv, SOS.role );    
    SOS_SET_WHOAMI(whoami, "main");

    dlog(0, "[%s]: Calling daemon_setup_socket()...\n", whoami);
    SOS_daemon_setup_socket();
    dlog(0, "[%s]: Calling daemon_init_database()...\n", whoami);
    SOS_daemon_init_database();


    /* Go! */
    dlog(0, "[%s]: Calling daemon_listen_loop()...\n", whoami);
    SOS_daemon_listen_loop();
  


    /* Done!  Cleanup and shut down. */
    pthread_mutex_destroy( &(SOSD.guid->lock) );
    SOS_finalize();
    dlog(0, "[%s]: Closing the socket.\n", whoami);
    shutdown(SOSD.net.server_socket_fd, SHUT_RDWR);
    dlog(0, "[%s]: Exiting daemon's main() gracefully.\n", whoami);
    closelog();
    if (DAEMON_LOG) { fclose(sos_daemon_log_fptr); }
    close(sos_daemon_lock_fptr);
    remove(LOCK_FILE);
    
    return(EXIT_SUCCESS);
} //end: main()


/* -------------------------------------------------- */
void SOS_daemon_listen_loop() {
    SOS_SET_WHOAMI(whoami, "daemon_listen_loop");
    SOS_msg_header header;
    int      i, byte_count;

    char    *buffer;
    buffer = (char *) malloc(sizeof(char) * SOSD.net.buffer_len);

    dlog(0, "[%s]: Entering main loop...\n", whoami);
    while (SOSD.daemon_running) {
        memset(buffer, '\0', SOSD.net.buffer_len);
        memset(&header, '\0', sizeof(SOS_msg_header));
        byte_count = 0;

        dlog(5, "[%s]: Listening for a message...\n", whoami);
        SOSD.net.peer_addr_len = sizeof(SOSD.net.peer_addr);
        SOSD.net.client_socket_fd = accept(SOSD.net.server_socket_fd, (struct sockaddr *) &SOSD.net.peer_addr, &SOSD.net.peer_addr_len);
        i = getnameinfo((struct sockaddr *) &SOSD.net.peer_addr, SOSD.net.peer_addr_len, SOSD.net.client_host, NI_MAXHOST, SOSD.net.client_port, NI_MAXSERV, NI_NUMERICSERV);
        if (i != 0) { dlog(0, "[%s]: Error calling getnameinfo() on client connection.  (%s)\n", whoami, strerror(errno)); exit(EXIT_FAILURE); }

        byte_count = recv(SOSD.net.client_socket_fd, (void *) buffer, SOSD.net.buffer_len, 0);
        if (byte_count < 1) continue;

        if (byte_count >= sizeof(SOS_msg_header)) {
            memcpy(&header, buffer, sizeof(SOS_msg_header));
        } else {
            dlog(0, "[%s]:   ... Received short (useless) message.\n", whoami);  continue;
        }
        
        dlog(5, "[%s]: Received connection.\n", whoami);
        dlog(5, "[%s]:   ... byte_count = %d\n", whoami, byte_count);
        dlog(5, "[%s]:   ... msg_from = %ld\n", whoami, header.msg_from);

        switch (header.msg_type) {
        case SOS_MSG_TYPE_REGISTER: dlog(5, "[%s]:   ... msg_type = REGISTER (%d)\n", whoami, header.msg_type);
            SOS_daemon_handle_register(buffer, byte_count); break; 

        case SOS_MSG_TYPE_ANNOUNCE: dlog(5, "[%s]:   ... msg_type = ANNOUNCE (%d)\n", whoami, header.msg_type);
            SOS_daemon_handle_announce(buffer, byte_count); break;
            
        case SOS_MSG_TYPE_PUBLISH:  dlog(5, "[%s]:   ... msg_type = PUBLISH (%d)\n", whoami, header.msg_type);
            SOS_daemon_handle_publish(buffer, byte_count); break;
            
        case SOS_MSG_TYPE_ECHO:     dlog(5, "[%s]:   ... msg_type = ECHO (%d)\n", whoami, header.msg_type);
            SOS_daemon_handle_echo(buffer, byte_count); break;

        case SOS_MSG_TYPE_SHUTDOWN: dlog(5, "[%s]:   ... msg_type = SHUTDOWN (%d)\n", whoami, header.msg_type);
            SOS_daemon_handle_shutdown(buffer, byte_count); break;

        default:                    dlog(1, "[%s]:   ... msg_type = UNKNOWN (%d)\n", whoami, header.msg_type); break;
            SOS_daemon_handle_unknown(buffer, byte_count); break;
        }

        close( SOSD.net.client_socket_fd );
        
    }

    free(buffer);
    dlog(1, "[%s]: Leaving the socket listening loop.\n", whoami);

    return;
}
/* -------------------------------------------------- */




void SOS_daemon_handle_echo(char *msg, int msg_size) { 
    SOS_SET_WHOAMI(whoami, "daemon_handle_echo");
    SOS_msg_header header;
    int ptr = 0;
    int i   = 0;

    dlog(5, "[%s]: header.msg_type = SOS_MSG_TYPE_ECHO\n", whoami);
    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);

    i = send(SOSD.net.client_socket_fd, (void *) (msg + ptr), (msg_size - sizeof(SOS_msg_header)), 0);
    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }
        
    return;
}



void SOS_daemon_handle_register(char *msg, int msg_size) {
    SOS_SET_WHOAMI(whoami, "daemon_handle_register");
    SOS_msg_header header;
    int  ptr  = 0;
    int  i    = 0;
    long guid = 0;

    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);
    dlog(5, "[%s]: header.msg_type = SOS_MSG_TYPE_REGISTER\n", whoami);

    if (header.msg_from == 0) {
        guid = SOS_next_id( SOS.uid.seq );
    } else {
        guid = header.msg_from;
    }

    char response[SOS_DEFAULT_BUFFER_LEN];
    memset(response, '\0', SOS_DEFAULT_BUFFER_LEN);

    memcpy(response, &guid, sizeof(long));
    i = send( SOSD.net.client_socket_fd, (void *) response, sizeof(long), 0 );

    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }
    else {
        dlog(5, "[%s]:   ... send() returned the following bytecount: %d\n", whoami, i);
    }

    return;
}



void SOS_daemon_handle_announce(char *msg, int msg_size) {
    SOS_SET_WHOAMI(whoami, "daemon_handle_announce");
    SOS_msg_header header;
    int   ptr;
    int   i;
    long  guid;
    char *response;
    int   response_len;
    char  response_stack[SOS_DEFAULT_BUFFER_LEN];
    char  response_alloc;

    SOS_pub *pub;
    char     guid_str[SOS_DEFAULT_STRING_LEN];

    response = response_stack;
    response_alloc = 0;
    response_len = 0;

    /* Process the message into a pub handle... */

    ptr = 0;
    dlog(5, "[%s]: header.msg_type = SOS_MSG_TYPE_ANNOUNCE\n", whoami);

    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);
    guid = header.pub_guid;

    /* If this is a freshly announced pub, assign it a GUID. */
    if (guid < 1) { guid = SOS_next_id( SOS.uid.seq ); }
    memset(guid_str, '\0', SOS_DEFAULT_STRING_LEN);
    sprintf(guid_str, "%ld", guid);
    /* Check the table for this pub ... */
    dlog(5, "[%s]:    ... checking SOS.tbl for GUID(%s) --> ", whoami, guid_str);
    pub = (SOS_pub *) SOSD.tbl->get(SOSD.tbl, guid_str);
    if (pub == NULL) {
        dlog(5, "NOPE!  Adding new pub to the table.\n");
        /* If it's not in the table, add it. */
        pub = SOS_new_pub(guid_str);
        SOSD.tbl->put(SOSD.tbl, guid_str, pub);
        pub->guid = guid;
    } else {
        dlog(5, "FOUND IT!\n");
    }

    dlog(5, "[%s]: calling SOS_apply_announce() ...\n", whoami);


    SOS_apply_announce(pub, msg, msg_size);

    dlog(5, "[%s]:   ... pub->elem_count = %d\n", whoami, pub->elem_count);

    /* Supply the calling system with any needed GUID's... */

    response_len = (1 + pub->elem_count) * sizeof(long);

    if (response_len > SOS_DEFAULT_BUFFER_LEN) {
        response = (char *) malloc( response_len );
        if (response == NULL) { dlog(0, "[%s]: ERROR!  Could not allocate memory for an announcement response!  (%s)\n", whoami, strerror(errno));  exit(1); }
        memset (response, '\0', response_len);
        response_alloc = 1;
    }


    ptr = 0;
    for (i = 0; i < pub->elem_count; i++) {
        if (pub->data[i]->guid < 1) { guid = SOS_next_id( SOS.uid.seq ); pub->data[i]->guid = guid; } else { guid = pub->data[i]->guid; }
        dlog(5, "[%s]:       >   pub(%s)->data[%d]->guid = %ld\n", whoami, guid_str, i, guid);
        memcpy((response + ptr), &guid, sizeof(long));
        ptr += sizeof(long);
    }

    if (pub->guid < 1) { guid = SOS_next_id( SOS.uid.seq ); pub->guid = guid; } else { guid = pub->guid; }
    memcpy((response + ptr), &guid, sizeof(long));

    i = send( SOSD.net.client_socket_fd, (void *) response, response_len, 0);
    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }
    else { dlog(5, "[%s]:   ... send() returned the following bytecount: %d\n", whoami, i); }
    
    dlog(5, "[%s]:   ... Done.\n", whoami);

    return;
}



void SOS_daemon_handle_publish(char *msg, int msg_size)  {
    SOS_SET_WHOAMI(whoami, "daemon_handle_publish");
    SOS_msg_header header;
    long  guid = 0;
    int   ptr = 0;
    int   i   = 0;

    SOS_pub *pub;
    char     guid_str[SOS_DEFAULT_STRING_LEN];

    memset(guid_str, '\0', SOS_DEFAULT_STRING_LEN);

    dlog(5, "[%s]: header.msg_type = SOS_MSG_TYPE_PUBLISH\n", whoami);
    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);
    guid = header.pub_guid;

    /* If this is a freshly announced pub, assign it a GUID. */
    if (guid < 1) { guid = SOS_next_id( SOS.uid.seq ); }
    sprintf(guid_str, "%ld", guid);
    /* Check the table for this pub ... */
    dlog(5, "[%s]:   ... checking SOS.tbl for GUID(%s) --> ", whoami, guid_str);
    pub = (SOS_pub *) SOSD.tbl->get(SOSD.tbl, guid_str);
    if (pub == NULL) {
        /* If it's not in the table, add it. */
        dlog(5, "not found, ADDING new pub to the table.\n");
        pub = SOS_new_pub(guid_str);
        SOSD.tbl->put(SOSD.tbl, guid_str, pub);
        pub->guid = guid;
    } else {
        dlog(5, "FOUND it!\n");
    }

    SOS_apply_publish( pub, msg, msg_size );


    for (i = 0; i < pub->elem_count; i++) {
        switch (pub->data[i]->type) {
        case SOS_VAL_TYPE_INT:    dlog(5, "[%s]:       |   pub(%s)->data[%d]->val.i_val = %d\n", whoami, guid_str,     i, pub->data[i]->val.i_val);  break;
        case SOS_VAL_TYPE_LONG:   dlog(5, "[%s]:       |   pub(%s)->data[%d]->val.l_val = %ld\n", whoami, guid_str,    i, pub->data[i]->val.l_val);  break;
        case SOS_VAL_TYPE_DOUBLE: dlog(5, "[%s]:       |   pub(%s)->data[%d]->val.d_val = %lf\n", whoami, guid_str,    i, pub->data[i]->val.d_val);  break;
        case SOS_VAL_TYPE_STRING: dlog(5, "[%s]:       |   pub(%s)->data[%d]->val.c_val = \"%s\"\n", whoami, guid_str, i, pub->data[i]->val.c_val);  break;
        }
    }

    char response[SOS_DEFAULT_BUFFER_LEN];
    memset (response, '\0', SOS_DEFAULT_BUFFER_LEN);
    sprintf(response, "I received your PUBLISH!");

    i = send( SOSD.net.client_socket_fd, (void *) response, strlen(response), 0);
    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }
    else {
        dlog(5, "[%s]:   ... send() returned the following bytecount: %d\n", whoami, i);
    }

    dlog(5, "[%s]:   ... Done.\n", whoami);

    return;
}



void SOS_daemon_handle_shutdown(char *msg, int msg_size) {
    SOS_SET_WHOAMI(whoami, "daemon_handle_shutdown");
    SOS_msg_header header;
    int ptr = 0;
    int i   = 0;

    dlog(1, "[%s]: header.msg_type = SOS_MSG_TYPE_SHUTDOWN\n", whoami);
    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);

    char response[SOS_DEFAULT_BUFFER_LEN];
    memset ( response, '\0', SOS_DEFAULT_BUFFER_LEN );
    sprintf( response, "I received your SHUTDOWN!");

    i = send( SOSD.net.client_socket_fd, (void *) response, strlen(response), 0 );
    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }
    else { dlog(5, "[%s]:   ... send() returned the following bytecount: %d\n", whoami, i); }

    SOSD.daemon_running = 0;

    return;
}



void SOS_daemon_handle_unknown(char *msg, int msg_size) {
    SOS_SET_WHOAMI(whoami, "daemon_handle_unknown");
    SOS_msg_header header;
    int ptr = 0;
    int i   = 0;

    dlog(1, "[%s]: header.msg_type = UNKNOWN\n", whoami);
    memcpy(&header, (msg + ptr), sizeof(SOS_msg_header));  ptr += sizeof(SOS_msg_header);

    char response[SOS_DEFAULT_BUFFER_LEN];
    memset ( response, '\0', SOS_DEFAULT_BUFFER_LEN );
    sprintf( response, "SOS daemon did not understand your message!");

    i = send( SOSD.net.client_socket_fd, (void *) response, strlen(response), 0 );
    if (i == -1) { dlog(0, "[%s]: Error sending a response.  (%s)\n", whoami, strerror(errno)); }
    else { dlog(5, "[%s]:   ... send() returned the following bytecount: %d\n", whoami, i); }

    return;
}



void SOS_daemon_setup_socket() {
    SOS_SET_WHOAMI(whoami, "daemon_setup_socket");
    int i;
    int yes;
    int opts;

    yes = 1;

    memset(&SOSD.net.server_hint, '\0', sizeof(struct addrinfo));
    SOSD.net.server_hint.ai_family     = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
    SOSD.net.server_hint.ai_socktype   = SOCK_STREAM;   /* SOCK_STREAM vs. SOCK_DGRAM vs. SOCK_RAW */
    SOSD.net.server_hint.ai_flags      = AI_PASSIVE;    /* For wildcard IP addresses */
    SOSD.net.server_hint.ai_protocol   = 0;             /* Any protocol */
    SOSD.net.server_hint.ai_canonname  = NULL;
    SOSD.net.server_hint.ai_addr       = NULL;
    SOSD.net.server_hint.ai_next       = NULL;

    i = getaddrinfo(NULL, SOSD.net.server_port, &SOSD.net.server_hint, &SOSD.net.result);
    if (i != 0) { dlog(0, "[%s]: Error!  getaddrinfo() failed. (%s) Exiting daemon.\n", whoami, strerror(errno)); exit(EXIT_FAILURE); }

    for ( SOSD.net.server_addr = SOSD.net.result ; SOSD.net.server_addr != NULL ; SOSD.net.server_addr = SOSD.net.server_addr->ai_next ) {
        dlog(1, "[%s]: Trying an address...\n", whoami);

        SOSD.net.server_socket_fd = socket(SOSD.net.server_addr->ai_family, SOSD.net.server_addr->ai_socktype, SOSD.net.server_addr->ai_protocol );
        if ( SOSD.net.server_socket_fd < 1) {
            dlog(0, "[%s]:   ... failed to get a socket.  (%s)\n", whoami, strerror(errno));
            continue;
        }

        /*
         *  Allow this socket to be reused/rebound quickly by the daemon.
         */
        if ( setsockopt( SOSD.net.server_socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            dlog(0, "[%s]:   ... could not set socket options.  (%s)\n", whoami, strerror(errno));
            continue;
        }

        if ( bind( SOSD.net.server_socket_fd, SOSD.net.server_addr->ai_addr, SOSD.net.server_addr->ai_addrlen ) == -1 ) {
            dlog(0, "[%s]:   ... failed to bind to socket.  (%s)\n", whoami, strerror(errno));
            close( SOSD.net.server_socket_fd );
            continue;
        } 
        /* If we get here, we're good to stop looking. */
        break;
    }

    if ( SOSD.net.server_socket_fd < 0 ) {
        dlog(0, "[%s]:   ... could not socket/setsockopt/bind to anything in the result set.  last errno = (%d:%s)\n", whoami, errno, strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        dlog(0, "[%s]:   ... got a socket, and bound to it!\n", whoami);
    }

    freeaddrinfo(SOSD.net.result);

    /*
     *   Enforce that this is a BLOCKING socket:
     */
    opts = fcntl(SOSD.net.server_socket_fd, F_GETFL);
    if (opts < 0) { dlog(0, "[%s]: ERROR!  Cannot call fcntl() on the server_socket_fd to get its options.  Carrying on.  (%s)\n", whoami, strerror(errno)); }
 
    opts = opts & !(O_NONBLOCK);
    i    = fcntl(SOSD.net.server_socket_fd, F_SETFL, opts);
    if (i < 0) { dlog(0, "[%s]: ERROR!  Cannot use fcntl() to set the server_socket_fd to BLOCKING more.  Carrying on.  (%s).\n", whoami, strerror(errno)); }


    listen( SOSD.net.server_socket_fd, SOSD.net.listen_backlog );
    dlog(0, "[%s]: Listening on socket.\n", whoami);

    return;
}
 


void SOS_daemon_init_database() {

    SOS_db_init_database();
    SOS_db_create_tables();

    return;
}



void SOS_daemon_init() {
    SOS_SET_WHOAMI(whoami, "daemon_init");
    pid_t pid, sid;
    int rc;

    /* [fork]
     *     split off from the parent process (& terminate parent)
     */
    pid = fork();
    if (pid < 0) {
        fprintf(stderr, "Unable to start daemon (%s): Could not fork() off parent process.\n", DAEMON_NAME);
        exit(EXIT_FAILURE);
    }
    if (pid > 0) { exit(EXIT_SUCCESS); } //close the parent

    sprintf(SOSD.daemon_pid_str, "%d", getpid());
    fprintf(stderr, "Starting daemon (%s) with PID = %s\n", DAEMON_NAME, SOSD.daemon_pid_str);

    /* [child session]
     *     create/occupy independent session from parent process
     */
    umask(0);
    sid = setsid();
    if (sid < 0) {
        fprintf(stderr, "Unable to start daemon (%s): Could not acquire a session id.\n", DAEMON_NAME); 
        exit(EXIT_FAILURE);
    }
    if ((chdir(SOSD.work_dir)) < 0) {
        fprintf(stderr, "Unable to start daemon (%s): Could not change to working directory: %s\n", \
                DAEMON_NAME, SOSD.work_dir);
        exit(EXIT_FAILURE);
    }

    /* [lock file]
     *     create and hold lock file to prevent multiple daemon spawn
     */
    sos_daemon_lock_fptr = open(LOCK_FILE, O_RDWR | O_CREAT, 0640);
    if (sos_daemon_lock_fptr < 0) { 
        fprintf(stderr, "\nUnable to start daemon (%s): Could not access lock file %s in directory %s\n", \
                DAEMON_NAME, LOCK_FILE, SOSD.work_dir);
        exit(EXIT_FAILURE);
    }
    if (lockf(sos_daemon_lock_fptr, F_TLOCK, 0) < 0) {
        fprintf(stderr, "\nUnable to start daemon (%s): An instance is already running.\n", DAEMON_NAME);
        exit(EXIT_FAILURE);
    }
    rc = write(sos_daemon_lock_fptr, SOSD.daemon_pid_str, strlen(SOSD.daemon_pid_str));


    /* [file handles]
     *     close unused IO handles
     */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /* [signals]
     *     register the signals we care to trap
     */
    if (SOS_DEBUG) SOS_register_signal_handler();

    /* [guid's]
     *     configure the issuer of guids for this daemon
     */
    /* TODO: { GUID SOSD INIT } Make this sub-divide for groups of nodes. */
    SOSD.guid->next = 1;
    SOSD.guid->last = SOS_DEFAULT_UID_MAX;
    pthread_mutex_init( &(SOSD.guid->lock), NULL );


    /* [hashtable]
     *    storage system for received pubs.  (will enque their key -> db)
     */
    SOSD.tbl = qhashtbl(SOS_DEFAULT_TABLE_SIZE);


    SOSD.daemon_running = 1;
    return;
}



