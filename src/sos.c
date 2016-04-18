
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
void*        SOS_THREAD_feedback(void *arg);
void         SOS_handle_feedback(SOS_runtime *sos_context, unsigned char *buffer, int buffer_length);
void         SOS_expand_data(SOS_pub *pub);

SOS_runtime* SOS_init_runtime(int *argc, char ***argv, SOS_role role, SOS_layer layer, SOS_runtime *extant_sos_runtime);

void         SOS_val_snap_queue_drain(SOS_val_snap_queue *queue, SOS_pub *pub);
             /* NOTE: Doesn't lock the queue, for use within queue functions when lock is held. */


/* Private variables (not exposed in the header file) */
int            SOS_NULL_STR_LEN  = sizeof(unsigned char);
unsigned char  SOS_NULL_STR_CHAR = '\0';
unsigned char *SOS_NULL_STR      = &SOS_NULL_STR_CHAR;

/* Global data structure */
SOS_runtime SOS;

/* **************************************** */
/* [util]                                   */
/* **************************************** */

SOS_runtime* SOS_init( int *argc, char ***argv, SOS_role role, SOS_layer layer) {
    return SOS_init_runtime(argc, argv, role, layer, NULL);
}

SOS_runtime* SOS_init_runtime( int *argc, char ***argv, SOS_role role, SOS_layer layer, SOS_runtime *extant_sos_runtime ) {
    SOS_msg_header header;
    unsigned char buffer[SOS_DEFAULT_REPLY_LEN] = {0};
    int i, n, retval, server_socket_fd;
    SOS_guid guid_pool_from;
    SOS_guid guid_pool_to;


    SOS_runtime *NEW_SOS = NULL;
    if (extant_sos_runtime == NULL) {
        NEW_SOS = (SOS_runtime *) malloc(sizeof(SOS_runtime));
        memset(NEW_SOS, '\0', sizeof(SOS_runtime));
    } else {
        NEW_SOS = extant_sos_runtime;
    }

    /*
     *  Before SOS_init returned a unique context per caller, we wanted
     *  to make it re-entrant.  This saved mistakes from being made when
     *  multiple parts of a single binary were independently instrumented
     *  with SOS calls reflecting different layers or metadata.
     *
     *  The way it works now, wrappers and libraries and applications
     *  can all have their own unique contexts and metadata, this is better.
     *
    static bool _initialized = false;   //old way
    if (_initialized) return;
    _initialized = true;
    */


    if (role == SOS_ROLE_OFFLINE_TEST_MODE) {
        NEW_SOS->config.offline_test_mode = true;
        NEW_SOS->role = SOS_ROLE_CLIENT;
    } else {
        NEW_SOS->config.offline_test_mode = false;
        NEW_SOS->role = role;
    }

    NEW_SOS->status = SOS_STATUS_INIT;
    NEW_SOS->config.layer  = layer;
    SOS_SET_CONTEXT(NEW_SOS, "SOS_init");

    dlog(1, "Initializing SOS ...\n");
    dlog(1, "  ... setting argc / argv\n");
    SOS->config.argc = *argc;
    SOS->config.argv = *argv;
    SOS->config.process_id = (int) getpid();

    SOS->config.node_id = (char *) malloc( SOS_DEFAULT_STRING_LEN );
    gethostname( SOS->config.node_id, SOS_DEFAULT_STRING_LEN );
    dlog(1, "  ... node_id: %s\n", SOS->config.node_id );

    dlog(1, "  ... configuring data rings.\n");
    SOS_ring_init(SOS, &SOS->ring.send);
    SOS_ring_init(SOS, &SOS->ring.recv);

    if (SOS->role == SOS_ROLE_CLIENT) {
        SOS_val_snap_queue_init(SOS, &SOS->task.val_intake);
        SOS->task.val_outlet = NULL;
    }

    if (SOS->role == SOS_ROLE_CLIENT) {

        #if (SOS_CONFIG_USE_THREAD_POOL > 0)
        SOS->task.feedback =            (pthread_t *) malloc(sizeof(pthread_t));
        SOS->task.feedback_lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
        SOS->task.feedback_cond =  (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
        
        dlog(1, "  ... launching libsos runtime thread[s].\n");
        retval = pthread_create( SOS->task.feedback, NULL, (void *) SOS_THREAD_feedback, (void *) SOS );
        if (retval != 0) { dlog(0, " ... ERROR (%d) launching SOS->task.feedback thread!  (%s)\n", retval, strerror(errno)); exit(EXIT_FAILURE); }
        retval = pthread_mutex_init(SOS->task.feedback_lock, NULL);
        if (retval != 0) { dlog(0, " ... ERROR (%d) creating SOS->task.feedback_lock!  (%s)\n", retval, strerror(errno)); exit(EXIT_FAILURE); }
        retval = pthread_cond_init(SOS->task.feedback_cond, NULL);
        if (retval != 0) { dlog(0, " ... ERROR (%d) creating SOS->task.feedback_cond!  (%s)\n", retval, strerror(errno)); exit(EXIT_FAILURE); }
        SOS->net.send_lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
        retval = pthread_mutex_init(SOS->net.send_lock, NULL);
        if (retval != 0) { dlog(0, " ... ERROR (%d) creating SOS->net.send_lock!  (%s)\n", retval, strerror(errno)); exit(EXIT_FAILURE); }
        #endif

    }


    if (SOS->config.offline_test_mode == true) {
        /* Here, the offline mode finishes up any non-networking initialization and bails out. */
        SOS_uid_init(SOS, &SOS->uid.local_serial, 0, SOS_DEFAULT_UID_MAX);
        SOS_uid_init(SOS, &SOS->uid.my_guid_pool, 0, SOS_DEFAULT_UID_MAX);
        SOS->my_guid = SOS_uid_next( SOS->uid.my_guid_pool );
        SOS->status = SOS_STATUS_RUNNING;
        dlog(1, "  ... done with SOS_init().  [OFFLINE_TEST_MODE]\n");
        dlog(1, "SOS->status = SOS_STATUS_RUNNING\n");
        return SOS;
    }

    if (SOS->role == SOS_ROLE_CLIENT) {
        /* NOTE: This is only used for clients.  Daemons handle their own. */
        char *env_rank;
        char *env_size;

        env_rank = getenv("PMI_RANK");
        env_size = getenv("PMI_SIZE");
        if ((env_rank!= NULL) && (env_size != NULL)) {
            /* MPICH_ */
            SOS->config.comm_rank = atoi(env_rank);
            SOS->config.comm_size = atoi(env_size);
            dlog(1, "  ... MPICH environment detected. (rank: %d/ size:%d)\n", SOS->config.comm_rank, SOS->config.comm_size);
        } else {
            env_rank = getenv("OMPI_COMM_WORLD_RANK");
            env_size = getenv("OMPI_COMM_WORLD_SIZE");
            if ((env_rank != NULL) && (env_size != NULL)) {
                /* OpenMPI */
                SOS->config.comm_rank = atoi(env_rank);
                SOS->config.comm_size = atoi(env_size);
                dlog(1, "  ... OpenMPI environment detected. (rank: %d/ size:%d)\n", SOS->config.comm_rank, SOS->config.comm_size);
            } else {
                /* non-MPI client. */
                SOS->config.comm_rank = 0;
                SOS->config.comm_size = 1;
                dlog(1, "  ... Non-MPI environment detected. (rank: %d/ size:%d)\n", SOS->config.comm_rank, SOS->config.comm_size);
            }
        }
    }

    if (SOS->role == SOS_ROLE_CLIENT) {
        /*
         *
         *  CLIENT
         *
         */
        dlog(1, "  ... setting up socket communications with the daemon.\n" );

        SOS->net.buffer_len    = SOS_DEFAULT_BUFFER_LEN;
        SOS->net.timeout       = SOS_DEFAULT_MSG_TIMEOUT;
        SOS->net.server_host   = SOS_DEFAULT_SERVER_HOST;
        SOS->net.server_port   = getenv("SOS_CMD_PORT");
        if ( SOS->net.server_port == NULL ) { fprintf(stderr, "ERROR!  SOS_CMD_PORT environment variable is not set!\n"); exit(EXIT_FAILURE); }
        if ( strlen(SOS->net.server_port) < 2 ) { fprintf(stderr, "ERROR!  SOS_CMD_PORT environment variable is not set!\n"); exit(EXIT_FAILURE); }

        SOS->net.server_hint.ai_family    = AF_UNSPEC;        /* Allow IPv4 or IPv6 */
        SOS->net.server_hint.ai_protocol  = 0;                /* Any protocol */
        SOS->net.server_hint.ai_socktype  = SOCK_STREAM;      /* SOCK_STREAM vs. SOCK_DGRAM vs. SOCK_RAW */
        SOS->net.server_hint.ai_flags     = AI_NUMERICSERV | SOS->net.server_hint.ai_flags;

            retval = getaddrinfo(SOS->net.server_host, SOS->net.server_port, &SOS->net.server_hint, &SOS->net.result_list );
            if ( retval < 0 ) { dlog(0, "ERROR!  Could not locate the SOS daemon.  (%s:%s)\n", SOS->net.server_host, SOS->net.server_port ); exit(1); }

        for ( SOS->net.server_addr = SOS->net.result_list ; SOS->net.server_addr != NULL ; SOS->net.server_addr = SOS->net.server_addr->ai_next ) {
            /* Iterate the possible connections and register with the SOS daemon: */
            server_socket_fd = socket(SOS->net.server_addr->ai_family, SOS->net.server_addr->ai_socktype, SOS->net.server_addr->ai_protocol );
            if ( server_socket_fd == -1 ) continue;
            if ( connect(server_socket_fd, SOS->net.server_addr->ai_addr, SOS->net.server_addr->ai_addrlen) != -1 ) break; /* success! */
            close( server_socket_fd );
        }

        freeaddrinfo( SOS->net.result_list );
        
        if (server_socket_fd == 0) { dlog(0, "ERROR!  Could not connect to the server.  (%s:%s)\n", SOS->net.server_host, SOS->net.server_port); exit(1); }

        dlog(1, "  ... registering this instance with SOS->   (%s:%s)\n", SOS->net.server_host, SOS->net.server_port);

        header.msg_size = sizeof(SOS_msg_header);
        header.msg_type = SOS_MSG_TYPE_REGISTER;
        header.msg_from = 0;
        header.pub_guid = 0;

        memset(buffer, '\0', SOS_DEFAULT_REPLY_LEN);

        SOS_buffer_pack(SOS, buffer, "iigg", 
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.pub_guid);
        
        retval = sendto( server_socket_fd, buffer, sizeof(SOS_msg_header), 0, 0, 0 );
        if (retval < 0) { dlog(0, "ERROR!  Could not write to server socket!  (%s:%s)\n", SOS->net.server_host, SOS->net.server_port); exit(1); }

        dlog(1, "  ... listening for the server to reply...\n");
        memset(buffer, '\0', SOS_DEFAULT_REPLY_LEN);
        retval = recv( server_socket_fd, (void *) buffer, SOS_DEFAULT_REPLY_LEN, 0);

        dlog(6, "  ... server responded with %d bytes.\n", retval);
        memcpy(&guid_pool_from, buffer, sizeof(SOS_guid));
        memcpy(&guid_pool_to, (buffer + sizeof(SOS_guid)), sizeof(SOS_guid));
        dlog(1, "  ... received guid range from %" SOS_GUID_FMT " to %" SOS_GUID_FMT ".\n", guid_pool_from, guid_pool_to);
        dlog(1, "  ... configuring uid sets.\n");

        SOS_uid_init(SOS, &SOS->uid.local_serial, 0, SOS_DEFAULT_UID_MAX);
        SOS_uid_init(SOS, &SOS->uid.my_guid_pool, guid_pool_from, guid_pool_to);   /* DAEMON doesn't use this, it's for CLIENTS. */

        SOS->my_guid = SOS_uid_next( SOS->uid.my_guid_pool );
        dlog(1, "  ... SOS->my_guid == %" SOS_GUID_FMT "\n", SOS->my_guid);

        close( server_socket_fd );

    } else {
        /*
         *
         *  CONFIGURATION: DAEMON / DATABASE / etc.
         *
         */

        dlog(0, "  ... skipping socket setup (becase we're the daemon).\n");
    }

    SOS->status = SOS_STATUS_RUNNING;

    dlog(1, "  ... done with SOS_init().\n");
    dlog(1, "SOS->status = SOS_STATUS_RUNNING\n");
    return SOS;
}


/* TODO */
int SOS_sense_register(SOS_runtime *sos_context, char *handle, void (*your_callback)(void *data)) {
    SOS_SET_CONTEXT(sos_context, "SOS_sense_register");



    return 0;
}


 /* TODO */
void SOS_sense_activate(SOS_runtime *sos_context, char *handle, SOS_layer layer, void *data, int data_length) {
    SOS_SET_CONTEXT(sos_context, "SOS_sense_activate");

    return;
}



void SOS_async_buf_pair_init(SOS_runtime *sos_context, SOS_async_buf_pair **buf_pair_ptr) {
    SOS_SET_CONTEXT(sos_context, "SOS_async_buf_pair_init");
    SOS_async_buf_pair *buf_pair;

    *buf_pair_ptr = (SOS_async_buf_pair *) malloc(sizeof(SOS_async_buf_pair));
    buf_pair = *buf_pair_ptr;
    memset(buf_pair, '\0', sizeof(SOS_async_buf_pair));

    buf_pair->sos_context = sos_context;
    
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

    buf_pair->flush_lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(buf_pair->flush_lock, NULL);
    buf_pair->flush_cond = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
    pthread_cond_init(buf_pair->flush_cond, NULL);

    return;
}


void SOS_async_buf_pair_fflush(SOS_async_buf_pair *buf_pair) {
    SOS_SET_CONTEXT(buf_pair->sos_context, "SOS_async_buf_pair_fflush");

    /* The thread that actually does the 'flushing' is set up by the
     * context that creats the buf_pair.  Specifically, all we do here
     * is grab the locks, swap the roles of the buffers, trigger the
     * flush condition, and release the lock.
     * Immediately, grow_buf is available to handle new
     * writes, because the facilitating thread does not go back to sleep
     * and release the flush lock until the sending buffer has been
     * entirely handled. */

    dlog(1, "Forcing a flush...\n");
    if (SOS->status != SOS_STATUS_RUNNING) {
        dlog(1, "  ... skipping buffer flush, system is shutting down\n");
        return;
    }
    dlog(7, "  ... LOCK buf_pair->flush_lock\n");
    pthread_mutex_lock(buf_pair->flush_lock);
    dlog(7, "  ... LOCK buf_pair->send_buf->lock\n");
    pthread_mutex_lock(buf_pair->send_buf->lock);
    dlog(7, "  ... LOCK buf_pair->grow_buf->lock\n");
    pthread_mutex_lock(buf_pair->grow_buf->lock);


    dlog(1, "  ... swap buffers\n");
    if (buf_pair->send_buf == &buf_pair->a) {
        buf_pair->grow_buf = &buf_pair->a;
        buf_pair->send_buf = &buf_pair->b;
    } else {
        buf_pair->grow_buf = &buf_pair->b;
        buf_pair->send_buf = &buf_pair->a;
    }
    dlog(7, "  ... UNLOCK buf_pair->send_buf->lock\n");
    pthread_mutex_unlock(buf_pair->send_buf->lock);
    dlog(7, "  ... UNLOCK buf_pair->grow_buf->lock\n");
    pthread_mutex_unlock(buf_pair->grow_buf->lock);

    dlog(7, "  ... signal flush condition\n");
    pthread_cond_signal(buf_pair->flush_cond);
    dlog(7, "  ... UNLOCK buf_pair->flush_lock\n");
    pthread_mutex_unlock(buf_pair->flush_lock);

    return;
}

void SOS_async_buf_pair_insert(SOS_async_buf_pair *buf_pair, unsigned char *msg_ptr, int msg_len) {
    SOS_SET_CONTEXT(buf_pair->sos_context, "SOS_async_buf_pair_insert");
    SOS_buf *buf;
    int count;
    int count_size;

    buf = buf_pair->grow_buf;

    dlog(7, "  ... LOCK grow_buf->lock\n");
    pthread_mutex_lock(buf->lock);
    count_size = SOS_buffer_unpack(SOS, buf->data, "i", &count);

    /* NOTE: '8' to make sure we'll have room to pack in the msg_len... */
    if ((8 + msg_len) > buf->max) {
        dlog(0, "WARNING! You've attempted to insert a value that is larger than the buffer!  (msg_len == %d)\n", msg_len);
        dlog(0, "WARNING! Skipping this value, but the system is no longer tracking all entries.\n");
        dlog(7, "  ... UNLOCK grow_buf->lock\n");
        pthread_mutex_unlock(buf->lock);
        return;
    }

    while ((8 + msg_len + buf->len) > buf->max) {
        pthread_mutex_unlock(buf->lock);
        SOS_async_buf_pair_fflush(buf_pair);
        buf = buf_pair->grow_buf;
        pthread_mutex_lock(buf->lock);
    }
    /* Now buf points to an totally empty/available buffer,
     * and we know our value fits, even if multiple threads
     * are hitting it. */

    /* NOTE: We can't use SOS_buffer_pack() for the msg because, being already
     *       the product of packing, msg may contain inline null zero's and is
     *       not useable as a 'string'.  We use memcpy on it instead.  This will
     *       be true on the receiving/processing side of this buffer as well. */
    memcpy((buf->data + buf->len + count_size), msg_ptr, msg_len);
    buf->len += msg_len;

    buf->entry_count++;
    dlog(7, "buf->entry_count == %d\n", buf->entry_count);

    SOS_buffer_pack(SOS, buf->data, "i", buf->entry_count);

    dlog(7, "  ... UNLOCK grow_buf->lock\n");
    pthread_mutex_unlock(buf->lock);

    return;
}


void SOS_async_buf_pair_destroy(SOS_async_buf_pair *buf_pair) {
    SOS_SET_CONTEXT(buf_pair->sos_context, "SOS_async_buf_pair_destroy");

    /* Naturally, exit/join any affiliated threads before calling this, as
     * this wipes out the condition variable that the thread might be
     * waiting on.  */

    dlog(7, "  ... send_buf mutex\n");
    dlog(7, "     ... LOCK send_buf->lock\n");
    pthread_mutex_lock(buf_pair->send_buf->lock);
    dlog(7, "     ... UNLOCK send_buf->lock (destroy)\n");
    pthread_mutex_destroy(buf_pair->send_buf->lock);

    dlog(7, "  ... grow_buf mutex\n");
    dlog(7, "     ... LOCK grow_buf->lock\n");
    pthread_mutex_lock(buf_pair->grow_buf->lock);
    dlog(7, "     ... UNLOCK grow_buf->lock (destroy)\n");
    pthread_mutex_destroy(buf_pair->grow_buf->lock);

    dlog(7, "  ... flush_lock mutex\n");
    dlog(7, "     ... LOCK bp->flush_lock\n");
    pthread_mutex_lock(buf_pair->flush_lock);
    dlog(7, "     ... UNLOCK bp->flush_lock (destroy)\n");
    pthread_mutex_destroy(buf_pair->flush_lock);

    free(buf_pair->a.lock);
    free(buf_pair->b.lock);
    pthread_cond_destroy(buf_pair->flush_cond);
    free(buf_pair->flush_cond);

    memset(buf_pair, '\0', sizeof(SOS_async_buf_pair));
    free(buf_pair);

    return;
}



void SOS_ring_init(SOS_runtime *sos_context, SOS_ring_queue **ring_var) {
    SOS_SET_CONTEXT(sos_context, "SOS_ring_init");
    SOS_ring_queue *ring;

    dlog(5, "  ... initializing ring_var @ %ld\n", (long) ring_var);
    ring = *ring_var = (SOS_ring_queue *) malloc(sizeof(SOS_ring_queue));
    ring->read_elem  = ring->write_elem  = 0;
    ring->elem_count = 0;
    ring->elem_max   = SOS_DEFAULT_RING_SIZE;
    ring->elem_size  = sizeof(long);
    ring->heap       = (long *) malloc( ring->elem_max * ring->elem_size );
    memset( ring->heap, '\0', (ring->elem_max * ring->elem_size) );

    ring->sos_context = sos_context;

    dlog(5, "     ... successfully initialized ring queue.\n");
    dlog(5, "     ... initializing mutex.\n");
    ring->lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(ring->lock, NULL);
    dlog(5, "  ... done.\n");

    return;
}

void SOS_ring_destroy(SOS_ring_queue *ring) {
    SOS_SET_CONTEXT(ring->sos_context, "SOS_ring_destroy");

    free(ring->lock);
    memset(ring->heap, '\0', (ring->elem_max * ring->elem_size));
    free(ring->heap);
    memset(ring, '\0', (sizeof(SOS_ring_queue)));
    free(ring);

    return;
}


int SOS_ring_put(SOS_ring_queue *ring, long item) {
    SOS_SET_CONTEXT(ring->sos_context, "SOS_ring_put");
    dlog(7, "LOCK ring->lock\n");
    pthread_mutex_lock(ring->lock);

    dlog(5, "Attempting to add (%ld) into the ring.\n", item);
    if (item == 0) { dlog(1, "  ... ERROR: Being asked to insert a '0' element!\n"); }
    if (ring == NULL) { dlog(0, "ERROR!  Attempted to insert into a NULL ring!\n"); exit(EXIT_FAILURE); }

    if (ring->elem_count >= ring->elem_max) {
        /* The ring is full... */
        dlog(5, "ERROR!  Attempting to insert a data element into a full ring!");
        dlog(7, "  ... UNLOCK ring->lock\n");
        pthread_mutex_unlock(ring->lock);
        return(-1);
    }
    
    ring->heap[ring->write_elem] = item;

    ring->write_elem = (ring->write_elem + 1) % ring->elem_max;
    ring->elem_count++;

    dlog(5, "  ... this is item %d of %d @ position %d.\n", ring->elem_count, ring->elem_max, (ring->write_elem - 1));
    dlog(5, "  ... UNLOCK ring->lock\n");
    pthread_mutex_unlock(ring->lock);
    dlog(5, "  ... done.\n");

    return(0);
}


long SOS_ring_get(SOS_ring_queue *ring) {
    SOS_SET_CONTEXT(ring->sos_context, "SOS_ring_get");
    long element;

    dlog(7, "LOCK ring->lock\n");
    pthread_mutex_lock(ring->lock);

    if (ring->elem_count == 0) {
        pthread_mutex_unlock(ring->lock);
        return -1;
    }

    element = ring->heap[ring->read_elem];
    ring->read_elem = (ring->read_elem + 1) % ring->elem_max;
    ring->elem_count--;

    dlog(7, "UNLOCK ring->lock\n");
    pthread_mutex_unlock(ring->lock);

    return element;
}


long* SOS_ring_get_all(SOS_ring_queue *ring, int *elem_returning) {
    SOS_SET_CONTEXT(ring->sos_context, "SOS_ring_get_all");
    long *elem_list;
    int elem_list_bytes;
    int fragment_count;

    dlog(7, "LOCK ring->lock\n");
    pthread_mutex_lock(ring->lock);

    elem_list_bytes = (ring->elem_count * ring->elem_size);
    elem_list = (long *) malloc(elem_list_bytes);
    memset(elem_list, '\0', elem_list_bytes);

    *elem_returning = ring->elem_count;

    memcpy(elem_list, &ring->heap[ring->read_elem], (ring->elem_count * ring->elem_size));
    ring->read_elem = ring->write_elem;
    ring->elem_count = 0;

    dlog(7, "UNLOCK ring->lock\n");
    pthread_mutex_unlock(ring->lock);

    return elem_list;
}

void SOS_send_to_daemon(SOS_runtime *sos_context, unsigned char *msg, int msg_len, unsigned char *reply, int reply_max ) {
    SOS_SET_CONTEXT(sos_context, "SOS_send_to_daemon");

    SOS_msg_header header;
    int server_socket_fd;
    int retval;

    if (SOS->status == SOS_STATUS_SHUTDOWN) {
        dlog(1, "Suppressing a send to the daemon.  (SOS_STATUS_SHUTDOWN)\n");
        return;
    }

    if (SOS->config.offline_test_mode == true) {
        dlog(1, "Suppressing a send to the daemon.  (OFFLINE_TEST_MODE)\n");
        return;
    }

    #if (SOS_CONFIG_USE_THREAD_POOL > 0)
    pthread_mutex_lock(SOS->net.send_lock);
    #endif

    retval = getaddrinfo(SOS->net.server_host, SOS->net.server_port, &SOS->net.server_hint, &SOS->net.result_list );
    if ( retval < 0 ) { dlog(0, "ERROR!  Could not locate the SOS daemon.  (%s:%s)\n", SOS->net.server_host, SOS->net.server_port ); exit(1); }
    
    /* Iterate the possible connections and register with the SOS daemon: */
    for ( SOS->net.server_addr = SOS->net.result_list ; SOS->net.server_addr != NULL ; SOS->net.server_addr = SOS->net.server_addr->ai_next ) {
        server_socket_fd = socket(SOS->net.server_addr->ai_family, SOS->net.server_addr->ai_socktype, SOS->net.server_addr->ai_protocol );
        if ( server_socket_fd == -1 ) continue;
        if ( connect( server_socket_fd, SOS->net.server_addr->ai_addr, SOS->net.server_addr->ai_addrlen) != -1 ) break; /* success! */
        close( server_socket_fd );
    }
    
    freeaddrinfo( SOS->net.result_list );
    
    if (server_socket_fd == 0) {
        dlog(0, "Error attempting to connect to the server.  (%s:%s)\n", SOS->net.server_host, SOS->net.server_port);
        exit(1);  /* TODO:{ SEND_TO_DAEMON }  Make this a loop that tries X times to connect, doesn't crash app. */
    }

    /* TODO: { SEND_TO_DAEMON } Make this a loop that ensures all data was sent. */
    retval = send(server_socket_fd, msg, msg_len, 0 );
    if (retval == -1) { dlog(0, "Error sending message to daemon.\n %s", strerror(errno)); }

    retval = recv(server_socket_fd, reply, reply_max, 0);
    if (retval == -1) { dlog(0, "Error receiving message from daemon.  (retval = %d, errno = %d:\"%s\")\n", retval, errno, strerror(errno)); }
    else { dlog(6, "Server sent a (%d) byte reply.\n", retval); }

    close( server_socket_fd );

    #if (SOS_CONFIG_USE_THREAD_POOL > 0)
    pthread_mutex_unlock(SOS->net.send_lock);
    #endif

    return;
}



void SOS_finalize(SOS_runtime *sos_context) {
    SOS_SET_CONTEXT(sos_context, "SOS_finalize");
    
    /* This will cause any SOS threads to leave their loops next time they wake up. */
    dlog(1, "SOS->status = SOS_STATUS_SHUTDOWN\n");
    SOS->status = SOS_STATUS_SHUTDOWN;

    free(SOS->config.node_id);

    if (SOS->role == SOS_ROLE_CLIENT) {
        #if (SOS_CONFIG_USE_THREAD_POOL > 0)
        dlog(1, "  ... Joining threads...\n");
        pthread_cond_signal(SOS->task.feedback_cond);
        pthread_join(*SOS->task.feedback, NULL);
        pthread_cond_destroy(SOS->task.feedback_cond);
        pthread_mutex_lock(SOS->task.feedback_lock);
        pthread_mutex_destroy(SOS->task.feedback_lock);
        free(SOS->task.feedback_lock);
        free(SOS->task.feedback_cond);
        free(SOS->task.feedback);

        dlog(1, "  ... Removing send lock...\n");
        pthread_mutex_lock(SOS->net.send_lock);
        pthread_mutex_destroy(SOS->net.send_lock);
        free(SOS->net.send_lock);
        #endif

        dlog(1, "  ... Releasing snapshot queue...\n");
        SOS_val_snap_queue_destroy(SOS->task.val_intake);

        dlog(1, "  ... Releasing uid objects...\n");
        SOS_uid_destroy(SOS->uid.local_serial);
        SOS_uid_destroy(SOS->uid.my_guid_pool);
    }

    dlog(1, "  ... Releasing ring queues...\n");
    SOS_ring_destroy(SOS->ring.send);
    SOS_ring_destroy(SOS->ring.recv);

    dlog(1, "Done!\n");
    free(SOS);

    return;
}




void* SOS_THREAD_feedback( void *args ) {
    SOS_runtime *local_ptr_to_context = (SOS_runtime *) args;
    SOS_SET_CONTEXT(local_ptr_to_context, "SOS_THREAD_feedback");
    struct timespec ts;
    struct timeval  tp;
    int wake_type;

    unsigned char *ptr;
    unsigned char *check_in_msg;
    unsigned char *feedback_msg;
    int ptr_offset;

    SOS_msg_header header;
    SOS_feedback feedback;

    if ( SOS->config.offline_test_mode == true ) { return NULL; }

    check_in_msg = (unsigned char *) malloc(SOS_DEFAULT_FEEDBACK_LEN * sizeof(unsigned char));
    feedback_msg = (unsigned char *) malloc(SOS_DEFAULT_FEEDBACK_LEN * sizeof(unsigned char));

    sleep(1);
    if (SOS->status != SOS_STATUS_SHUTDOWN) {
      sleep(4 + (random() % 5));
    }

    /* Set the wakeup time (ts) to 2 seconds in the future. */
    gettimeofday(&tp, NULL);
    ts.tv_sec  = (tp.tv_sec + 2);
    ts.tv_nsec = (1000 * tp.tv_usec) + 62500000;

    /* Grab the lock that the wakeup condition is bound to. */
    pthread_mutex_lock(SOS->task.feedback_lock);

    while (SOS->status != SOS_STATUS_SHUTDOWN) {
        /* Build a checkin message. */
        memset(check_in_msg, '\0', SOS_DEFAULT_FEEDBACK_LEN);
        memset(feedback_msg, '\0', SOS_DEFAULT_FEEDBACK_LEN);

        header.msg_size = -1;
        header.msg_from = SOS->my_guid;
        header.msg_type = SOS_MSG_TYPE_CHECK_IN;
        header.pub_guid = 0;

        ptr = check_in_msg;
        ptr_offset = 0;

        ptr_offset += SOS_buffer_pack(SOS, ptr, "iigg",
            header.msg_size,
            header.msg_type,
            header.msg_from,
            header.pub_guid);
        ptr = (check_in_msg + ptr_offset);

        header.msg_size = ptr_offset;
        SOS_buffer_pack(SOS, check_in_msg, "i", header.msg_size);

        /* Ping the daemon to see if there is anything to do. */
        SOS_send_to_daemon(SOS, (unsigned char *) check_in_msg, ptr_offset, feedback_msg, SOS_DEFAULT_FEEDBACK_LEN);

        memset(&header, '\0', sizeof(SOS_msg_header));
        ptr = feedback_msg;
        ptr_offset = 0;

        ptr_offset += SOS_buffer_unpack(SOS, ptr, "iigg",
            &header.msg_size,
            &header.msg_type,
            &header.msg_from,
            &header.pub_guid);
        ptr = (feedback_msg + ptr_offset);
        if (header.msg_type != SOS_MSG_TYPE_FEEDBACK) {
            dlog(0, "WARNING! --- sosd (daemon) responded to a CHECK_IN msg malformed FEEDBACK!\n");
            gettimeofday(&tp, NULL);
            ts.tv_sec  = (tp.tv_sec + 2);
            ts.tv_nsec = (1000 * tp.tv_usec) + 62500000;
            continue;
        }

        ptr_offset += SOS_buffer_unpack(SOS, ptr, "i",
            &feedback);
        ptr = (feedback_msg + ptr_offset);

        switch (feedback) {
        case SOS_FEEDBACK_CONTINUE: break;

        case SOS_FEEDBACK_EXEC_FUNCTION: 
        case SOS_FEEDBACK_SET_PARAMETER: 
        case SOS_FEEDBACK_EFFECT_CHANGE: 
            SOS_handle_feedback(SOS, feedback_msg, header.msg_size);
            break;

        default: break;
        }

        /* Set the timer to 2 seconds in the future. */
        gettimeofday(&tp, NULL);
        ts.tv_sec  = (tp.tv_sec + 2);
        ts.tv_nsec = (1000 * tp.tv_usec);
        /* Go to sleep until the wakeup time (ts) is reached. */
        wake_type = pthread_cond_timedwait(SOS->task.feedback_cond, SOS->task.feedback_lock, &ts);
        if (wake_type == ETIMEDOUT) {
            /* ...any special actions that need to happen if timed-out vs. explicitly triggered */
        }
    }

    free(check_in_msg);
    free(feedback_msg);

    pthread_mutex_unlock(SOS->task.feedback_lock);
    return NULL;
}


void SOS_handle_feedback(SOS_runtime *sos_context, unsigned char *msg, int msg_length) {
    SOS_SET_CONTEXT(sos_context, "SOS_handle_feedback");
    int  activity_code;
    char function_sig[SOS_DEFAULT_STRING_LEN] = {0};

    SOS_msg_header header;

    char *ptr;
    int ptr_offset;

    /* TODO: { FEEDBACK } Ponder the thread-safety of all this jazz... */

    ptr = msg;
    ptr_offset = 0;

    ptr_offset += SOS_buffer_unpack(SOS, ptr, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);
    ptr = (msg + ptr_offset);

    ptr_offset += SOS_buffer_unpack(SOS, ptr, "i",
        &activity_code);
    ptr = (msg + ptr_offset);
    
    
    switch (activity_code) {
    case SOS_FEEDBACK_CONTINUE: break;

    case SOS_FEEDBACK_EXEC_FUNCTION:
        /* TODO: { HANDLE_FEEDACK } Add in more robust / variable handlers. (hardcoded right now) */
        /* Read in the function signature from the buffer. */
        /* Check if that function is supported by this libsos client. */
        /* Launch the SOS_feedback_exec(...) routine for that function. */

        ptr_offset += SOS_buffer_unpack(SOS, ptr, "s", function_sig);
        ptr = (msg + ptr_offset);
        dlog(5, "FEEDBACK activity_code {%d} called --> EXEC_FUNCTION(%s) triggered.\n", activity_code, function_sig);

    case SOS_FEEDBACK_SET_PARAMETER: break;
    case SOS_FEEDBACK_EFFECT_CHANGE: break;
    default: break;
    }

    return;
}





void SOS_uid_init(SOS_runtime *sos_context,  SOS_uid **id_var, SOS_guid set_from, SOS_guid set_to ) {
    SOS_SET_CONTEXT(sos_context, "SOS_uid_init");
    SOS_uid *id;

    dlog(5, "  ... allocating uid sets\n");
    id = *id_var = (SOS_uid *) malloc(sizeof(SOS_uid));
    id->next = (set_from > 0) ? set_from : 1;
    id->last = (set_to   < SOS_DEFAULT_UID_MAX) ? set_to : SOS_DEFAULT_UID_MAX;
    dlog(5, "     ... default set for uid range (%ld -> %ld).\n", id->next, id->last);
    dlog(5, "     ... initializing uid mutex.\n");
    id->lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(id->lock, NULL );

    id->sos_context = sos_context;

    return;
}

void SOS_uid_destroy( SOS_uid *id ) {
    SOS_SET_CONTEXT(id->sos_context, "SOS_uid_destroy");

    dlog(5, "  ... destroying uid mutex     &(%ld)\n", (long) &id->lock );
    pthread_mutex_destroy( id->lock );
    dlog(5, "  ... freeing uid mutex space  &(%ld)\n", (long) &id->lock );
    free(id->lock);
    dlog(5, "  ... freeing uid memory       &(%ld)\n", (long) id);
    memset(id, '\0', sizeof(SOS_uid));
    free(id);

    return;
}


SOS_guid SOS_uid_next( SOS_uid *id ) {
    SOS_SET_CONTEXT(id->sos_context, "SOS_uid_next");
    long next_serial;

    dlog(7, "LOCK id->lock\n");
    pthread_mutex_lock( id->lock );

    next_serial = id->next++;

    if (id->next > id->last) {
    /* The assumption here is that we're dealing with a GUID, as the other
     * 'local' uid ranges are so large as to effectively guarantee this case
     * will not occur for them.
     */

        if (SOS->role == SOS_ROLE_DAEMON) {
            /* NOTE: There is no recourse if a DAEMON runs out of GUIDs.
             *       That should *never* happen.
             */
            dlog(0, "ERROR!  This sosd instance has run out of GUIDs!  Terminating.\n");
            exit(EXIT_FAILURE);
        } else {
            /* Acquire a fresh block of GUIDs from the DAEMON... */
            SOS_msg_header msg;
            unsigned char buffer[SOS_DEFAULT_REPLY_LEN] = {0};
            
            dlog(1, "The last guid has been used from SOS->uid.my_guid_pool!  Requesting a new block...\n");
            msg.msg_size = sizeof(SOS_msg_header);
            msg.msg_from = SOS->my_guid;
            msg.msg_type = SOS_MSG_TYPE_GUID_BLOCK;
            msg.pub_guid = 0;
            SOS_send_to_daemon(SOS, (unsigned char *) &msg, sizeof(SOS_msg_header), buffer, SOS_DEFAULT_REPLY_LEN);
            if (SOS->config.offline_test_mode == true) {
                /* NOTE: In OFFLINE_TEST_MODE there is zero chance of exhausting GUID's... seriously. */
            } else {
                memcpy(&id->next, buffer, sizeof(long));
                memcpy(&id->last, (buffer + sizeof(long)), sizeof(long));
            }
            dlog(1, "  ... recieved a new guid block from %" SOS_GUID_FMT " to %" SOS_GUID_FMT ".\n", id->next, id->last);
        }
    }

    dlog(7, "UNLOCK id->lock\n");
    pthread_mutex_unlock( id->lock );

    return next_serial;
}


SOS_pub* SOS_pub_create(SOS_runtime *sos_context, char *title, SOS_nature nature) {
    return SOS_pub_create_sized(sos_context, title, nature, SOS_DEFAULT_ELEM_MAX);
}

SOS_pub* SOS_pub_create_sized(SOS_runtime *sos_context, char *title, SOS_nature nature, int new_size) {
    SOS_SET_CONTEXT(sos_context, "SOS_pub_create_sized");
    SOS_pub   *new_pub;
    int        i;

    dlog(6, "Allocating and initializing a new pub handle....\n");

    new_pub = malloc(sizeof(SOS_pub));
    memset(new_pub, '\0', sizeof(SOS_pub));

    dlog(6, "  ... binding pub to it's execution context.\n");
    new_pub->sos_context = sos_context;

    if (SOS->role != SOS_ROLE_CLIENT) {
        new_pub->guid = -1;
    } else {
        new_pub->guid = SOS_uid_next( SOS->uid.my_guid_pool );
    }
    snprintf(new_pub->guid_str, SOS_DEFAULT_STRING_LEN, "%" SOS_GUID_FMT, new_pub->guid);

    dlog(6, "  ... setting default values, allocating space for strings.\n");

    new_pub->process_id   = 0;
    new_pub->thread_id    = 0;
    new_pub->comm_rank    = SOS->config.comm_rank;
    new_pub->pragma_len   = 0;
    strcpy(new_pub->title, title);
    new_pub->announced           = 0;
    new_pub->elem_count          = 0;
    new_pub->elem_max            = new_size;
    new_pub->meta.channel     = 0;
    new_pub->meta.layer       = SOS->config.layer;
    new_pub->meta.nature      = nature;
    new_pub->meta.pri_hint    = SOS_PRI_DEFAULT;
    new_pub->meta.scope_hint  = SOS_SCOPE_DEFAULT;
    new_pub->meta.retain_hint = SOS_RETAIN_DEFAULT;

    dlog(6, "  ... zero-ing out the strings.\n");

    /* Set some defaults for the SOS_ROLE_CLIENT's */
    if (SOS->role == SOS_ROLE_CLIENT) {
        dlog(6, "  ... setting defaults specific to SOS_ROLE_CLIENT.\n");
        strncpy(new_pub->node_id, SOS->config.node_id, SOS_DEFAULT_STRING_LEN);
        new_pub->process_id = SOS->config.process_id;
        strncpy(new_pub->prog_name, SOS->config.argv[0], SOS_DEFAULT_STRING_LEN);
    }

    dlog(6, "  ... allocating space for data elements.\n");
    new_pub->data                = malloc(sizeof(SOS_data *) * new_size);

    dlog(6, "  ... setting defaults for each data element.\n");
    for (i = 0; i < new_size; i++) {
        new_pub->data[i] = malloc(sizeof(SOS_data));
            memset(new_pub->data[i], '\0', sizeof(SOS_data));
            new_pub->data[i]->guid      = 0;
            new_pub->data[i]->name[0]   = '\0';
            new_pub->data[i]->type      = SOS_VAL_TYPE_INT;
            new_pub->data[i]->val_len   = 0;
            new_pub->data[i]->val.l_val = 0;
            new_pub->data[i]->state     = SOS_VAL_STATE_EMPTY;
            new_pub->data[i]->time.pack = 0.0;
            new_pub->data[i]->time.send = 0.0;
            new_pub->data[i]->time.recv = 0.0;

            new_pub->data[i]->meta.freq       = SOS_VAL_FREQ_DEFAULT;
            new_pub->data[i]->meta.classifier = SOS_VAL_CLASS_DATA;
            new_pub->data[i]->meta.semantic   = SOS_VAL_SEMANTIC_DEFAULT;
            new_pub->data[i]->meta.pattern    = SOS_VAL_PATTERN_DEFAULT;
            new_pub->data[i]->meta.compare    = SOS_VAL_COMPARE_SELF;
            new_pub->data[i]->meta.mood       = SOS_MOOD_GOOD;

    }

    if (sos_context->role == SOS_ROLE_CLIENT) {
        dlog(6, "  ... configuring pub to use sos_context->task.val_intake for snap queue.\n");
        new_pub->snap_queue = sos_context->task.val_intake;
    }

    dlog(6, "  ... initializing the name table for values.\n");
    new_pub->name_table = qhashtbl(SOS_DEFAULT_TABLE_SIZE);

    dlog(6, "  ... done.\n");

    return new_pub;
}




void SOS_expand_data( SOS_pub *pub ) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_expand_data");

    int n;
    SOS_data **expanded_data;

    dlog(6, "Growing pub(\"%s\")->elem_max from %d to %d...\n",
            pub->title,
            pub->elem_max,
            (pub->elem_max + SOS_DEFAULT_ELEM_MAX));

    expanded_data = malloc((pub->elem_max + SOS_DEFAULT_ELEM_MAX) * sizeof(SOS_data *));
    memset(expanded_data, '\0', ((pub->elem_max + SOS_DEFAULT_ELEM_MAX) * sizeof(SOS_data *)));

    memcpy(expanded_data, pub->data, (pub->elem_max * sizeof(SOS_data *)));
    for (n = pub->elem_max; n < (pub->elem_max + SOS_DEFAULT_ELEM_MAX); n++) {
        expanded_data[n] = malloc(sizeof(SOS_data));
        memset(expanded_data[n], '\0', sizeof(SOS_data)); }
    free(pub->data);
    pub->data = expanded_data;
    pub->elem_max = (pub->elem_max + SOS_DEFAULT_ELEM_MAX);

    dlog(6, "  ... done.\n");

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


int SOS_event(SOS_pub *pub, const char *name, SOS_val_semantic semantic) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_event");
    int pos = 0;
    int val = 1;

    pos = SOS_pub_search(pub, name);
    if (pos >= 0) {
        val = pub->data[pos]->val.i_val;
        val++;
    } else {
        val = 1;
    }

    pos = SOS_pack(pub, name, SOS_VAL_TYPE_INT, (SOS_val) val);

    pub->data[pos]->meta.classifier = SOS_VAL_CLASS_EVENT;
    pub->data[pos]->meta.semantic   = semantic;

    return pos;
}


int SOS_pack( SOS_pub *pub, const char *name, SOS_val_type pack_type, SOS_val pack_val ) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_pack");

    int       pos;
    SOS_data *data;

    /* The hash table will return NULL if a value is not present.
     * The pub->data[elem] index is zero-indexed, so indices are stored +1, to
     *   differentiate between empty and the first position.  The value
     *   returned by SOS_pub_search() is the actual array index to be used.
     */
    pos = SOS_pub_search(pub, name);

    if (pos < 0) {
        /* Value does NOT EXIST in the pub. */
        /* Check if we need to expand the pub */
        if (pub->elem_count >= pub->elem_max) {
            SOS_expand_data(pub);
        }

        /* Insert the value... */
        pos = pub->elem_count;
        pub->elem_count++;

        /* NOTE: (pos + 1) is correct, we're storing it's "N'th element" position
         *       rather than it's array index. See SOS_pub_search(...) for explanation. */
        pub->name_table->put(pub->name_table, name, (void *) ((long)(pos + 1)));

        data = pub->data[pos];

        data->guid = SOS_uid_next(SOS->uid.my_guid_pool);
        data->type = pack_type;
        if (data->type == SOS_VAL_TYPE_STRING) {
            data->val.c_val = strndup(pack_val.c_val, SOS_DEFAULT_STRING_LEN);
            dlog(8, "[STRING] Packing a string: %s\n", data->val.c_val);
        } else {
            data->val  = pack_val;
        }
        data->state = SOS_VAL_STATE_DIRTY;

        strncpy(data->name, name, SOS_DEFAULT_STRING_LEN);
        SOS_TIME( data->time.pack );

    } else {
        /* Name ALREADY EXISTS in the pub... (update new/enqueue old) */

        data = pub->data[pos];
        /* If the value is dirty, smash it down into the queue. */
        if (data->state == SOS_VAL_STATE_DIRTY) {
            SOS_val_snap_enqueue(pub->snap_queue, pub, pos);
        }

        /* Update the value in the pub->data[elem] position. */
        if (data->type == SOS_VAL_TYPE_STRING) {
            if (data->val.c_val != NULL) { free(data->val.c_val); }
            data->val.c_val = strndup( pack_val.c_val, SOS_DEFAULT_STRING_LEN);
            dlog(8, "[STRING] Re-packing a string value: %s\n", data->val.c_val);
        } else {
            data->val = pack_val;
        }
        data->state = SOS_VAL_STATE_DIRTY;
        SOS_TIME( data->time.pack );
    }

    return pos;
}

int SOS_pub_search(SOS_pub *pub, const char *name) {
    long i;
    
    i = (long) pub->name_table->get(pub->name_table, name);

    /* If the name does not exist, i==0... since the data elements are
     * zero-indexed, we subtract 1 from i so that 'does not exist' is
     * returned as -1. This is accounted for when values are initially
     * packed, as the name is added to the name_table with the index being
     * (i + 1)'ed.
     */

    i--;

    return i;
}


void SOS_pub_destroy(SOS_pub *pub) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_pub_destroy");
    int elem;

    if (SOS->config.offline_test_mode != true) {
        /* TODO: { PUB DESTROY } Right now this only works in offline test mode
         * within the client-side library code. The Daemon likely has additional
         * memory structures in play.
         *  ...is this still the case?  */
        return;
    }

    if (pub == NULL) { return; }

    dlog(6, "Freeing pub components:\n");
    dlog(6, "  ... snapshot queue\n");
    SOS_val_snap_queue_drain(pub->snap_queue, pub);
    dlog(6, "  ... element data: ");
    for (elem = 0; elem < pub->elem_max; elem++) {
        if (pub->data[elem]->type == SOS_VAL_TYPE_STRING) {
            if (pub->data[elem]->val.c_val != NULL) {
                 free(pub->data[elem]->val.c_val);
            }

        }
        if (pub->data[elem] != NULL) { free(pub->data[elem]); }
    }
    dlog(6, "done. (%d element capacity)\n", pub->elem_max);
    dlog(6, "  ... element pointer array\n");
    if (pub->data != NULL) { free(pub->data); }
    dlog(6, "  ... name table\n");
    pub->name_table->free(pub->name_table);
    dlog(6, "  ... pub handle itself\n");
    if (pub != NULL) { free(pub); pub = NULL; }
    dlog(6, "  done.\n");

    return;
}



void SOS_display_pub(SOS_pub *pub, FILE *output_to) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_display_pub");
    
    /* TODO:{ DISPLAY_PUB, CHAD }
     *
     * This needs to get cleaned up and restored to a the useful CSV/TSV that it was.
     */
    
    return;
}


void SOS_val_snap_queue_init(SOS_runtime *sos_context, SOS_val_snap_queue **queue_var) {
    SOS_SET_CONTEXT(sos_context, "SOS_val_snap_queue_init");
    SOS_val_snap_queue *queue;

    *queue_var = (SOS_val_snap_queue *) malloc(sizeof(SOS_val_snap_queue));
    queue = *queue_var;

    dlog(5, "Initializing a val_snap_queue:\n");

    queue->sos_context = sos_context;

    dlog(5, "  ... initializing queue->lock\n");
    queue->lock = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(queue->lock, NULL);

    dlog(5, "  ... initializing queue->from (hash table)\n");
    queue->from = qhashtbl(SOS_DEFAULT_TABLE_SIZE);

    dlog(5, "  ... done.\n");

    return;
}



void SOS_val_snap_enqueue(SOS_val_snap_queue *queue, SOS_pub *pub, int elem) {
    SOS_SET_CONTEXT(queue->sos_context, "SOS_val_snap_enqueue");
    SOS_val_snap *new_snap;
    SOS_val val_copy;

    dlog(6, "Creating and enqueue'ing a snapshot of \"%s\":\n", pub->data[elem]->name);

    if (queue == NULL) { return; }
    dlog(7, "  ... LOCK queue->lock\n");
    pthread_mutex_lock(queue->lock);

    dlog(6, "  ... initializing snapshot\n");
    new_snap = (SOS_val_snap *) malloc(sizeof(SOS_val_snap));
    memset(new_snap, '\0', sizeof(SOS_val_snap));

    memset(pub->guid_str, '\0', SOS_DEFAULT_STRING_LEN);
    sprintf(pub->guid_str, "%" SOS_GUID_FMT, pub->guid);

    val_copy = pub->data[elem]->val;
    if (pub->data[elem]->type == SOS_VAL_TYPE_STRING) {
        dlog(6, "  ... allocating memory for a string (pub->data[%d]->val_len == %d)\n", elem, pub->data[elem]->val_len);
        val_copy.c_val = (char *) malloc (1 + pub->data[elem]->val_len);
        memset(val_copy.c_val, '\0', (1 + pub->data[elem]->val_len));
        memcpy(val_copy.c_val, pub->data[elem]->val.c_val, pub->data[elem]->val_len);
    }

    dlog(6, "  ... assigning values to snapshot\n");

    new_snap->elem      = elem;
    new_snap->guid      = pub->data[elem]->guid;
    new_snap->mood      = pub->data[elem]->meta.mood;
    new_snap->semantic  = pub->data[elem]->meta.semantic;
    new_snap->val       = val_copy;
    new_snap->time      = pub->data[elem]->time;
    new_snap->frame     = pub->frame;
    new_snap->next      = (void *) queue->from->get(queue->from, pub->guid_str);
    queue->from->remove(queue->from, pub->guid_str);

    dlog(6, "     &(%ld) new_snap->elem      = %d\n",  (long) new_snap, new_snap->elem);
    dlog(6, "     &(%ld) new_snap->guid      = %" SOS_GUID_FMT "\n", (long) new_snap, new_snap->guid);
    dlog(6, "     &(%ld) new_snap->mood      = %d\n",  (long) new_snap, new_snap->mood);
    dlog(6, "     &(%ld) new_snap->semantic  = %d\n",  (long) new_snap, new_snap->semantic);
    
    switch(pub->data[elem]->type) {
    case SOS_VAL_TYPE_INT:     dlog(6, "     &(%ld) new_snap->val       = %d (int)\n",     (long) new_snap, new_snap->val.i_val); break;
    case SOS_VAL_TYPE_LONG:    dlog(6, "     &(%ld) new_snap->val       = %ld (long)\n",   (long) new_snap, new_snap->val.l_val); break;
    case SOS_VAL_TYPE_DOUBLE:  dlog(6, "     &(%ld) new_snap->val       = %lf (double)\n", (long) new_snap, new_snap->val.d_val); break;
    case SOS_VAL_TYPE_STRING:  dlog(6, "     &(%ld) new_snap->val       = %s (string)\n",  (long) new_snap, new_snap->val.c_val); break;
    default:
        dlog(6, "Invalid type (%d) at index %d of pub->guid == %ld.\n", pub->data[elem]->type, elem, pub->guid);
        break;
    }
    dlog(6, "     &(%ld) new_snap->frame     = %ld\n", (long) new_snap, new_snap->frame);
    dlog(6, "     &(%ld) new_snap->next      = %ld\n", (long) new_snap, (long) new_snap->next);
    dlog(6, "  ... making this the new head of the val_snap queue\n");

    /* We're hashing into per-pub val_snap stacks... (say that 5 times fast) */
    queue->from->put(queue->from, pub->guid_str, (void *) new_snap);

    dlog(7, "  ... UNLOCK queue->lock\n");
    pthread_mutex_unlock(queue->lock);

    dlog(6, "  ... done.\n");

    return;
}


void SOS_val_snap_queue_to_buffer(SOS_val_snap_queue *queue, SOS_pub *pub, unsigned char **buf_ptr, int *buf_len, bool drain) {
    SOS_SET_CONTEXT(queue->sos_context, "SOS_val_snap_queue_to_buffer");
    SOS_msg_header header;
    unsigned char *buffer;
    unsigned char *ptr;
    int   buffer_len;
    SOS_val_snap *snap;

    buffer = *buf_ptr;
    buffer_len = 0;
    ptr = (buffer + buffer_len);
    memset(ptr, '\0', SOS_DEFAULT_BUFFER_LEN);

    snap = (SOS_val_snap *) queue->from->get(queue->from, pub->guid_str);

    if (snap == NULL) {
        dlog(4, "  ... nothing to do for pub(%s)\n", pub->guid_str);
        *buf_len = buffer_len;
        return;
    }

    dlog(6, "  ... building buffer from the val_snap queue:\n");
    dlog(7, "     ... LOCK queue->lock\n");
    pthread_mutex_lock( queue->lock );
    dlog(6, "     ... processing header\n");

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_VAL_SNAPS;
    header.msg_from = SOS->my_guid;
    header.pub_guid = pub->guid;

    buffer_len += SOS_buffer_pack(SOS, ptr, "iigg",
                                  header.msg_size,
                                  header.msg_type,
                                  header.msg_from,
                                  header.pub_guid);
    ptr = (buffer + buffer_len);

    dlog(6, "     ... processing snaps extracted from the queue\n");
    snap = (SOS_val_snap *) queue->from->get(queue->from, pub->guid_str);

    while (snap != NULL) {

        dlog(6, "     ... guid=%" SOS_GUID_FMT "\n", snap->guid);
        snap->visits++;

        buffer_len += SOS_buffer_pack(SOS, ptr, "igdddl",
                                      snap->elem,
                                      snap->guid,
                                      snap->time.pack,
                                      snap->time.send,
                                      snap->time.recv,
                                      snap->frame);
        ptr = (buffer + buffer_len);

        switch (pub->data[snap->elem]->type) {
        case SOS_VAL_TYPE_INT:    buffer_len += SOS_buffer_pack(SOS, ptr, "i", snap->val.i_val); break;
        case SOS_VAL_TYPE_LONG:   buffer_len += SOS_buffer_pack(SOS, ptr, "l", snap->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE: buffer_len += SOS_buffer_pack(SOS, ptr, "d", snap->val.d_val); break;
        case SOS_VAL_TYPE_STRING: buffer_len += SOS_buffer_pack(SOS, ptr, "s", snap->val.c_val); break;
        default:
            dlog(6, "Invalid type (%d) at index %d of pub->guid == %" SOS_GUID_FMT ".\n", pub->data[snap->elem]->type, snap->elem, pub->guid);
            break;
        }
        ptr = (buffer + buffer_len);
        snap = snap->next;
    }
    if (drain) {
        dlog(6, "     ... draining queue for pub(%s)\n", pub->guid_str);
        SOS_val_snap_queue_drain(queue, pub);
    }

    dlog(7, "     ... UNLOCK queue->lock\n");
    pthread_mutex_unlock( queue->lock );

    *buf_len        = buffer_len;
    header.msg_size = buffer_len;
    SOS_buffer_pack(SOS, buffer, "i", header.msg_size);
    dlog(6, "     ... done   (buf_len == %d)\n", *buf_len);
   
    return;
}



void SOS_val_snap_queue_from_buffer(SOS_val_snap_queue *queue, qhashtbl_t *pub_table, unsigned char *buffer, int buffer_size) {
    SOS_SET_CONTEXT(queue->sos_context, "SOS_val_snap_queue_from_buffer");
    SOS_msg_header header;
    unsigned char *ptr;
    int   offset;
    SOS_val_snap *snap;
    SOS_pub *pub;
    int string_len;

    offset = 0;
    ptr = (buffer + offset);

    dlog(6, "  ... building val_snap queue from a buffer:\n");
    dlog(7, "     ... LOCK queue->lock\n");
    pthread_mutex_lock( queue->lock );
    dlog(6, "     ... processing header\n");

    offset += SOS_buffer_unpack(SOS, ptr, "iill",
                                  &header.msg_size,
                                  &header.msg_type,
                                  &header.msg_from,
                                  &header.pub_guid);
    ptr = (buffer + offset);

    dlog(6, "     ... header.msg_size == %d\n", header.msg_size);
    dlog(6, "     ... header.msg_type == %d\n", header.msg_type);
    dlog(6, "     ... header.msg_from == %" SOS_GUID_FMT "\n", header.msg_from);
    dlog(6, "     ... header.pub_guid == %" SOS_GUID_FMT "\n", header.pub_guid);
    
    char guid_str[SOS_DEFAULT_STRING_LEN];
    snprintf(guid_str, SOS_DEFAULT_STRING_LEN, "%" SOS_GUID_FMT, header.pub_guid);

    pub = (SOS_pub *) pub_table->get(pub_table, guid_str);
    
    if (pub == NULL) {
        dlog(1, "WARNING! Attempting to build snap_queue for a pub we don't know about.\n");
        dlog(1, "  ... skipping this request.\n");
        dlog(7, "  ... UNLOCK queue->lock\n");
        pthread_mutex_unlock( queue->lock );
        return;
    }
    

    dlog(6, "     ... pushing snaps down onto the queue.\n");

    while (offset < buffer_size) {
        snap = (SOS_val_snap *) malloc(sizeof(SOS_val_snap));
        memset(snap, '\0', sizeof(SOS_val_snap));

        offset += SOS_buffer_unpack(SOS, ptr, "ildddl",
                                    &snap->elem,
                                    &snap->guid,
                                    &snap->time.pack,
                                    &snap->time.send,
                                    &snap->time.recv,
                                    &snap->frame);
        ptr = (buffer + offset);

        switch (pub->data[snap->elem]->type) {
        case SOS_VAL_TYPE_INT:    offset += SOS_buffer_unpack(SOS, ptr, "i", &snap->val.i_val); break;
        case SOS_VAL_TYPE_LONG:   offset += SOS_buffer_unpack(SOS, ptr, "l", &snap->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE: offset += SOS_buffer_unpack(SOS, ptr, "d", &snap->val.d_val); break;
        case SOS_VAL_TYPE_STRING:
            /* Extract only the length so we know how much space to allocate. */
            SOS_buffer_unpack(SOS, ptr, "i", &string_len);
            snap->val.c_val = (char *) malloc(1 + string_len);
            memset(snap->val.c_val, '\0', (1 + string_len));
            offset += SOS_buffer_unpack(SOS, ptr, "s", snap->val.c_val);
            dlog(0, "[STRING] Extracted val_snap string: %s\n", snap->val.c_val);
            break;
        default:
            dlog(6, "Invalid type (%d) at index %d of pub->guid == %" SOS_GUID_FMT ".\n", pub->data[snap->elem]->type, snap->elem, pub->guid);
            break;
        }
        ptr = (buffer + offset);
        SOS_val_snap_push_down(queue, pub->guid_str, snap, 0);

        /* TODO: 
         *   Do we want to make sure the pub[elem]->... stuff is updated
         *   with the value from this snapshot as well?
         */

    }

    dlog(7, "     ... UNLOCK queue->lock\n");
    pthread_mutex_unlock( queue->lock );
    dlog(6, "     ... done\n");

    return;
}



void SOS_val_snap_push_down(SOS_val_snap_queue *queue, char *pub_guid_str, SOS_val_snap *snap, int use_lock) {
    SOS_SET_CONTEXT(queue->sos_context, "SOS_val_snap_push_down");

    if (use_lock) { dlog(7, "LOCK queue->lock\n"); pthread_mutex_lock(queue->lock); }
    snap->next = (SOS_val_snap *) queue->from->get(queue->from, pub_guid_str);
    queue->from->remove(queue->from, pub_guid_str);
    queue->from->put(queue->from, pub_guid_str, (void *) snap);
    if (use_lock) { dlog(7, "UNLOCK queue->lock\n"); pthread_mutex_unlock(queue->lock); }

    return;
}



void SOS_val_snap_queue_drain(SOS_val_snap_queue *queue, SOS_pub *pub) {
    SOS_SET_CONTEXT(queue->sos_context, "SOS_val_snap_queue_drain");
    SOS_val_snap *snap;
    SOS_val_snap *next_snap;
    long this_frame;

    snap = (SOS_val_snap *) queue->from->get(queue->from, pub->guid_str);
    queue->from->remove(queue->from, pub->guid_str);

    while (snap != NULL) {
        /* We do this test to make sure we don't drain parts of the queue that
         * might not have been handled yet. Only handle one message's worth
         * at a time. */
        //if (snap->frame != this_frame) return;

        if (SOS->role == SOS_ROLE_DAEMON) {
            if (snap->visits != 2) {
                dlog(0, "snap->guid(%" SOS_GUID_FMT ") is being drained with %d visits!\n", snap->guid, snap->visits);
            }
        }

        if (SOS->role == SOS_ROLE_DB) {
            dlog(0, "snap->guid(%" SOS_GUID_FMT ") is being drained in the SOS_ROLE_DB ... this is wrong.\n", snap->guid);
        }

        next_snap = snap->next;
        if (pub->data[snap->elem]->type == SOS_VAL_TYPE_STRING) {
            free(snap->val.c_val);
        }
        free(snap);
        snap = next_snap;
    }

    return;
}



void SOS_val_snap_queue_destroy(SOS_val_snap_queue *queue) {
    SOS_SET_CONTEXT(queue->sos_context, "SOS_val_snap_queue_destroy");

    pthread_mutex_destroy(queue->lock);
    free(queue->lock);

    queue->from->free(queue->from);
    free(queue);

    return;
}






/* WARNING: For simplicity's sake and for performance, this routine does not
 *          perform any buffer size safety checks.
 * TODO: { BUFFERS } Force explicit buffer length safety checks.
 */
void SOS_announce_to_buffer( SOS_pub *pub, unsigned char **buf_ptr, int *buf_len ) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_announce_to_buffer");
    SOS_msg_header header;
    unsigned char *buffer;
    unsigned char *ptr;
    int   buffer_len;
    int   elem;

    buffer = *buf_ptr;
    buffer_len = 0;
    ptr = buffer;

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_ANNOUNCE;
    header.msg_from = SOS->my_guid;
    header.pub_guid = pub->guid;
    /* Pack the header, (we'll re-pack the header.msg_size at the end) */
    buffer_len += SOS_buffer_pack(SOS, ptr, "iigg",
        header.msg_size,
        header.msg_type,
        header.msg_from,
        header.pub_guid);
    ptr = (buffer + buffer_len);

    /* Pack the pub definition: */
    buffer_len += SOS_buffer_pack(SOS, ptr, "siiississiiiiiii",
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
        buffer_len += SOS_buffer_pack(SOS, ptr, "gsiiiiiii",
            pub->data[elem]->guid,
            pub->data[elem]->name,
            pub->data[elem]->type,
            pub->data[elem]->meta.freq,
            pub->data[elem]->meta.semantic,
            pub->data[elem]->meta.classifier,
            pub->data[elem]->meta.pattern,
            pub->data[elem]->meta.compare,
            pub->data[elem]->meta.mood );
        ptr = (buffer + buffer_len);
    }

    /* Re-pack the message size now that we know it. */
    header.msg_size = buffer_len;
    SOS_buffer_pack(SOS, buffer, "i", header.msg_size);

    *buf_len = buffer_len;

    return;
}


void SOS_publish_to_buffer( SOS_pub *pub, unsigned char **buf_ptr, int *buf_len ) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_publish_to_buffer");
    SOS_msg_header   header;
    unsigned char   *buffer;
    unsigned char   *ptr;
    long             this_frame;
    int              buffer_len;
    int              elem;
    double           send_time;

    buffer = *buf_ptr;
    buffer_len = 0;
    ptr = buffer;

    SOS_TIME( send_time );

    if (SOS->role == SOS_ROLE_CLIENT) {
        /* Only CLIENT updates the frame when sending, in case this is re-used
         * internally / on the backplane by the DB or DAEMON. */
        this_frame = pub->frame++;
    }

    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_PUBLISH;
    header.msg_from = SOS->my_guid;
    header.pub_guid = pub->guid;
    /* Pack the header, (we'll re-pack the header.msg_size at the end) */
    buffer_len += SOS_buffer_pack(SOS, ptr, "iigg",
        header.msg_size,
        header.msg_type,
        header.msg_from,
        header.pub_guid);
    ptr = (buffer + buffer_len);

    /* Pack in the frame of these elements: */
    buffer_len += SOS_buffer_pack(SOS, ptr, "l", this_frame);
    ptr = (buffer + buffer_len);

    /* Pack in the data elements. */
    for (elem = 0; elem < pub->elem_count; elem++) {
        if ( pub->data[elem]->state != SOS_VAL_STATE_DIRTY) { continue; }
        
        pub->data[elem]->state = SOS_VAL_STATE_CLEAN;
        pub->data[elem]->time.send = send_time;

        dlog(7, "pub->data[%d]->time.pack == %lf   pub->data[%d]->time.send == %lf\n",
             elem,
             pub->data[elem]->time.pack,
             elem,
             pub->data[elem]->time.send);

        buffer_len += SOS_buffer_pack(SOS, ptr, "iddi",
            elem,
            pub->data[elem]->time.pack,
            pub->data[elem]->time.send,
            pub->data[elem]->val_len);
        ptr = (buffer + buffer_len);

        buffer_len += SOS_buffer_pack(SOS, ptr, "ii",
                                      pub->data[elem]->meta.semantic,
                                      pub->data[elem]->meta.mood);
        ptr = (buffer + buffer_len);

        switch (pub->data[elem]->type) {
        case SOS_VAL_TYPE_INT:     buffer_len += SOS_buffer_pack(SOS, ptr, "i", pub->data[elem]->val.i_val); break;
        case SOS_VAL_TYPE_LONG:    buffer_len += SOS_buffer_pack(SOS, ptr, "l", pub->data[elem]->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE:  buffer_len += SOS_buffer_pack(SOS, ptr, "d", pub->data[elem]->val.d_val); break;
        case SOS_VAL_TYPE_STRING:  buffer_len += SOS_buffer_pack(SOS, ptr, "s", pub->data[elem]->val.c_val); break;
        default:
            dlog(6, "Invalid type (%d) at index %d of pub->guid == %" SOS_GUID_FMT ".\n", pub->data[elem]->type, elem, pub->guid);
            break;
        }
        ptr = (buffer + buffer_len);
    }

    /* Re-pack the message size now that we know what it is. */
    header.msg_size = buffer_len;
    SOS_buffer_pack(SOS, buffer, "i", header.msg_size);

    *buf_len = buffer_len;

    return;
}


void SOS_announce_from_buffer( SOS_pub *pub, unsigned char *buf_ptr ) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_announce_from_buffer");
    SOS_msg_header header;
    unsigned char *buffer;
    unsigned char *ptr;
    int   buffer_pos;
    int   elem;

    dlog(6, "Applying an ANNOUNCE from a buffer...\n");

    ptr        = buf_ptr;
    buffer     = buf_ptr;
    buffer_pos = 0;

    dlog(6, "  ... unpacking the header.\n");
    /* Unpack the header */
    buffer_pos += SOS_buffer_unpack(SOS, ptr, "iill",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);
    ptr = (buffer + buffer_pos);

    dlog(6, "  ::: ptr = %ld :::\n", (long int) ptr);

    pub->guid = header.pub_guid;

    snprintf(pub->guid_str, SOS_DEFAULT_STRING_LEN, "%" SOS_GUID_FMT, pub->guid);

    dlog(6, "  ... unpacking the pub definition.\n");
    /* Unpack the pub definition: */

    buffer_pos += SOS_buffer_unpack(SOS, ptr, "siiississiiiiiii",
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

    dlog(6, "pub->node_id = \"%s\"\n", pub->node_id);
    dlog(6, "pub->process_id = %d\n", pub->process_id);
    dlog(6, "pub->thread_id = %d\n", pub->thread_id);
    dlog(6, "pub->comm_rank = %d\n", pub->comm_rank);
    dlog(6, "pub->prog_name = \"%s\"\n", pub->prog_name);
    dlog(6, "pub->prog_ver = \"%s\"\n", pub->prog_ver);
    dlog(6, "pub->pragma_len = %d\n", pub->pragma_len);
    dlog(6, "pub->pragma_msg = \"%s\"\n", pub->pragma_msg);
    dlog(6, "pub->title = \"%s\"\n", pub->title);
    dlog(6, "pub->elem_count = %d\n", elem);
    dlog(6, "pub->meta.channel = %d\n", pub->meta.channel);
    dlog(6, "pub->meta.layer = %d\n", pub->meta.layer);
    dlog(6, "pub->meta.nature = %d\n", pub->meta.nature);
    dlog(6, "pub->meta.pri_hint = %d\n", pub->meta.pri_hint);
    dlog(6, "pub->meta.scope_hint = %d\n", pub->meta.scope_hint);
    dlog(6, "pub->meta.retain_hint = %d\n", pub->meta.retain_hint);

    if (SOS->role == SOS_ROLE_DAEMON) {
        dlog(4, "AUTOGROW --\n");
        dlog(4, "AUTOGROW --\n");
        dlog(4, "AUTOGROW -- Announced pub size: %d", elem);
        dlog(4, "AUTOGROW -- In-memory pub size: %d", pub->elem_max);
        dlog(4, "AUTOGROW --\n");
        dlog(4, "AUTOGROW --\n");
    }

    /* Ensure there is room in this pub to handle incoming data definitions. */
    while(pub->elem_max < elem) {
        dlog(6, "  ... doubling pub->elem_max from %d to handle %d elements...\n", pub->elem_max, elem);
        SOS_expand_data(pub);
    }
    pub->elem_count = elem;

    dlog(6, "  ... unpacking the data definitions.\n");
    /* Unpack the data definitions: */
    elem = 0;
    for (elem = 0; elem < pub->elem_count; elem++) {
        buffer_pos += SOS_buffer_unpack(SOS, ptr, "lsiiiiiii",
            &pub->data[elem]->guid,
            pub->data[elem]->name,
            &pub->data[elem]->type,
            &pub->data[elem]->meta.freq,
            &pub->data[elem]->meta.semantic,
            &pub->data[elem]->meta.classifier,
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
        dlog(6, "  ... pub->data[%d]->guid = %" SOS_GUID_FMT "\n", elem, pub->data[elem]->guid);
        dlog(6, "  ... pub->data[%d]->name = %s\n", elem, pub->data[elem]->name);
        dlog(6, "  ... pub->data[%d]->type = %d\n", elem, pub->data[elem]->type);
    }
    dlog(6, "  ... done.\n");

    return;
}

void SOS_publish_from_buffer( SOS_pub *pub, unsigned char *buf_ptr, SOS_val_snap_queue *opt_queue ) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_publish_from_buffer");
    SOS_msg_header header;
    unsigned char *buffer;
    unsigned char *ptr;
    long  this_frame;
    int   buffer_pos;
    int   elem;

    ptr        = buf_ptr;
    buffer     = buf_ptr;
    buffer_pos = 0;

    dlog(7, "Unpacking the values from the buffer...\n");

    /* Unpack the header */
    buffer_pos += SOS_buffer_unpack(SOS, ptr, "iill",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);
    ptr = (buffer + buffer_pos);

    dlog(7, "  ... header.msg_size = %d\n", header.msg_size);
    dlog(7, "  ... header.msg_type = %d\n", header.msg_type);
    dlog(7, "  ... header.msg_from = %ld\n", header.msg_from);
    dlog(7, "  ... header.pub_guid = %" SOS_GUID_FMT "\n", header.pub_guid);
    dlog(7, "  ... values:\n");


    /* Unpack the frame: */
    buffer_pos += SOS_buffer_unpack(SOS, ptr, "l", &this_frame);
    ptr = (buffer + buffer_pos);

    pub->frame = this_frame;

    /* Unpack in the data elements. */
    while (buffer_pos < header.msg_size) {

        buffer_pos += SOS_buffer_unpack(SOS, ptr, "i", &elem);
        ptr = (buffer + buffer_pos);

        buffer_pos += SOS_buffer_unpack(SOS, ptr, "ddi",
            &pub->data[elem]->time.pack,
            &pub->data[elem]->time.send,
            &pub->data[elem]->val_len);
        ptr = (buffer + buffer_pos);

        buffer_pos += SOS_buffer_unpack(SOS, ptr, "ii",
                                        &pub->data[elem]->meta.semantic,
                                        &pub->data[elem]->meta.mood);
        ptr = (buffer + buffer_pos);

        dlog(7, "pub->data[%d]->time.pack == %lf   pub->data[%d]->time.send == %lf\n",
             elem,
             pub->data[elem]->time.pack,
             elem,
             pub->data[elem]->time.send);
 

       switch (pub->data[elem]->type) {
        case SOS_VAL_TYPE_INT:    buffer_pos += SOS_buffer_unpack(SOS, ptr, "i", &pub->data[elem]->val.i_val); break;
        case SOS_VAL_TYPE_LONG:   buffer_pos += SOS_buffer_unpack(SOS, ptr, "l", &pub->data[elem]->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE: buffer_pos += SOS_buffer_unpack(SOS, ptr, "d", &pub->data[elem]->val.d_val); break;
        case SOS_VAL_TYPE_STRING:
            if (pub->data[elem]->val_len > SOS_DEFAULT_STRING_LEN) {
                free( pub->data[elem]->val.c_val );
                pub->data[elem]->val.c_val = (char *) malloc(1 + pub->data[elem]->val_len);
                memset(pub->data[elem]->val.c_val, '\0', (1 + pub->data[elem]->val_len));
            }
            buffer_pos += SOS_buffer_unpack(SOS, ptr, "s", pub->data[elem]->val.c_val);
            dlog(0, "[STRING] Extracted pub message string: %s\n", pub->data[elem]->val.c_val);
            break;
        default:
            dlog(6, "Invalid type (%d) at index %d of pub->guid == %" SOS_GUID_FMT ".\n", pub->data[elem]->type, elem, pub->guid);
            break;
        }
        ptr = (buffer + buffer_pos);

        /* Enqueue this value for writing out to local_sync and cloud_sync:
         * NOTE: Flushing *this* queue is triggered by the sync thread
         *       encounting this pub handle in the to-do queue. */
        if (opt_queue != NULL) {
            dlog(7, "Enqueing a val_snap for \"%s\"\n", pub->data[elem]->name);
            SOS_val_snap_enqueue(opt_queue, pub, elem);
        }

    }

    dlog(7, "  ... done.\n");

    return;
}


void SOS_announce( SOS_pub *pub ) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_announce");

    unsigned char *buffer;
    unsigned char  buffer_stack[SOS_DEFAULT_BUFFER_LEN] = {0};
    int            buffer_len;
    unsigned char *reply;
    unsigned char  reply_stack[SOS_DEFAULT_REPLY_LEN] = {0};
    int            reply_max;

    dlog(6, "Preparing an announcement message...\n");
    dlog(6, "  ... pub->guid       = %" SOS_GUID_FMT "\n", pub->guid);
    dlog(6, "  ... pub->title      = %s\n", pub->title);
    dlog(6, "  ... pub->elem_count = %d\n", pub->elem_count);
    dlog(6, "  ... pub->elem_max   = %d\n", pub->elem_max);

    buffer     = buffer_stack;
    buffer_len = 0;
    reply      = reply_stack;
    reply_max  = SOS_DEFAULT_REPLY_LEN;

    dlog(6, "  ... placing the announce message in a buffer.\n");
    SOS_announce_to_buffer(pub, &buffer, &buffer_len);
    dlog(6, "  ... sending the buffer to the daemon.\n");
    SOS_send_to_daemon(SOS, buffer, buffer_len, reply, reply_max);
    dlog(6, "  ... done.\n");
    pub->announced = 1;

    return;
}


void SOS_publish( SOS_pub *pub ) {
    SOS_SET_CONTEXT(pub->sos_context, "SOS_publish");

    unsigned char   *buffer;
    unsigned char    buffer_stack[SOS_DEFAULT_BUFFER_LEN] = {0};
    int              buffer_len;
    unsigned char   *reply;
    unsigned char    reply_stack[SOS_DEFAULT_REPLY_LEN] = {0};
    int              reply_max;

    SOS_val_snap_queue *snaps;

    memset(buffer_stack, '\0', SOS_DEFAULT_BUFFER_LEN);
    buffer     = buffer_stack;
    buffer_len = 0;
    memset(reply_stack,  '\0', SOS_DEFAULT_REPLY_LEN);
    reply     = reply_stack;
    reply_max = SOS_DEFAULT_REPLY_LEN;
    
    if (pub->announced == 0) {
        dlog(6, "AUTO-ANNOUNCING this pub...\n");
        SOS_announce( pub );
    }

    dlog(6, "Preparing a publish message...\n");
    dlog(6, "  ... pub->guid       = %" SOS_GUID_FMT "\n", pub->guid);
    dlog(6, "  ... pub->title      = %s\n", pub->title);
    dlog(6, "  ... pub->elem_count = %d\n", pub->elem_count);
    dlog(6, "  ... pub->elem_max   = %d\n", pub->elem_max);

    dlog(6, "  ... placing the publish message in a buffer.\n");
    SOS_publish_to_buffer(pub, &buffer, &buffer_len);
    dlog(6, "  ... sending the buffer to the daemon.\n");
    SOS_send_to_daemon(SOS, buffer, buffer_len, reply, reply_max);

    dlog(6, "  ... checking for val_snap queue entries.\n");
    snaps = pub->snap_queue;
    if ((snaps->from->get(snaps->from, pub->guid_str)) != NULL) {
        dlog(6, "  ... entries found, placing them into a buffer.\n");
        SOS_val_snap_queue_to_buffer(snaps, pub, &buffer, &buffer_len, true);
        dlog(6, "  ... sending the buffer to the daemon.\n");
        SOS_send_to_daemon(SOS, buffer, buffer_len, reply, reply_max);
    }
    dlog(6, "  ... done.\n");

    return;
}


