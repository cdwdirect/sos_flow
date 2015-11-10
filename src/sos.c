
/*
 * sos.c                 SOS library routines
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <sys/socket.h>
#include <netdb.h>

#include "sos.h"
#include "sos_debug.h"
#include "pack_buffer.h"
#include "qhashtbl.h"

/* Private functions (not in the header file) */
void*  SOS_THREAD_post( void *arg );
void*  SOS_THREAD_read( void *arg );
void*  SOS_THREAD_scan( void *arg );

void   SOS_expand_data( SOS_pub *pub );

void   SOS_uid_init( SOS_uid **uid, long from, long to );
long   SOS_uid_next( SOS_uid *uid );
void   SOS_uid_destroy( SOS_uid *uid );

/* Doesn't lock the queue, for use within queue functions. */
void   SOS_val_snap_queue_drain(SOS_val_snap_queue *queue, SOS_pub *pub);


/* Private variables (not exposed in the header file) */
int   SOS_NULL_STR_LEN  = sizeof(char);
char  SOS_NULL_STR_CHAR = '\0';
char *SOS_NULL_STR      = &SOS_NULL_STR_CHAR;


/* **************************************** */
/* [util]                                   */
/* **************************************** */

void SOS_init( int *argc, char ***argv, SOS_role role ) {
    SOS_msg_header header;
    char buffer[SOS_DEFAULT_ACK_LEN];
    int i, n, retval, server_socket_fd;
    long guid_pool_from;
    long guid_pool_to;

    if (role == SOS_ROLE_CLIENT) { memset(&SOS, '\0', sizeof(SOS_runtime)); }

    SOS.role = role;
    SOS.status = SOS_STATUS_INIT;

    SOS_SET_WHOAMI(whoami, "SOS_init");

    dlog(1, "[%s]: Initializing SOS...\n", whoami);
    dlog(1, "[%s]:   ... setting argc / argv\n", whoami);
    SOS.config.argc = *argc;
    SOS.config.argv = *argv;
    SOS.config.process_id = (int) getpid();

    SOS.config.node_id = (char *) malloc( SOS_DEFAULT_STRING_LEN );
    gethostname( SOS.config.node_id, SOS_DEFAULT_STRING_LEN );
    dlog(1, "[%s]:   ... node_id: %s\n", whoami, SOS.config.node_id );

    dlog(1, "[%s]:   ... configuring data rings.\n", whoami);
    SOS_ring_init(&SOS.ring.send);
    SOS_ring_init(&SOS.ring.recv);

    #if (SOS_CONFIG_USE_THREAD_POOL > 0)
    SOS.task.post = (pthread_t *) malloc(sizeof(pthread_t));
    SOS.task.read = (pthread_t *) malloc(sizeof(pthread_t));
    SOS.task.scan = (pthread_t *) malloc(sizeof(pthread_t));
    dlog(1, "[%s]:   ... launching data migration threads.\n", whoami);
    retval = pthread_create( SOS.task.post, NULL, (void *) SOS_THREAD_post, NULL );
    if (retval != 0) { dlog(0, "[%s]:  ... ERROR (%d) launching SOS.task.post thread!  (%s)\n", whoami, retval, strerror(errno)); exit(EXIT_FAILURE); }
    retval = pthread_create( SOS.task.read, NULL, (void *) SOS_THREAD_read, NULL );
    if (retval != 0) { dlog(0, "[%s]:  ... ERROR (%d) launching SOS.task.read thread!  (%s)\n", whoami, retval, strerror(errno)); exit(EXIT_FAILURE); }
    retval = pthread_create( SOS.task.scan, NULL, (void *) SOS_THREAD_scan, NULL );
    if (retval != 0) { dlog(0, "[%s]:  ... ERROR (%d) launching SOS.task.scan thread!  (%s)\n", whoami, retval, strerror(errno)); exit(EXIT_FAILURE); }
    #endif

    /* The CLIENT doesn't do pack-granularity queueing (yet)... */
    SOS.task.val_intake = NULL;
    SOS.task.val_outlet = NULL;


    if (SOS.role == SOS_ROLE_CLIENT) {
        /*
         *
         *  CLIENT / CONTROL
         *
         */
        dlog(1, "[%s]:   ... setting up socket communications with the daemon.\n", whoami );

        SOS.net.buffer_len    = SOS_DEFAULT_BUFFER_LEN;
        SOS.net.timeout       = SOS_DEFAULT_MSG_TIMEOUT;
        SOS.net.server_host   = SOS_DEFAULT_SERVER_HOST;
        SOS.net.server_port   = getenv("SOS_CMD_PORT");
        if ( SOS.net.server_port == NULL ) { dlog(0, "[%s]: ERROR!  SOS_CMD_PORT environment variable is not set!\n", whoami); exit(-1); }
        if ( strlen(SOS.net.server_port) < 2 ) { dlog(0, "[%s]: ERROR!  SOS_CMD_PORT environment variable is not set!\n", whoami); exit(-1); }

        SOS.net.server_hint.ai_family    = AF_UNSPEC;        /* Allow IPv4 or IPv6 */
        SOS.net.server_hint.ai_protocol  = 0;                /* Any protocol */
        SOS.net.server_hint.ai_socktype  = SOCK_STREAM;      /* SOCK_STREAM vs. SOCK_DGRAM vs. SOCK_RAW */
        SOS.net.server_hint.ai_flags     = AI_NUMERICSERV | SOS.net.server_hint.ai_flags;

        retval = getaddrinfo(SOS.net.server_host, SOS.net.server_port, &SOS.net.server_hint, &SOS.net.result_list );
        if ( retval < 0 ) { dlog(0, "[%s]: ERROR!  Could not locate the SOS daemon.  (%s:%s)\n", whoami, SOS.net.server_host, SOS.net.server_port ); exit(1); }


        for ( SOS.net.server_addr = SOS.net.result_list ; SOS.net.server_addr != NULL ; SOS.net.server_addr = SOS.net.server_addr->ai_next ) {
            /* Iterate the possible connections and register with the SOS daemon: */
            server_socket_fd = socket(SOS.net.server_addr->ai_family, SOS.net.server_addr->ai_socktype, SOS.net.server_addr->ai_protocol );
            if ( server_socket_fd == -1 ) continue;
            if ( connect(server_socket_fd, SOS.net.server_addr->ai_addr, SOS.net.server_addr->ai_addrlen) != -1 ) break; /* success! */
            close( server_socket_fd );
        }

        freeaddrinfo( SOS.net.result_list );
        
        if (server_socket_fd == 0) { dlog(0, "[%s]: ERROR!  Could not connect to the server.  (%s:%s)\n", whoami, SOS.net.server_host, SOS.net.server_port); exit(1); }

        dlog(1, "[%s]:   ... registering this instance with SOS.   (%s:%s)\n", whoami, SOS.net.server_host, SOS.net.server_port);

        header.msg_size = sizeof(SOS_msg_header);
        header.msg_type = SOS_MSG_TYPE_REGISTER;
        header.msg_from = 0;
        header.pub_guid = 0;

        memset(buffer, '\0', SOS_DEFAULT_ACK_LEN);

        SOS_buffer_pack(buffer, "iill", 
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.pub_guid);
        
        retval = sendto( server_socket_fd, buffer, sizeof(SOS_msg_header), 0, 0, 0 );
        if (retval < 0) { dlog(0, "[%s]: ERROR!  Could not write to server socket!  (%s:%s)\n", whoami, SOS.net.server_host, SOS.net.server_port); exit(1); }

        dlog(1, "[%s]:   ... listening for the server to reply...\n", whoami);
        memset(buffer, '\0', SOS_DEFAULT_ACK_LEN);
        retval = recv( server_socket_fd, (void *) buffer, SOS_DEFAULT_ACK_LEN, 0);

        dlog(6, "[%s]:   ... server responded with %d bytes.\n", whoami, retval);
        memcpy(&guid_pool_from, buffer, sizeof(long));
        memcpy(&guid_pool_to, (buffer + sizeof(long)), sizeof(long));
        dlog(1, "[%s]:   ... received guid range from %ld to %ld.\n", whoami, guid_pool_from, guid_pool_to);
        dlog(1, "[%s]:   ... configuring uid sets.\n", whoami);

        SOS_uid_init(&SOS.uid.local_serial, 0, SOS_DEFAULT_UID_MAX);
        SOS_uid_init(&SOS.uid.my_guid_pool, guid_pool_from, guid_pool_to);   /* DAEMON doesn't use this, it's for CLIENTS. */

        SOS.my_guid = SOS_uid_next( SOS.uid.my_guid_pool );
        dlog(1, "[%s]:   ... SOS.my_guid == %ld\n", whoami, SOS.my_guid);

        close( server_socket_fd );

    } else {
        /*
         *
         *  CONFIGURATION: DAEMON / DATABASE / etc.
         *
         */

        dlog(0, "[%s]:   ... skipping socket setup (becase we're the daemon).\n", whoami);
    }

    SOS.status = SOS_STATUS_RUNNING;

    dlog(1, "[%s]:   ... done with SOS_init().\n", whoami);
    return;
}


void SOS_async_buf_pair_init(SOS_async_buf_pair **buf_pair_ptr) {
    SOS_SET_WHOAMI(whoami, "SOS_async_buf_pair_init");
    SOS_async_buf_pair *buf_pair;

    *buf_pair_ptr = (SOS_async_buf_pair *) malloc(sizeof(SOS_async_buf_pair));
    buf_pair = *buf_pair_ptr;
    memset(buf_pair, '\0', sizeof(SOS_async_buf_pair));
    
    memset(buf_pair->a.data, '\0', SOS_DEFAULT_BUFFER_LEN);
    memset(buf_pair->b.data, '\0', SOS_DEFAULT_BUFFER_LEN);
    buf_pair->a.len = 0;
    buf_pair->b.len = 0;
    buf_pair->a.max = SOS_DEFAULT_BUFFER_LEN;
    buf_pair->b.max = SOS_DEFAULT_BUFFER_LEN;
    buf_pair->grow_buf = &buf_pair->a;
    buf_pair->send_buf = &buf_pair->b;

    buf_pair->a.lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    buf_pair->b.lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(buf_pair->a.lock, NULL);
    pthread_mutex_init(buf_pair->b.lock, NULL);
    buf_pair->flush_cond = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    pthread_cond_init(buf_pair->flush_cond, NULL);

    return;
}

void SOS_async_buf_pair_fflush(SOS_async_buf_pair *buf_pair) {
    SOS_SET_WHOAMI(whoami, "SOS_async_buf_pair_fflush");

    dlog(1, "[%s]: Forcing a flush...\n", whoami);
    if (SOS.status != SOS_STATUS_RUNNING) {
        dlog(1, "[%s]:   ... skipping buffer flush, system is shutting down\n", whoami);
        return;
    }
    dlog(1, "[%s]:   ... LOCK grow_buf->lock\n", whoami);
    pthread_mutex_lock(buf_pair->grow_buf->lock);
    dlog(1, "[%s]:   ... calling _autoflush()\n", whoami);
    SOS_async_buf_pair_autoflush(buf_pair);
    dlog(1, "[%s]:   ... UNLOCK grow_buf->lock\n", whoami);
    pthread_mutex_unlock(buf_pair->grow_buf->lock);

    return;
}

void SOS_async_buf_pair_autoflush(SOS_async_buf_pair *buf_pair) {
    SOS_SET_WHOAMI(whoami, "SOS_async_buf_pair_autoflush");

    /* The thread that actually does the 'flushing' is set up by the
     * context that creats the buf_pair.  Specifically, all we do here
     * is grab the locks, swap the roles of the buffers, and trigger the
     * flush condition.  Immediately, grow_buf is available to handle new
     * writes.  Whatever thread was started that is waiting on the flush
     * condition will be activated to perform the async send/write, and
     * it can operate knowing that it effectively holds locks on both
     * grow and send buffers.
     * NOTE: When that thread is done with the actual send/commit it
     *       has been designed to do, and it releases the send lock,
     *       the send buffer is unstable, that is, it might be cleared
     *       out by some other function.  Do your operation entirely
     *       atomically, or make a copy of the buffer to work with before
     *       releasing the send lock! */

    /* NOTE: This is the _autoflush function, so the grow_buf lock
     *       has already been obtained/locked by either _fflush()
     *       or the _insert() buf_pair functions.  We don't need
     *       to be concerned about it here. */

    dlog(1, "[%s]:   ... LOCK send_buf->lock\n", whoami);
    pthread_mutex_lock(buf_pair->send_buf->lock);
    dlog(1, "[%s]:   ... swap buffers\n", whoami);
    if (buf_pair->send_buf == &buf_pair->a) {
        buf_pair->grow_buf = &buf_pair->a;
        buf_pair->send_buf = &buf_pair->b;
    } else {
        buf_pair->grow_buf = &buf_pair->b;
        buf_pair->send_buf = &buf_pair->a;
    }

    dlog(1, "[%s]:   ... signal flush condition\n", whoami);
    pthread_cond_signal(buf_pair->flush_cond);
    dlog(1, "[%s]:   ... UNLOCK send_buf->lock\n", whoami);
    pthread_mutex_unlock(buf_pair->send_buf->lock);

    return;
}

void SOS_async_buf_pair_insert(SOS_async_buf_pair *buf_pair, char *msg_ptr, int msg_len) {
    SOS_SET_WHOAMI(whoami, "SOS_async_buf_pair_insert");
    SOS_buf *buf;
    int count;
    int count_size;

    dlog(1, "[%s]:   ... LOCK grow_buf->lock\n", whoami);
    pthread_mutex_lock(buf_pair->grow_buf->lock);

    buf = buf_pair->grow_buf;
    count_size = SOS_buffer_unpack(buf->data, "i", &count);

    /* NOTE: '8' to make sure we'll have room to pack in the msg_len... */
    if ((8 + msg_len) > buf->max) {
        dlog(0, "[%s]: WARNING! You've attempted to insert a value that is larger than the buffer!  (msg_len == %d)\n", whoami, msg_len);
        dlog(0, "[%s]: WARNING! Skipping this value, but the system is no longer tracking all entries.\n", whoami);
        dlog(1, "[%s]:   ... UNLOCK grow_buf->lock\n", whoami);
        pthread_mutex_unlock(buf_pair->grow_buf->lock);
        return;
    }

    if ((8 + msg_len + buf->len) > buf->max) {
        SOS_async_buf_pair_autoflush(buf_pair);
        buf = buf_pair->grow_buf;
        /* Now buf points to an totally empty/available buffer,
         * and we know our value fits.*/
    }

    /* NOTE: We can't use SOS_buffer_pack() for the msg because, being already
     *       the product of packing, msg may contain inline null zero's and is
     *       not useable as a 'string'.  We use memcpy on it instead.  This will
     *       be true on the receiving/processing side of this buffer as well. */
    memcpy((buf->data + buf->len + count_size), msg_ptr, msg_len);
    buf->len += msg_len;

    buf->entry_count++;
    dlog(7, "[%s]: buf->entry_count == %d\n", whoami, buf->entry_count);

    SOS_buffer_pack(buf->data, "i", buf->entry_count);

    dlog(1, "[%s]:   ... UNLOCK grow_buf->lock\n", whoami);
    pthread_mutex_unlock(buf->lock);

    return;
}


void SOS_async_buf_pair_destroy(SOS_async_buf_pair *buf_pair) {
    SOS_SET_WHOAMI(whoami, "SOS_async_buf_pair_destroy");

    /* Naturally, exit/join any affiliated threads before calling this, as
     * this wipes out the condition variable that the thread might be
     * waiting on.  */

    dlog(1, "[%s]:   ... send_buf mutex\n", whoami);
    dlog(1, "[%s]:      ... LOCK send_buf->lock\n", whoami);
    pthread_mutex_lock(buf_pair->send_buf->lock);
    dlog(1, "[%s]:      ... destroying\n", whoami);
    pthread_mutex_destroy(buf_pair->send_buf->lock);

    dlog(1, "[%s]:   ... grow_buf mutex\n", whoami);
    dlog(1, "[%s]:      ... LOCK grow_buf->lock\n", whoami);
    pthread_mutex_lock(buf_pair->grow_buf->lock);
    dlog(1, "[%s]:      ... destroying\n", whoami);
    pthread_mutex_destroy(buf_pair->grow_buf->lock);

    free(buf_pair->a.lock);
    free(buf_pair->b.lock);
    pthread_cond_destroy(buf_pair->flush_cond);
    free(buf_pair->flush_cond);

    memset(buf_pair, '\0', sizeof(buf_pair));
    free(buf_pair);

    return;
}



void SOS_ring_init(SOS_ring_queue **ring_var) {
    SOS_SET_WHOAMI(whoami, "SOS_ring_init");
    SOS_ring_queue *ring;

    dlog(1, "[%s]:   ... initializing ring_var @ %ld\n", whoami, (long) ring_var);
    ring = *ring_var = (SOS_ring_queue *) malloc(sizeof(SOS_ring_queue));
    ring->read_elem  = ring->write_elem  = 0;
    ring->elem_count = 0;
    ring->elem_max   = SOS_DEFAULT_RING_SIZE;
    ring->elem_size  = sizeof(long);
    ring->heap       = (long *) malloc( ring->elem_max * ring->elem_size );
    memset( ring->heap, '\0', (ring->elem_max * ring->elem_size) );
    dlog(1, "[%s]:      ... successfully initialized ring queue.\n", whoami);
    dlog(1, "[%s]:      ... initializing mutex.\n", whoami);
    ring->lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(ring->lock, NULL);
    dlog(1, "[%s]:   ... done.\n", whoami);
    return;
}

void SOS_ring_destroy(SOS_ring_queue *ring) {
    SOS_SET_WHOAMI(whoami, "SOS_ring_destroy");

    free(ring->lock);

    memset(ring->heap, '\0', (ring->elem_max * ring->elem_size));
    free(ring->heap);
    memset(ring, '\0', (sizeof(SOS_ring_queue)));
    free(ring);

    return;
}


int SOS_ring_put(SOS_ring_queue *ring, long item) {
    SOS_SET_WHOAMI(whoami, "SOS_ring_put");
    dlog(5, "[%s]: LOCK ring->lock\n", whoami);
    pthread_mutex_lock(ring->lock);

    dlog(5, "[%s]: Attempting to add (%ld) into the ring.\n", whoami, item);

    if (ring == NULL) { dlog(0, "[%s]: ERROR!  Attempted to insert into a NULL ring!\n", whoami); exit(EXIT_FAILURE); }

    if (ring->elem_count >= ring->elem_max) {
        /* The ring is full... */
        dlog(5, "[%s]: ERROR!  Attempting to insert a data element into a full ring!", whoami);
        dlog(5, "[%s]:   ... UNLOCK ring->lock\n", whoami);
        pthread_mutex_unlock(ring->lock);
        return(-1);
    }
    
    ring->heap[ring->write_elem] = item;

    ring->write_elem = (ring->write_elem + 1) % ring->elem_max;
    ring->elem_count++;

    dlog(5, "[%s]:   ... this is item %d of %d @ position %d.\n", whoami, ring->elem_count, ring->elem_max, (ring->write_elem - 1));

    dlog(5, "[%s]:   ... UNLOCK ring->lock\n", whoami);
    pthread_mutex_unlock(ring->lock);
    dlog(5, "[%s]:   ... done.\n", whoami);

    return(0);
}


long SOS_ring_get(SOS_ring_queue *ring) {
    SOS_SET_WHOAMI(whoami, "SOS_ring_get");
    long element;

    dlog(1, "[%s]: LOCK ring->lock\n", whoami);
    pthread_mutex_lock(ring->lock);

    if (ring->elem_count == 0) {
        pthread_mutex_unlock(ring->lock);
        return -1;
    }

    element = ring->heap[ring->read_elem];
    ring->read_elem = (ring->read_elem + 1) % ring->elem_max;
    ring->elem_count--;

    dlog(1, "[%s]: UNLOCK ring->lock\n", whoami);
    pthread_mutex_unlock(ring->lock);

    return element;
}


long* SOS_ring_get_all(SOS_ring_queue *ring, int *elem_returning) {
    SOS_SET_WHOAMI(whoami, "SOS_ring_get_all");
    long *elem_list;
    int elem_list_bytes;
    int fragment_count;

    dlog(1, "[%s]: LOCK ring->lock\n", whoami);
    pthread_mutex_lock(ring->lock);

    elem_list_bytes = (ring->elem_count * ring->elem_size);
    elem_list = (long *) malloc(elem_list_bytes);
    memset(elem_list, '\0', elem_list_bytes);

    *elem_returning = ring->elem_count;

    memcpy(elem_list, &ring->heap[ring->read_elem], (ring->elem_count * ring->elem_size));
    ring->read_elem = ring->write_elem;
    ring->elem_count = 0;

    dlog(1, "[%s]: UNLOCK ring->lock\n", whoami);
    pthread_mutex_unlock(ring->lock);

    return elem_list;
}

void SOS_send_to_daemon( char *msg, int msg_len, char *reply, int reply_max ) {
    SOS_SET_WHOAMI(whoami, "SOS_send_to_daemon");

    SOS_msg_header header;
    int server_socket_fd;
    int retval;

    /* TODO: { SEND_TO_DAEMON } Perhaps this should be made thread safe. */

    retval = getaddrinfo(SOS.net.server_host, SOS.net.server_port, &SOS.net.server_hint, &SOS.net.result_list );
    if ( retval < 0 ) { dlog(0, "[%s]: ERROR!  Could not locate the SOS daemon.  (%s:%s)\n", whoami, SOS.net.server_host, SOS.net.server_port ); exit(1); }
    
    /* Iterate the possible connections and register with the SOS daemon: */
    for ( SOS.net.server_addr = SOS.net.result_list ; SOS.net.server_addr != NULL ; SOS.net.server_addr = SOS.net.server_addr->ai_next ) {
        server_socket_fd = socket(SOS.net.server_addr->ai_family, SOS.net.server_addr->ai_socktype, SOS.net.server_addr->ai_protocol );
        if ( server_socket_fd == -1 ) continue;
        if ( connect( server_socket_fd, SOS.net.server_addr->ai_addr, SOS.net.server_addr->ai_addrlen) != -1 ) break; /* success! */
        close( server_socket_fd );
    }
    
    freeaddrinfo( SOS.net.result_list );
    
    if (server_socket_fd == 0) {
        dlog(0, "[%s]: Error attempting to connect to the server.  (%s:%s)\n", whoami, SOS.net.server_host, SOS.net.server_port);
        exit(1);  /* TODO:{ SEND_TO_DAEMON }  Make this a loop that tries X times to connect, doesn't crash app. */
    }

    /* TODO: { SEND_TO_DAEMON } Make this a loop that ensures all data was sent. */
    retval = send(server_socket_fd, msg, msg_len, 0 );
    if (retval == -1) { dlog(0, "[%s]: Error sending message to daemon.\n", whoami); }

    retval = recv(server_socket_fd, reply, reply_max, 0);
    if (retval == -1) { dlog(0, "[%s]: Error receiving message from daemon.\n", whoami); }
    else { dlog(6, "[%s]: Server sent a (%d) byte reply.\n", whoami, retval); }

    close( server_socket_fd );

    return;
}



void SOS_finalize() {
    SOS_SET_WHOAMI(whoami, "SOS_finalize");
    
    /* This will 'notify' any SOS threads to break out of their loops. */
    dlog(0, "[%s]: SOS.status = SOS_STATUS_SHUTDOWN\n", whoami);
    SOS.status = SOS_STATUS_SHUTDOWN;

    #if (SOS_CONFIG_USE_THREAD_POOL > 0)
    dlog(0, "[%s]:   ... Joining threads...\n", whoami);
    pthread_join( *SOS.task.post, NULL );
    pthread_join( *SOS.task.read, NULL );
    pthread_join( *SOS.task.scan, NULL );
    #endif

    if (SOS.role == SOS_ROLE_CLIENT) {
        dlog(0, "[%s]:   ... Releasing uid objects...\n", whoami);
        SOS_uid_destroy(SOS.uid.local_serial);
        SOS_uid_destroy(SOS.uid.my_guid_pool);
    }

    dlog(0, "[%s]:   ... Releasing ring queues...\n", whoami);
    SOS_ring_destroy(SOS.ring.send);
    SOS_ring_destroy(SOS.ring.recv);

    dlog(0, "[%s]: Done!\n", whoami);
    return;
}



void* SOS_THREAD_post( void *args ) {
    SOS_SET_WHOAMI(whoami, "SOS_THREAD_post");
    
    while (SOS.status != SOS_STATUS_SHUTDOWN) {
        /*
         *  Transmit messages to the daemon.
         * ...
         *
         */
        usleep(250000);
    }
    return NULL;
}



void* SOS_THREAD_read( void *args ) {
    SOS_SET_WHOAMI(whoami, "SOS_THREAD_read");

    while (SOS.status != SOS_STATUS_SHUTDOWN) {
        /*
         *  Read the char* messages we've received and unpack into data structures.
         * ...
         *
         */
        usleep(250000);
    }    
    return NULL;
}




void* SOS_THREAD_scan( void *args ) {
    SOS_SET_WHOAMI(whoami, "SOS_THREAD_scan");

    while (SOS.status == SOS_STATUS_SHUTDOWN) {
        /*
         *  Check out the dirty data and package it for sending.
         * ...
         *
         */
        sleep(250000);
    }
    return NULL;
}



void SOS_uid_init( SOS_uid **id_var, long set_from, long set_to ) {
    SOS_SET_WHOAMI(whoami, "SOS_uid_init");
    SOS_uid *id;

    dlog(1, "[%s]:   ... allocating uid sets\n", whoami);
    id = *id_var = (SOS_uid *) malloc(sizeof(SOS_uid));
    id->next = (set_from > 0) ? set_from : 1;
    id->last = (set_to   < SOS_DEFAULT_UID_MAX) ? set_to : SOS_DEFAULT_UID_MAX;
    dlog(1, "[%s]:      ... default set for uid range (%ld -> %ld).\n", whoami, id->next, id->last);

    dlog(1, "[%s]:      ... initializing uid mutex.\n", whoami);
    id->lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(id->lock, NULL );

    return;
}


void SOS_uid_destroy( SOS_uid *id ) {
    SOS_SET_WHOAMI(whoami, "SOS_uid_destroy");


    dlog(1, "[%s]:   ... destroying uid mutex     &(%ld)\n", whoami, (long) &id->lock );
    pthread_mutex_destroy( id->lock );
    dlog(1, "[%s]:   ... freeing uid mutex space  &(%ld)\n", whoami, (long) &id->lock );
    free(id->lock);
    dlog(1, "[%s]:   ... freeing uid memory       &(%ld)\n", whoami, (long) id);
    memset(id, '\0', sizeof(SOS_uid));
    free(id);

    return;
}


long SOS_uid_next( SOS_uid *id ) {
    SOS_SET_WHOAMI(whoami, "SOS_uid_next");
    long next_serial;

    dlog(1, "[%s]: LOCK id->lock\n", whoami);
    pthread_mutex_lock( id->lock );

    next_serial = id->next++;

    if (id->next > id->last) {
    /* The assumption here is that we're dealing with a GUID, as the other
     * 'local' uid ranges are so large as to effectively guarantee this case
     * will not occur for them.
     */

        if (SOS.role == SOS_ROLE_DAEMON) {
            /* NOTE: There is no recourse if a DAEMON runs out of GUIDs.
             *       That should *never* happen.
             */
            dlog(0, "[%s]: ERROR!  This sosd instance has run out of GUIDs!  Terminating.\n", whoami);
            exit(EXIT_FAILURE);
        } else {
            /* Acquire a fresh block of GUIDs from the DAEMON... */
            SOS_msg_header msg;
            char buffer[SOS_DEFAULT_ACK_LEN];
            
            dlog(1, "[%s]: The last guid has been used from SOS.uid.my_guid_pool!  Requesting a new block...\n", whoami);
            msg.msg_size = sizeof(SOS_msg_header);
            msg.msg_from = SOS.my_guid;
            msg.msg_type = SOS_MSG_TYPE_GUID_BLOCK;
            msg.pub_guid = 0;
            SOS_send_to_daemon((char *) &msg, sizeof(SOS_msg_header), buffer, SOS_DEFAULT_ACK_LEN);
            memcpy(&id->next, buffer, sizeof(long));
            memcpy(&id->last, (buffer + sizeof(long)), sizeof(long));
            dlog(1, "[%s]:   ... recieved a new guid block from %ld to %ld.\n", whoami, id->next, id->last);
        }
    }

    dlog(1, "[%s]: UNLOCK id->lock\n", whoami);
    pthread_mutex_unlock( id->lock );

    return next_serial;
}


SOS_pub* SOS_pub_create(char *title) { return SOS_pub_create_sized(title, SOS_DEFAULT_ELEM_MAX); }
SOS_pub* SOS_new_post(char *title){ return SOS_pub_create_sized(title, 1); }
SOS_pub* SOS_pub_create_sized(char *title, int new_size) {
    SOS_SET_WHOAMI(whoami, "SOS_pub_create_sized");

    SOS_pub   *new_pub;
    int        i;

    dlog(6, "[%s]: Allocating and initializing a new pub handle....\n", whoami);

    new_pub = malloc(sizeof(SOS_pub));
    memset(new_pub, '\0', sizeof(SOS_pub));

    if (SOS.role != SOS_ROLE_CLIENT) {
        new_pub->guid = -1;
    } else {
        new_pub->guid = SOS_uid_next( SOS.uid.my_guid_pool );
    }

    dlog(6, "[%s]:   ... setting default values, allocating space for strings.\n", whoami);

    new_pub->node_id      = (char *) malloc( SOS_DEFAULT_STRING_LEN );
    new_pub->process_id   = 0;
    new_pub->thread_id    = 0;
    new_pub->comm_rank    = 0;
    new_pub->prog_name    = (char *) malloc( SOS_DEFAULT_STRING_LEN );
    new_pub->prog_ver     = (char *) malloc( SOS_DEFAULT_STRING_LEN );
    new_pub->pragma_len   = 0;
    new_pub->pragma_msg   = (char *) malloc( SOS_DEFAULT_STRING_LEN );
    new_pub->title = (char *) malloc(strlen(title) + 1);
        memset(new_pub->title, '\0', (strlen(title) + 1));
        strcpy(new_pub->title, title);
    new_pub->announced           = 0;
    new_pub->elem_count          = 0;
    new_pub->elem_max            = new_size;
    new_pub->meta.channel     = 0;
    new_pub->meta.layer       = SOS_LAYER_APP;
    new_pub->meta.nature      = SOS_NATURE_SOS;
    new_pub->meta.pri_hint    = SOS_PRI_DEFAULT;
    new_pub->meta.scope_hint  = SOS_SCOPE_DEFAULT;
    new_pub->meta.retain_hint = SOS_RETAIN_DEFAULT;

    dlog(6, "[%s]:   ... zero-ing out the strings.\n", whoami);

    memset(new_pub->node_id,    '\0', SOS_DEFAULT_STRING_LEN);
    memset(new_pub->prog_name,  '\0', SOS_DEFAULT_STRING_LEN);
    memset(new_pub->prog_ver,   '\0', SOS_DEFAULT_STRING_LEN);
    memset(new_pub->pragma_msg, '\0', SOS_DEFAULT_STRING_LEN);

    /* Set some defaults for the SOS_ROLE_CLIENT's */
    if (SOS.role == SOS_ROLE_CLIENT) {
        dlog(6, "[%s]:   ... setting defaults specific to SOS_ROLE_CLIENT.\n", whoami);
        strncpy(new_pub->node_id, SOS.config.node_id, SOS_DEFAULT_STRING_LEN);
        new_pub->process_id = SOS.config.process_id;
        strncpy(new_pub->prog_name, SOS.config.argv[0], SOS_DEFAULT_STRING_LEN);
    }

    dlog(6, "[%s]:   ... allocating space for data elements.\n", whoami);
    new_pub->data                = malloc(sizeof(SOS_data *) * new_size);

    dlog(6, "[%s]:   ... setting defaults for each data element.\n", whoami);
    for (i = 0; i < new_size; i++) {
        new_pub->data[i] = malloc(sizeof(SOS_data));
            memset(new_pub->data[i], '\0', sizeof(SOS_data));
            new_pub->data[i]->guid      = 0;
            new_pub->data[i]->name      = (char *) malloc( SOS_DEFAULT_STRING_LEN );
            memset(new_pub->data[i]->name, '\0', SOS_DEFAULT_STRING_LEN);
            new_pub->data[i]->type      = SOS_VAL_TYPE_INT;
            new_pub->data[i]->val_len   = 0;
            new_pub->data[i]->val.l_val = 0;
            new_pub->data[i]->state     = SOS_VAL_STATE_EMPTY;
            new_pub->data[i]->time.pack = 0.0;
            new_pub->data[i]->time.send = 0.0;
            new_pub->data[i]->time.recv = 0.0;

            new_pub->data[i]->meta.freq     = SOS_VAL_FREQ_DEFAULT;
            new_pub->data[i]->meta.class    = SOS_VAL_CLASS_DATA;
            new_pub->data[i]->meta.semantic = SOS_VAL_SEMANTIC_DEFAULT;
            new_pub->data[i]->meta.pattern  = SOS_VAL_PATTERN_DEFAULT;
            new_pub->data[i]->meta.compare  = SOS_VAL_COMPARE_SELF;
            new_pub->data[i]->meta.mood     = SOS_MOOD_GOOD;

    }
    dlog(6, "[%s]:   ... done.\n", whoami);

    return new_pub;
}



void SOS_expand_data( SOS_pub *pub ) {
    SOS_SET_WHOAMI(whoami, "SOS_expand_data");

    int n;
    SOS_data **expanded_data;

    expanded_data = malloc((pub->elem_max + SOS_DEFAULT_ELEM_MAX) * sizeof(SOS_data *));
    memcpy(expanded_data, pub->data, (pub->elem_max * sizeof(SOS_data *)));
    for (n = pub->elem_max; n < (pub->elem_max + SOS_DEFAULT_ELEM_MAX); n++) {
        expanded_data[n] = malloc(sizeof(SOS_data));
        memset(expanded_data[n], '\0', sizeof(SOS_data)); }
    free(pub->data);
    pub->data = expanded_data;
    pub->elem_max = (pub->elem_max + SOS_DEFAULT_ELEM_MAX);

    return;
}


void SOS_strip_str( char *str ) {
    int i, len;
    len = strlen(str);

    for (i = 0; i < len; i++) {
        if (str[i] == '\"') str[i] = '\'';
        if (str[i] < ' ' || str[i] > '~') str[i] = '#';
    }
  
    return;
}


int SOS_define_value( SOS_pub *pub, const char *name, SOS_val_type val_type, SOS_val_meta val_meta) {
    SOS_SET_WHOAMI(whoami, "SOS_define_val");

    SOS_val empty;
    int val_pos;

    memset(&empty, '\0', sizeof(SOS_val));

    switch (val_type) {
    case SOS_VAL_TYPE_INT:    empty.i_val = 0;    break;
    case SOS_VAL_TYPE_LONG:   empty.l_val = 0;    break;
    case SOS_VAL_TYPE_DOUBLE: empty.d_val = 0.0;  break;
    case SOS_VAL_TYPE_STRING: empty.c_val = NULL; break;
    default: dlog(0, "[%s]: ERROR!  Attempted to define an invalid value type (%d).\n", whoami, val_type); exit(EXIT_FAILURE); break;
    }

    val_pos = SOS_pack(pub, name, val_type, empty);
    pub->data[val_pos]->meta = val_meta;

    return val_pos;
}


int SOS_pack( SOS_pub *pub, const char *name, SOS_val_type pack_type, SOS_val pack_val ) {
    SOS_SET_WHOAMI(whoami, "SOS_pack");

    /* TODO:{ PACK } This is due for a re-write, eventually.
     *               Add hashtable lookup to improve search time in linear array. */

    //counter variables
    int i, n;
    //variables for working with adding pack_val SOS_VAL_TYPE_STRINGs
    int new_str_len;
    char *new_str_ptr;
    char *pub_str_ptr;
    char *new_name;



    //try to find the name in the existing pub schema:
    for (i = 0; i < pub->elem_count; i++) {
        if (strcmp(pub->data[i]->name, name) == 0) {

            dlog(6, "[%s]: (%s) name located at position %d.\n", whoami, name, i);

            switch (pack_type) {

            case SOS_VAL_TYPE_STRING :
                pub_str_ptr = pub->data[i]->val.c_val;
                new_str_ptr = pack_val.c_val;
                new_str_len = strlen(new_str_ptr);

                if (strcmp(pub_str_ptr, new_str_ptr) == 0) {
                    dlog(5, "[%s]: Packed value is identical to existing value.  Updating timestamp and skipping.\n", whoami);
                    SOS_TIME(pub->data[i]->time.pack);
                    return i;
                }

                free(pub_str_ptr);
                pub_str_ptr = malloc(new_str_len + 1);
                strncpy(pub_str_ptr, new_str_ptr, new_str_len);
                pub_str_ptr[new_str_len + 1] = '\0';

                dlog(6, "[%s]: assigning a new string.   \"%s\"   (updating)\n", whoami, pack_val.c_val);
                pub->data[i]->val = (SOS_val) pub_str_ptr;
                break;

            case SOS_VAL_TYPE_INT :
            case SOS_VAL_TYPE_LONG :
            case SOS_VAL_TYPE_DOUBLE :

                /* Test if the values are equal, otherwise fall through to the non-string assignment. */

                if (pack_type == SOS_VAL_TYPE_INT && (pub->data[i]->val.i_val == pack_val.i_val)) {
                    dlog(5, "[%s]: Packed value is identical to existing value.  Updating timestamp and skipping.\n", whoami);
                    SOS_TIME(pub->data[i]->time.pack);
                    return i;
                } else if (pack_type == SOS_VAL_TYPE_LONG && (pub->data[i]->val.l_val == pack_val.l_val)) {
                    dlog(5, "[%s]: Packed value is identical to existing value.  Updating timestamp and skipping.\n", whoami);
                    SOS_TIME(pub->data[i]->time.pack);
                    return i;
                } else if (pack_type == SOS_VAL_TYPE_DOUBLE) {
                    /*
                     *  TODO:{ PACK } Insert proper floating-point comparator here.
                     */
                }

            default :
                dlog(6, "[%s]: assigning a new value.   \"%ld\"   (updating)\n", whoami, pack_val.l_val);
                pub->data[i]->val = pack_val;
                break;
            }
            pub->data[i]->type = pack_type;
            pub->data[i]->state = SOS_VAL_STATE_DIRTY;
            SOS_TIME(pub->data[i]->time.pack);

            switch (pack_type) {
            case SOS_VAL_TYPE_INT:    pub->data[i]->val_len = sizeof(int);    break;
            case SOS_VAL_TYPE_LONG:   pub->data[i]->val_len = sizeof(long);   break;
            case SOS_VAL_TYPE_DOUBLE: pub->data[i]->val_len = sizeof(double); break;
            case SOS_VAL_TYPE_STRING: pub->data[i]->val_len = strlen(pub->data[i]->val.c_val); break;
            }

            dlog(6, "[%s]: (%s) successfully updated [%s] at position %d.\n", whoami, name, pub->data[i]->name, i);
            dlog(6, "[%s]: --------------------------------------------------------------\n", whoami);

            return i;
        }
    }

    dlog(6, "[%s]: (%s) name does not exist in schema yet, attempting to add it.\n", whoami, name);

    //name does not exist in the existing schema, add it:
    pub->announced = 0;
    new_str_len = strlen(name);
    new_name = malloc(new_str_len + 1);
    memset(new_name, '\0', (new_str_len + 1));
    strncpy(new_name, name, new_str_len);
    new_name[new_str_len] = '\0';

    if (pub->elem_count < pub->elem_max) {
        i = pub->elem_count;
        pub->elem_count++;

        dlog(6, "[%s]: (%s) inserting into position %d\n", whoami, name, i);

        switch (pack_type) {

        case SOS_VAL_TYPE_STRING :
            new_str_ptr = pack_val.c_val;
            new_str_len = strlen(new_str_ptr);
            pub_str_ptr = malloc(new_str_len + 1);
            memset(pub_str_ptr, '\0', (new_str_len + 1));
            strncpy(pub_str_ptr, new_str_ptr, new_str_len);
            pub_str_ptr[new_str_len + 1] = '\0';
            dlog(6, "[%s]: (%s) assigning a new string.   \"%s\"   (insert)\n", whoami, name, pub_str_ptr);
            pub->data[i]->val = (SOS_val) pub_str_ptr;
            break;

        case SOS_VAL_TYPE_DOUBLE:
            dlog(6, "[%s]: (%s) assigning a new double.   \"%lF\"   (insert)\n", whoami, name, pack_val.d_val);
            pub->data[i]->val = pack_val;
            break;
        
        case SOS_VAL_TYPE_INT:
        case SOS_VAL_TYPE_LONG:
        default :
            dlog(6, "[%s]: (%s) assigning a new value.   \"%ld\"   (insert)\n", whoami, name, pack_val.l_val);
            pub->data[i]->val = pack_val;
            break;
        }

        dlog(6, "[%s]: (%s) data copied in successfully.\n", whoami, name);

        pub->data[i]->name   = new_name;
        pub->data[i]->type   = pack_type;
        pub->data[i]->state  = SOS_VAL_STATE_DIRTY;
        pub->data[i]->guid   = SOS_uid_next( SOS.uid.my_guid_pool );
        SOS_TIME(pub->data[i]->time.pack);

        switch (pack_type) {
        case SOS_VAL_TYPE_INT:    pub->data[i]->val_len = sizeof(int);    break;
        case SOS_VAL_TYPE_LONG:   pub->data[i]->val_len = sizeof(long);   break;
        case SOS_VAL_TYPE_DOUBLE: pub->data[i]->val_len = sizeof(double); break;
        case SOS_VAL_TYPE_STRING: pub->data[i]->val_len = strlen(pub->data[i]->val.c_val); break;
        }

        dlog(6, "[%s]: (%s) successfully inserted [%s] at position %d. (DONE)\n", whoami, name, pub->data[i]->name, i);
        dlog(6, "[%s]: --------------------------------------------------------------\n", whoami);

        return i;

    } else {

        dlog(6, "[%s]: (%s) the data object is full, expanding it.  (pub->elem_max=%d)\n", whoami, name, pub->elem_max);

        SOS_expand_data(pub);
        pub->elem_count++;

        dlog(6, "[%s]: (%s) data object has been expanded successfully.  (pub->elem_max=%d)\n", whoami, name, pub->elem_max);

        //[step 2/2]: insert the new name
        switch (pack_type) {

        case SOS_VAL_TYPE_STRING :
            new_str_ptr = pack_val.c_val;
            new_str_len = strlen(new_str_ptr);
            pub_str_ptr = malloc(new_str_len + 1);
            strncpy(pub_str_ptr, new_str_ptr, new_str_len);
            pub_str_ptr[new_str_len + 1] = '\0';
            dlog(6, "[%s]: (%s) assigning a new string.   \"%s\"   (expanded)\n", whoami, name, pack_val.c_val);
            pub->data[i]->val = (SOS_val) pub_str_ptr;
            break;

        case SOS_VAL_TYPE_DOUBLE :
            dlog(6, "[%s]: (%s) assigning a new double.   \"%lF\"   (expanded)\n", whoami, name, pack_val.d_val);
            pub->data[i]->val = pack_val;
            break;
            
        case SOS_VAL_TYPE_INT :
        case SOS_VAL_TYPE_LONG :
        default :
            dlog(6, "[%s]: (%s) assigning a new value.   \"%ld\"   (expanded)\n", whoami, name, pack_val.l_val);
            pub->data[i]->val = pack_val;
            break;

        }

        dlog(6, "[%s]: ALMOST DONE....\n", whoami);

        pub->data[i]->name   = new_name;
        pub->data[i]->type   = pack_type;
        pub->data[i]->state  = SOS_VAL_STATE_DIRTY;
        pub->data[i]->guid   = SOS_uid_next( SOS.uid.my_guid_pool );
        SOS_TIME(pub->data[i]->time.pack);

        switch (pack_type) {
        case SOS_VAL_TYPE_INT:    pub->data[i]->val_len = sizeof(int);    break;
        case SOS_VAL_TYPE_LONG:   pub->data[i]->val_len = sizeof(long);   break;
        case SOS_VAL_TYPE_DOUBLE: pub->data[i]->val_len = sizeof(double); break;
        case SOS_VAL_TYPE_STRING: pub->data[i]->val_len = strlen(pub->data[i]->val.c_val); break;
        }

        dlog(6, "[%s]: (%s) successfully inserted [%s] at position %d. (DONE)\n", whoami, name, pub->data[i]->name, i);
        dlog(6, "[%s]: --------------------------------------------------------------\n", whoami);

        return i;
    }

    //shouln't ever get here.
    return -1;
}

void SOS_repack( SOS_pub *pub, int index, SOS_val pack_val ) {
    SOS_SET_WHOAMI(whoami, "SOS_repack");
    SOS_data *data;
    int len;

    switch (pub->data[index]->type) {
    case SOS_VAL_TYPE_INT    : dlog(6, "[%s]: (%s) @ %d -- pack_val.i_val = \"%d\"     (update)\n",  whoami, pub->data[index]->name, index, pack_val.i_val); break;
    case SOS_VAL_TYPE_LONG   : dlog(6, "[%s]: (%s) @ %d -- pack_val.l_val = \"%ld\"     (update)\n", whoami, pub->data[index]->name, index, pack_val.l_val); break;
    case SOS_VAL_TYPE_DOUBLE : dlog(6, "[%s]: (%s) @ %d -- pack_val.d_val = \"%lF\"     (update)\n", whoami, pub->data[index]->name, index, pack_val.d_val); break;
    case SOS_VAL_TYPE_STRING : dlog(6, "[%s]: (%s) @ %d -- pack_val.c_val = \"%s\"     (update)\n",  whoami, pub->data[index]->name, index, pack_val.c_val); break;
    }

    
    data = pub->data[index];

    switch (data->type) {

    case SOS_VAL_TYPE_STRING:
        /* Determine if the string has changed, and if so, free/malloc space for new one. */
        if (strcmp(data->val.c_val, pack_val.c_val) == 0) {
            /* Update the time stamp only. */
            SOS_TIME( data->time.pack );
        } else {
            /* Novel string is being packed, free the old one, allocate a copy. */
            free(data->val.c_val);
            len = strlen(pack_val.c_val);
            data->val.c_val = (char *) malloc(sizeof(char) * (len + 1));
            memset(data->val.c_val, '\0', len);
            memcpy(data->val.c_val, pack_val.c_val, len);
            SOS_TIME( data->time.pack );
            data->val_len = len;
        }
        break;

    case SOS_VAL_TYPE_INT:
    case SOS_VAL_TYPE_LONG:
    case SOS_VAL_TYPE_DOUBLE:
        data->val = pack_val;
        SOS_TIME(data->time.pack);
        break;
    }

    data->state = SOS_VAL_STATE_DIRTY;

    return;
}

SOS_val SOS_get_val(SOS_pub *pub, char *name) {
    int i;

    /* TODO:{ GET_VAL } Add hashtable lookup to improve search time in linear array. */

    for(i = 0; i < pub->elem_count; i++) {
        if (strcmp(name, pub->data[i]->name) == 0) return pub->data[i]->val;
    }

    return (SOS_val) 0;

}


SOS_sub* SOS_new_sub() {
    SOS_sub *new_sub;

    new_sub = malloc(sizeof(SOS_sub));
    memset(new_sub, '\0', sizeof(SOS_sub));
    new_sub->active = 1;
    new_sub->pub = SOS_pub_create("---empty---");

    return new_sub;
}

void SOS_free_pub(SOS_pub *pub) {

    /* TODO:{ FREE_PUB, CHAD } */
  
    return;
}

void SOS_free_sub(SOS_sub *sub) {

    /* TODO:{ FREE_SUB, CHAD } */

    return;
}



void SOS_display_pub(SOS_pub *pub, FILE *output_to) {
    int i;
    int rank;
    
    /* TODO:{ DISPLAY_PUB, CHAD }
     *
     * This needs to get cleaned up and restored to a the useful CSV/TSV that it was.
     */
    
    const char *SOS_TYPE_LOOKUP[4] = {"SOS_VAL_TYPE_INT", "SOS_VAL_TYPE_LONG", "SOS_VAL_TYPE_DOUBLE", "SOS_VAL_TYPE_STRING"};
    
    fprintf(output_to, "\n/---------------------------------------------------------------\\\n");
    fprintf(output_to, "|  %15s(%4d) : origin   %19s : title |\n", pub->prog_name, pub->comm_rank, pub->title);
    fprintf(output_to, "|  %3d of %3d elements used.                                    |\n", pub->elem_count, pub->elem_max);
    fprintf(output_to, "|---------------------------------------------------------------|\n");
    fprintf(output_to, "|       index,          id,        type,                   name | = <value>\n");
    fprintf(output_to, "|---------------------------------------------------------------|\n");
    for (i = 0; i < pub->elem_count; i++) {
        fprintf(output_to, " %c %20s | = ", ((pub->data[i]->state == SOS_VAL_STATE_DIRTY) ? '*' : ' '), pub->data[i]->name);
        switch (pub->data[i]->type) {
        case SOS_VAL_TYPE_INT : fprintf(output_to, "%d", pub->data[i]->val.i_val); break;
        case SOS_VAL_TYPE_LONG : fprintf(output_to, "%ld", pub->data[i]->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE : fprintf(output_to, "%lf", pub->data[i]->val.d_val); break;
        case SOS_VAL_TYPE_STRING : fprintf(output_to, "\"%s\"", pub->data[i]->val.c_val); break; }
        fprintf(output_to, "\n");
    }
    fprintf(output_to, "\\---------------------------------------------------------------/\n\n");
    
    return;
}




/* **************************************** */
/* [pub]                                    */
/* **************************************** */



void SOS_val_snap_queue_init(SOS_val_snap_queue **queue_var) {
    SOS_SET_WHOAMI(whoami, "SOS_val_snap_queue_init");
    SOS_val_snap_queue *queue;

    *queue_var = (SOS_val_snap_queue *) malloc(sizeof(SOS_val_snap_queue));
    queue = *queue_var;

    dlog(5, "[%s]: Initializing a val_snap_queue:\n", whoami);

    dlog(5, "[%s]:   ... initializing queue->lock\n", whoami);
    queue->lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(queue->lock, NULL);

    dlog(5, "[%s]:   ... initializing queue->from (hash table)\n", whoami);
    queue->from = qhashtbl(SOS_DEFAULT_TABLE_SIZE);

    dlog(5, "[%s]:   ... done.\n", whoami);

    return;
}



void SOS_val_snap_enqueue(SOS_val_snap_queue *queue, SOS_pub *pub, int elem) {
    SOS_SET_WHOAMI(whoami, "SOS_val_snap_enqueue");
    char pub_id_str[SOS_DEFAULT_STRING_LEN];
    SOS_val_snap *new_snap;
    SOS_val val_copy;

    dlog(2, "[%s]: Creating and enqueue'ing a snapshot of \"%s\":\n", whoami, pub->data[elem]->name);

    if (queue == NULL) { return; }
    dlog(2, "[%s]:   ... LOCK queue->lock\n", whoami);
    pthread_mutex_lock(queue->lock);

    dlog(2, "[%s]:   ... initializing snapshot\n", whoami);
    new_snap = (SOS_val_snap *) malloc(sizeof(SOS_val_snap));
    memset(new_snap, '\0', sizeof(SOS_val_snap));

    memset(pub_id_str, '\0', SOS_DEFAULT_STRING_LEN);
    sprintf(pub_id_str, "%ld", pub->guid);

    val_copy = pub->data[elem]->val;
    if (pub->data[elem]->type == SOS_VAL_TYPE_STRING) {
        dlog(2, "[%s]:   ... allocating memory for a string (pub->data[%d]->val_len == %d)\n", whoami, elem, pub->data[elem]->val_len);
        val_copy.c_val = (char *) malloc (1 + pub->data[elem]->val_len);
        memset(val_copy.c_val, '\0', (1 + pub->data[elem]->val_len));
        memcpy(val_copy.c_val, pub->data[elem]->val.c_val, pub->data[elem]->val_len);
    }

    dlog(2, "[%s]:   ... assigning values to snapshot\n", whoami);

    new_snap->elem  = elem;
    new_snap->guid  = pub->data[elem]->guid;
    new_snap->val   = val_copy;
    new_snap->time  = pub->data[elem]->time;
    new_snap->frame = pub->frame;
    new_snap->next  = (void *) queue->from->get(queue->from, pub_id_str);
    queue->from->remove(queue->from, pub_id_str);

    dlog(2, "[%s]:      &(%ld) new_snap->elem  = %d\n", whoami, (long) new_snap, new_snap->elem);
    dlog(2, "[%s]:      &(%ld) new_snap->guid  = %ld\n", whoami, (long) new_snap, new_snap->guid);
    switch(pub->data[elem]->type) {
    case SOS_VAL_TYPE_INT:     dlog(2, "[%s]:      &(%ld) new_snap->val   = %d (int)\n",     whoami, (long) new_snap, new_snap->val.i_val); break;
    case SOS_VAL_TYPE_LONG:    dlog(2, "[%s]:      &(%ld) new_snap->val   = %ld (long)\n",   whoami, (long) new_snap, new_snap->val.l_val); break;
    case SOS_VAL_TYPE_DOUBLE:  dlog(2, "[%s]:      &(%ld) new_snap->val   = %lf (double)\n", whoami, (long) new_snap, new_snap->val.d_val); break;
    case SOS_VAL_TYPE_STRING:  dlog(2, "[%s]:      &(%ld) new_snap->val   = %s (string)\n",  whoami, (long) new_snap, new_snap->val.c_val); break;
    }
    dlog(2, "[%s]:      &(%ld) new_snap->frame = %ld\n", whoami, (long) new_snap, new_snap->frame);
    dlog(2, "[%s]:      &(%ld) new_snap->next  = %ld\n", whoami, (long) new_snap, (long) new_snap->next);
    dlog(2, "[%s]:   ... making this the new head of the val_snap queue\n", whoami);

    /* We're hashing into per-pub val_snap stacks... (say that 5 times fast) */
    queue->from->put(queue->from, pub_id_str, (void *) new_snap);

    dlog(2, "[%s]:   ... UNLOCK queue->lock\n", whoami);
    pthread_mutex_unlock(queue->lock);

    dlog(2, "[%s]:   ... done.\n", whoami);

    return;
}


void SOS_val_snap_queue_to_buffer(SOS_val_snap_queue *queue, SOS_pub *pub, char **buf_ptr, int *buf_len, bool drain) {
    SOS_SET_WHOAMI(whoami, "SOS_val_snap_queue_to_buffer");
    SOS_msg_header header;
    char pub_guid_str[SOS_DEFAULT_STRING_LEN];
    char *buffer;
    char *ptr;
    int   buffer_len;
    SOS_val_snap *snap;

    memset(pub_guid_str, '\0', SOS_DEFAULT_STRING_LEN);
    snprintf(pub_guid_str, SOS_DEFAULT_STRING_LEN, "%ld", pub->guid);

    buffer = *buf_ptr;
    buffer_len = 0;
    ptr = (buffer + buffer_len);

    dlog(2, "[%s]:   ... building buffer from the val_snap queue:\n", whoami);
    dlog(2, "[%s]:      ... LOCK queue->lock\n", whoami);
    pthread_mutex_lock( queue->lock );
    dlog(2, "[%s]:      ... processing header\n", whoami);

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_VAL_SNAPS;
    header.msg_from = SOS.my_guid;
    header.pub_guid = pub->guid;

    buffer_len += SOS_buffer_pack(ptr, "iill",
                                  header.msg_size,
                                  header.msg_type,
                                  header.msg_from,
                                  header.pub_guid);
    ptr = (buffer + buffer_len);

    dlog(2, "[%s]:      ... processing snaps extracted from the queue\n", whoami);
    snap = (SOS_val_snap *) queue->from->get(queue->from, pub_guid_str);

    while (snap != NULL) {
    dlog(2, "[%s]:      ... guid=%ld\n", whoami, snap->guid);

        buffer_len += SOS_buffer_pack(ptr, "ildddl",
                                      snap->elem,
                                      snap->guid,
                                      snap->time.pack,
                                      snap->time.send,
                                      snap->time.recv,
                                      snap->frame);
        ptr = (buffer + buffer_len);

        switch (pub->data[snap->elem]->type) {
        case SOS_VAL_TYPE_INT:    buffer_len += SOS_buffer_pack(ptr, "i", snap->val.i_val); break;
        case SOS_VAL_TYPE_LONG:   buffer_len += SOS_buffer_pack(ptr, "l", snap->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE: buffer_len += SOS_buffer_pack(ptr, "d", snap->val.d_val); break;
        case SOS_VAL_TYPE_STRING: buffer_len += SOS_buffer_pack(ptr, "s", snap->val.c_val); break;
        }
        ptr = (buffer + buffer_len);
        snap = snap->next;
    }
    if (drain) {
        dlog(2, "[%s]:      ... draining queue for pub(%s)\n", whoami, pub_guid_str);
        SOS_val_snap_queue_drain(queue, pub);
    }

    dlog(2, "[%s]:      ... LOCK queue->lock\n", whoami);
    pthread_mutex_unlock( queue->lock );

    *buf_len        = buffer_len;
    header.msg_size = buffer_len;
    SOS_buffer_pack(buffer, "i", header.msg_size);
    dlog(2, "[%s]:      ... done   (buf_len == %d)\n", whoami, *buf_len);
   
    return;
}



void SOS_val_snap_queue_from_buffer(SOS_val_snap_queue *queue, qhashtbl_t *pub_table, char *buffer, int buffer_size) {
    SOS_SET_WHOAMI(whoami, "SOS_val_snap_queue_from_buffer");
    SOS_msg_header header;
    char pub_guid_str[SOS_DEFAULT_STRING_LEN];
    char *ptr;
    int   offset;
    SOS_val_snap *snap;
    SOS_pub *pub;
    int string_len;

    offset = 0;
    ptr = (buffer + offset);

    dlog(2, "[%s]:   ... building val_snap queue from a buffer:\n", whoami);
    dlog(2, "[%s]:      ... LOCK queue->lock\n", whoami);
    pthread_mutex_lock( queue->lock );
    dlog(2, "[%s]:      ... processing header\n", whoami);

    offset += SOS_buffer_unpack(ptr, "iill",
                                  &header.msg_size,
                                  &header.msg_type,
                                  &header.msg_from,
                                  &header.pub_guid);
    ptr = (buffer + offset);

    dlog(2, "[%s]:      ... header.msg_size == %d\n", whoami, header.msg_size);
    dlog(2, "[%s]:      ... header.msg_type == %d\n", whoami, header.msg_type);
    dlog(2, "[%s]:      ... header.msg_from == %ld\n", whoami, header.msg_from);
    dlog(2, "[%s]:      ... header.pub_guid == %ld\n", whoami, header.pub_guid);

    memset(pub_guid_str, '\0', SOS_DEFAULT_STRING_LEN);
    snprintf(pub_guid_str, SOS_DEFAULT_STRING_LEN, "%ld", header.pub_guid);

    pub = (SOS_pub *) pub_table->get(pub_table, pub_guid_str);
    
    if (pub == NULL) {
        dlog(1, "[%s]: WARNING! Attempting to build snap_queue for a pub we don't know about.\n", whoami);
        dlog(1, "[%s]:   ... skipping this request.\n", whoami);
        pthread_mutex_unlock( queue->lock );
        return;
    }
    

    dlog(2, "[%s]:      ... pushing snaps down onto the queue.\n", whoami);

    while (offset < buffer_size) {
        snap = (SOS_val_snap *) malloc(sizeof(SOS_val_snap));
        memset(snap, '\0', sizeof(SOS_val_snap));

        offset += SOS_buffer_unpack(ptr, "ildddl",
                                    &snap->elem,
                                    &snap->guid,
                                    &snap->time.pack,
                                    &snap->time.send,
                                    &snap->time.recv,
                                    &snap->frame);
        ptr = (buffer + offset);

        switch (pub->data[snap->elem]->type) {
        case SOS_VAL_TYPE_INT:    offset += SOS_buffer_unpack(ptr, "i", &snap->val.i_val); break;
        case SOS_VAL_TYPE_LONG:   offset += SOS_buffer_unpack(ptr, "l", &snap->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE: offset += SOS_buffer_unpack(ptr, "d", &snap->val.d_val); break;
        case SOS_VAL_TYPE_STRING:
            /* Extract only the length so we know how much space to allocate. */
            SOS_buffer_unpack(ptr, "i", &string_len);
            snap->val.c_val = (char *) malloc(1 + string_len);
            memset(snap->val.c_val, '\0', (1 + string_len));
            offset += SOS_buffer_unpack(ptr, "s", snap->val.c_val);
            break;
        }
        ptr = (buffer + offset);
        SOS_val_snap_push_down(queue, pub_guid_str, snap, 0);
    }

    dlog(2, "[%s]:      ... UNLOCK queue->lock\n", whoami);
    pthread_mutex_unlock( queue->lock );
    dlog(2, "[%s]:      ... done\n", whoami);

    return;
}



void SOS_val_snap_push_down(SOS_val_snap_queue *queue, char *pub_guid_str, SOS_val_snap *snap, int use_lock) {
    SOS_SET_WHOAMI(whoami, "SOS_val_snap_push_down");

    if (use_lock) { dlog(1, "[%s]: LOCK queue->lock\n", whoami); pthread_mutex_lock(queue->lock); }
    snap->next = (SOS_val_snap *) queue->from->get(queue->from, pub_guid_str);
    queue->from->remove(queue->from, pub_guid_str);
    queue->from->put(queue->from, pub_guid_str, (void *) snap);
    if (use_lock) { dlog(1, "[%s]: UNLOCK queue->lock\n", whoami); pthread_mutex_unlock(queue->lock); }

    return;
}



 void SOS_val_snap_queue_drain(SOS_val_snap_queue *queue, SOS_pub *pub) {
    SOS_val_snap *snap;
    SOS_val_snap *next_snap;
    char pub_guid_str[SOS_DEFAULT_STRING_LEN];

    memset(pub_guid_str, '\0', SOS_DEFAULT_STRING_LEN);
    snprintf(pub_guid_str, SOS_DEFAULT_STRING_LEN, "%ld", pub->guid);

    snap = (SOS_val_snap *) queue->from->get(queue->from, pub_guid_str);
    queue->from->remove(queue->from, pub_guid_str);
    
    while (snap != NULL) {
        next_snap = snap->next;
        if(pub->data[snap->elem]->type == SOS_VAL_TYPE_STRING) {
            free(snap->val.c_val);
        }
        free(snap);
        snap = next_snap;
    }

    return;
}



void SOS_val_snap_queue_destroy(SOS_val_snap_queue *queue) {
    SOS_SET_WHOAMI(whoami, "SOS_val_snap_queue_destroy");

    pthread_mutex_destroy(queue->lock);
    free(queue->lock);

    queue->from->free(queue->from);
    free(queue);

    return;
}






/* WARNING: For simplicity's sake and for performance, this routine does not
 *          perform any buffer size safety checks.
 */
void SOS_announce_to_buffer( SOS_pub *pub, char **buf_ptr, int *buf_len ) {
    SOS_SET_WHOAMI(whoami, "SOS_announce_to_buffer");
    SOS_msg_header header;
    char *buffer;
    char *ptr;
    int   buffer_len;
    int   elem;

    buffer = *buf_ptr;
    buffer_len = 0;
    ptr = buffer;

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_ANNOUNCE;
    header.msg_from = SOS.my_guid;
    header.pub_guid = pub->guid;
    /* Pack the header, (we'll re-pack the header.msg_size at the end) */
    buffer_len += SOS_buffer_pack(ptr, "iill",
        header.msg_size,
        header.msg_type,
        header.msg_from,
        header.pub_guid);
    ptr = (buffer + buffer_len);

    /* Pack the pub definition: */
    buffer_len += SOS_buffer_pack(ptr, "siiississiiiiiii",
        pub->node_id,
        pub->process_id,
        pub->thread_id,
        pub->comm_rank,
        pub->prog_name,
        pub->prog_ver,
        pub->pragma_len,
        pub->pragma_msg,
        pub->title,
        pub->elem_count,
        pub->meta.channel,
        pub->meta.layer,
        pub->meta.nature,
        pub->meta.pri_hint,
        pub->meta.scope_hint,
        pub->meta.retain_hint);
    ptr = (buffer + buffer_len);

    /* Pack the data definitions: */
    for (elem = 0; elem < pub->elem_count; elem++) {
        buffer_len += SOS_buffer_pack(ptr, "lsiiiiiii",
            pub->data[elem]->guid,
            pub->data[elem]->name,
            pub->data[elem]->type,
            pub->data[elem]->meta.freq,
            pub->data[elem]->meta.semantic,
            pub->data[elem]->meta.class,
            pub->data[elem]->meta.pattern,
            pub->data[elem]->meta.compare,
            pub->data[elem]->meta.mood );
        ptr = (buffer + buffer_len);
    }

    /* Re-pack the message size now that we know it. */
    header.msg_size = buffer_len;
    SOS_buffer_pack(buffer, "i", header.msg_size);

    *buf_len = buffer_len;

    return;
}


void SOS_publish_to_buffer( SOS_pub *pub, char **buf_ptr, int *buf_len ) {
    SOS_SET_WHOAMI(whoami, "SOS_publish_to_buffer");
    SOS_msg_header header;
    char   *buffer;
    char   *ptr;
    long    this_frame;
    int     buffer_len;
    int     elem;
    double  send_time;

    buffer = *buf_ptr;
    buffer_len = 0;
    ptr = buffer;

    SOS_TIME( send_time );

    if (SOS.role == SOS_ROLE_CLIENT) {
        /* Only CLIENT updates the frame when sending, in case this is re-used
         * internally / on the backplane by the DB or DAEMON. */
        this_frame = pub->frame++;
    }

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_PUBLISH;
    header.msg_from = SOS.my_guid;
    header.pub_guid = pub->guid;
    /* Pack the header, (we'll re-pack the header.msg_size at the end) */
    buffer_len += SOS_buffer_pack(ptr, "iill",
        header.msg_size,
        header.msg_type,
        header.msg_from,
        header.pub_guid);
    ptr = (buffer + buffer_len);

    /* Pack in the frame of these elements: */
    buffer_len += SOS_buffer_pack(ptr, "l", this_frame);
    ptr = (buffer + buffer_len);

    /* Pack in the data elements. */
    for (elem = 0; elem < pub->elem_count; elem++) {
        if ( pub->data[elem]->state != SOS_VAL_STATE_DIRTY) { continue; }
        
        pub->data[elem]->state = SOS_VAL_STATE_CLEAN;
        pub->data[elem]->time.send = send_time;

        buffer_len += SOS_buffer_pack(ptr, "iddi",
            elem,
            pub->data[elem]->time.pack,
            pub->data[elem]->time.send,
            pub->data[elem]->val_len);
        ptr = (buffer + buffer_len);

        switch (pub->data[elem]->type) {
        case SOS_VAL_TYPE_INT:     buffer_len += SOS_buffer_pack(ptr, "i", pub->data[elem]->val.i_val); break;
        case SOS_VAL_TYPE_LONG:    buffer_len += SOS_buffer_pack(ptr, "l", pub->data[elem]->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE:  buffer_len += SOS_buffer_pack(ptr, "d", pub->data[elem]->val.d_val); break;
        case SOS_VAL_TYPE_STRING:  buffer_len += SOS_buffer_pack(ptr, "s", pub->data[elem]->val.c_val); break;
        }
        ptr = (buffer + buffer_len);
    }

    /* Re-pack the message size now that we know what it is. */
    header.msg_size = buffer_len;
    SOS_buffer_pack(buffer, "i", header.msg_size);

    *buf_len = buffer_len;

    return;
}


void SOS_announce_from_buffer( SOS_pub *pub, char *buf_ptr ) {
    SOS_SET_WHOAMI(whoami, "SOS_announce_from_buffer");
    SOS_msg_header header;
    char *buffer;
    char *ptr;
    int   buffer_pos;
    int   elem;

    dlog(6, "[%s]: Applying an ANNOUNCE from a buffer...\n", whoami);

    ptr        = buf_ptr;
    buffer     = buf_ptr;
    buffer_pos = 0;

    dlog(6, "[%s]:   ... unpacking the header.\n", whoami);
    /* Unpack the header */
    buffer_pos += SOS_buffer_unpack(ptr, "iill",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);
    ptr = (buffer + buffer_pos);

    dlog(6, "[%s]:   ::: ptr = %ld :::\n", whoami, (long int) ptr);

    pub->guid = header.pub_guid;

    dlog(6, "[%s]:   ... unpacking the pub definition.\n", whoami);
    /* Unpack the pub definition: */

    buffer_pos += SOS_buffer_unpack(ptr, "siiississiiiiiii",
         pub->node_id,
        &pub->process_id,
        &pub->thread_id,
        &pub->comm_rank,
         pub->prog_name,
         pub->prog_ver,
        &pub->pragma_len,
         pub->pragma_msg,
         pub->title,
        &elem,
        &pub->meta.channel,
        &pub->meta.layer,
        &pub->meta.nature,
        &pub->meta.pri_hint,
        &pub->meta.scope_hint,
        &pub->meta.retain_hint);

    ptr = (buffer + buffer_pos);

    dlog(6, "[%s]: pub->node_id = \"%s\"\n", whoami, pub->node_id);
    dlog(6, "[%s]: pub->process_id = %d\n", whoami, pub->process_id);
    dlog(6, "[%s]: pub->thread_id = %d\n", whoami, pub->thread_id);
    dlog(6, "[%s]: pub->comm_rank = %d\n", whoami, pub->comm_rank);
    dlog(6, "[%s]: pub->prog_name = \"%s\"\n", whoami, pub->prog_name);
    dlog(6, "[%s]: pub->prog_ver = \"%s\"\n", whoami, pub->prog_ver);
    dlog(6, "[%s]: pub->pragma_len = %d\n", whoami, pub->pragma_len);
    dlog(6, "[%s]: pub->pragma_msg = \"%s\"\n", whoami, pub->pragma_msg);
    dlog(6, "[%s]: pub->title = \"%s\"\n", whoami, pub->title);
    dlog(6, "[%s]: pub->elem_count = %d\n", whoami, elem);
    dlog(6, "[%s]: pub->meta.channel = %d\n", whoami, pub->meta.channel);
    dlog(6, "[%s]: pub->meta.layer = %d\n", whoami, pub->meta.layer);
    dlog(6, "[%s]: pub->meta.nature = %d\n", whoami, pub->meta.nature);
    dlog(6, "[%s]: pub->meta.pri_hint = %d\n", whoami, pub->meta.pri_hint);
    dlog(6, "[%s]: pub->meta.scope_hint = %d\n", whoami, pub->meta.scope_hint);
    dlog(6, "[%s]: pub->meta.retain_hint = %d\n", whoami, pub->meta.retain_hint);

    /* Ensure there is room in this pub to handle incoming data definitions. */
    while(pub->elem_max < elem) {
        dlog(6, "[%s]:   ... doubling pub->elem_max from %d to handle %d elements...\n", whoami, pub->elem_max, elem);
        SOS_expand_data(pub);
    }
    pub->elem_count = elem;

    dlog(6, "[%s]:   ... unpacking the data definitions.\n", whoami);
    /* Unpack the data definitions: */
    elem = 0;
    for (elem = 0; elem < pub->elem_count; elem++) {
        buffer_pos += SOS_buffer_unpack(ptr, "lsiiiiiii",
            &pub->data[elem]->guid,
            pub->data[elem]->name,
            &pub->data[elem]->type,
            &pub->data[elem]->meta.freq,
            &pub->data[elem]->meta.semantic,
            &pub->data[elem]->meta.class,
            &pub->data[elem]->meta.pattern,
            &pub->data[elem]->meta.compare,
            &pub->data[elem]->meta.mood );

        /* For strings, we can't store them directly in the struct, so we need to initialize some space for them. */
        if ( pub->data[elem]->type == SOS_VAL_TYPE_STRING ) {
            if (!pub->data[elem]->val.c_val) {  /* In cases of re-announce we don't want a memory leak... */
                pub->data[elem]->val.c_val = (char *) malloc(SOS_DEFAULT_STRING_LEN);
                memset(pub->data[elem]->val.c_val, '\0', SOS_DEFAULT_STRING_LEN);
            }
        }

        ptr = (buffer + buffer_pos);
        dlog(6, "[%s]:   ... pub->data[%d]->guid = %ld\n", whoami, elem, pub->data[elem]->guid);
        dlog(6, "[%s]:   ... pub->data[%d]->name = %s\n", whoami, elem, pub->data[elem]->name);
        dlog(6, "[%s]:   ... pub->data[%d]->type = %d\n", whoami, elem, pub->data[elem]->type);
    }
    dlog(6, "[%s]:   ... done.\n", whoami);

    return;
}

 void SOS_publish_from_buffer( SOS_pub *pub, char *buf_ptr, SOS_val_snap_queue *opt_queue ) {
    SOS_SET_WHOAMI(whoami, "SOS_publish_from_buffer");
    SOS_msg_header header;
    char *buffer;
    char *ptr;
    long  this_frame;
    int   buffer_pos;
    int   elem;

    ptr        = buf_ptr;
    buffer     = buf_ptr;
    buffer_pos = 0;

    dlog(7, "[%s]: Unpacking the values from the buffer...\n", whoami);

    /* Unpack the header */
    buffer_pos += SOS_buffer_unpack(ptr, "iill",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);
    ptr = (buffer + buffer_pos);

    dlog(7, "[%s]:   ... header.msg_size = %d\n", whoami, header.msg_size);
    dlog(7, "[%s]:   ... header.msg_type = %d\n", whoami, header.msg_type);
    dlog(7, "[%s]:   ... header.msg_from = %ld\n", whoami, header.msg_from);
    dlog(7, "[%s]:   ... header.pub_guid = %ld\n", whoami, header.pub_guid);
    dlog(7, "[%s]:   ... values:\n", whoami);


    /* Unpack the frame: */
    buffer_pos += SOS_buffer_unpack(ptr, "l", &this_frame);
    ptr = (buffer + buffer_pos);

    pub->frame = this_frame;

    /* Unpack in the data elements. */
    while (buffer_pos < header.msg_size) {

        buffer_pos += SOS_buffer_unpack(ptr, "i", &elem);
        ptr = (buffer + buffer_pos);

        buffer_pos += SOS_buffer_unpack(ptr, "ddi",
            &pub->data[elem]->time.pack,
            &pub->data[elem]->time.send,
            &pub->data[elem]->val_len);
        ptr = (buffer + buffer_pos);

        switch (pub->data[elem]->type) {
        case SOS_VAL_TYPE_INT:    buffer_pos += SOS_buffer_unpack(ptr, "i", &pub->data[elem]->val.i_val); break;
        case SOS_VAL_TYPE_LONG:   buffer_pos += SOS_buffer_unpack(ptr, "l", &pub->data[elem]->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE: buffer_pos += SOS_buffer_unpack(ptr, "d", &pub->data[elem]->val.d_val); break;
        case SOS_VAL_TYPE_STRING:
            if (pub->data[elem]->val_len > SOS_DEFAULT_STRING_LEN) {
                free( pub->data[elem]->val.c_val );
                pub->data[elem]->val.c_val = (char *) malloc(1 + pub->data[elem]->val_len);
                memset(pub->data[elem]->val.c_val, '\0', (1 + pub->data[elem]->val_len));
            }
            buffer_pos += SOS_buffer_unpack(ptr, "s", pub->data[elem]->val.c_val);
            break;
        }
        ptr = (buffer + buffer_pos);

        /* Enqueue this value for writing out to local_sync and cloud_sync:
         * NOTE: Flushing *this* queue is triggered by the sync thread
         *       encounting this pub handle in the to-do queue. */
        if (opt_queue != NULL) {
            dlog(2, "[%s]: Enqueing a val_snap for \"%s\"\n", whoami, pub->data[elem]->name);
            SOS_val_snap_enqueue(opt_queue, pub, elem);
        }

    }

    dlog(7, "[%s]:   ... done.\n", whoami);

    return;
}


void SOS_announce( SOS_pub *pub ) {
    SOS_SET_WHOAMI(whoami, "SOS_announce");

    char *buffer;
    char  buffer_stack[SOS_DEFAULT_BUFFER_LEN];
    int   buffer_len;
    char *reply;
    char  reply_stack[SOS_DEFAULT_ACK_LEN];
    int   reply_max;

    dlog(6, "[%s]: Preparing an announcement message...\n",    whoami);
    dlog(6, "[%s]:   ... pub->guid       = %ld\n", whoami, pub->guid);
    dlog(6, "[%s]:   ... pub->title      = %s\n", whoami, pub->title);
    dlog(6, "[%s]:   ... pub->elem_count = %d\n", whoami, pub->elem_count);
    dlog(6, "[%s]:   ... pub->elem_max   = %d\n", whoami, pub->elem_max);

    memset(buffer_stack, '\0', SOS_DEFAULT_BUFFER_LEN);
    buffer     = buffer_stack;
    buffer_len = 0;
    memset(reply_stack,  '\0', SOS_DEFAULT_ACK_LEN);
    reply      = reply_stack;
    reply_max  = SOS_DEFAULT_ACK_LEN;

    dlog(6, "[%s]:   ... placing the announce message in a buffer.\n", whoami);
    SOS_announce_to_buffer(pub, &buffer, &buffer_len);
    dlog(6, "[%s]:   ... sending the buffer to the daemon.\n", whoami);
    SOS_send_to_daemon(buffer, buffer_len, reply, reply_max);
    dlog(6, "[%s]:   ... done.\n", whoami);
    pub->announced = 1;

    return;
}


void SOS_publish( SOS_pub *pub ) {
    SOS_SET_WHOAMI(whoami, "SOS_publish");

    char   *buffer;
    char    buffer_stack[SOS_DEFAULT_BUFFER_LEN];
    int     buffer_len;
    char   *reply;
    char    reply_stack[SOS_DEFAULT_ACK_LEN];
    int     reply_max;

    memset(buffer_stack, '\0', SOS_DEFAULT_BUFFER_LEN);
    buffer     = buffer_stack;
    buffer_len = 0;
    memset(reply_stack,  '\0', SOS_DEFAULT_ACK_LEN);
    reply     = reply_stack;
    reply_max = SOS_DEFAULT_ACK_LEN;
    
    if (pub->announced == 0) {
        dlog(6, "[%s]: AUTO-ANNOUNCING this pub...\n", whoami);
        SOS_announce( pub );
    }

    dlog(6, "[%s]: Preparing an announcement message...\n",    whoami);
    dlog(6, "[%s]:   ... pub->guid       = %ld\n", whoami, pub->guid);
    dlog(6, "[%s]:   ... pub->title      = %s\n", whoami, pub->title);
    dlog(6, "[%s]:   ... pub->elem_count = %d\n", whoami, pub->elem_count);
    dlog(6, "[%s]:   ... pub->elem_max   = %d\n", whoami, pub->elem_max);

    dlog(6, "[%s]:   ... placing the publish message in a buffer.\n", whoami);
    SOS_publish_to_buffer(pub, &buffer, &buffer_len);
    dlog(6, "[%s]:   ... sending the buffer to the daemon.\n", whoami);
    SOS_send_to_daemon(buffer, buffer_len, reply, reply_max);
    dlog(6, "[%s]:   ... done.\n", whoami);

    return;
}


void SOS_unannounce( SOS_pub *pub ) {

    /* TODO:{ UNANNOUNCE, CHAD } */

    return;
}


SOS_sub* SOS_subscribe( SOS_role source_role, int source_rank, char *pub_title, int refresh_delay ) {
    SOS_sub *new_sub;

    new_sub = (SOS_sub *) malloc( sizeof(SOS_sub) );
    memset(new_sub, '\0', sizeof(SOS_sub));

    return new_sub;
}


void* SOS_refresh_sub( void *arg ) {
    SOS_sub *sub = (SOS_sub*) arg;
    char *msg;
    int msg_len;

    while (sub->active == 1) {
        sleep(sub->refresh_delay);
    }
    return NULL;
}


void SOS_unsubscribe(SOS_sub *sub) {

    return;
}
