
/*
 * sos.c                 SOS library routines
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>

#include <sys/socket.h>
#include <netdb.h>

#include "sos.h"
#include "sos_debug.h"


/* Private functions (not in the header file) */
void* SOS_THREAD_post( void *arg );
void* SOS_THREAD_read( void *arg );
void* SOS_THREAD_scan( void *arg );

void SOS_post_to_daemon( char *msg, char *reply );

void SOS_free_pub(SOS_pub *pub);
void SOS_free_sub(SOS_sub *sub);
void SOS_expand_data(SOS_pub *pub);

//void* SOS_refresh_sub( void *arg );


/* **************************************** */
/* [util]                                   */
/* **************************************** */




//SOS_FLOW - ready
void SOS_init( int *argc, char ***argv, SOS_role role ) {
    char buffer[SOS_DEFAULT_BUFFER_LEN];
    int i, n, retval, server_socket_fd;

    SOS.status = SOS_STATUS_INIT;
    SOS.role = role;

    SOS_SET_WHOAMI(whoami, "SOS_init");
    dlog(2, "[%s]: Initializing SOS...\n", whoami);


    /* Network configuration: */

    if (SOS.role == SOS_ROLE_CLIENT) {
        SOS.net.buffer_len    = SOS_DEFAULT_BUFFER_LEN;
        SOS.net.timeout       = SOS_DEFAULT_MSG_TIMEOUT;
        SOS.net.server_host   = SOS_DEFAULT_SERVER_HOST;
        SOS.net.server_port   = getenv("SOS_CMD_PORT");
        if ( SOS.net.server_port < 1 ) { dlog(0, "[%s]: ERROR!  SOS_CMD_PORT environment variable is not set!\n", whoami); exit(1); }

        SOS.net.server_hint.ai_family    = AF_UNSPEC;     /* Allow IPv4 or IPv6 */
        SOS.net.server_hint.ai_protocol  = 0;             /* Any protocol */
        SOS.net.server_hint.ai_socktype  = SOCK_STREAM;    /* SOCK_STREAM vs. SOCK_DGRAM vs. SOCK_RAW */
        SOS.net.server_hint.ai_flags     = AI_NUMERICSERV | SOS.net.server_hint.ai_flags;

        retval = getaddrinfo(SOS.net.server_host, SOS.net.server_port, &SOS.net.server_hint, &SOS.net.result_list );
        if ( retval < 0 ) { dlog(0, "[%s]: ERROR!  Could not locate the SOS daemon.  (%s:%s)\n", whoami, SOS.net.server_host, SOS.net.server_port ); exit(1); }


        /* Iterate the possible connections and register with the SOS daemon: */
        for ( SOS.net.server_addr = SOS.net.result_list ; SOS.net.server_addr != NULL ; SOS.net.server_addr = SOS.net.server_addr->ai_next ) {
            server_socket_fd = socket(SOS.net.server_addr->ai_family, SOS.net.server_addr->ai_socktype, SOS.net.server_addr->ai_protocol );
            if ( server_socket_fd == -1 ) continue;
            if ( connect(server_socket_fd, SOS.net.server_addr->ai_addr, SOS.net.server_addr->ai_addrlen) != -1 ) break; /* success! */
            close( server_socket_fd );
        }

        freeaddrinfo( SOS.net.result_list );
        
        if (server_socket_fd == NULL) { dlog(0, "[%s]: ERROR!  Could not connect to the server.  (%s:%s)\n", whoami, SOS.net.server_host, SOS.net.server_port); exit(1); }
        memset(buffer, '\0', SOS_DEFAULT_BUFFER_LEN);

        /* TODO:{ CHAD, INIT } Put together a registration string requesting a GUID for this instance. */

        dlog(2, "[%s]: Connecting to SOS...   (%s:%s)\n", whoami, SOS.net.server_host, SOS.net.server_port);
        snprintf(buffer, SOS_DEFAULT_BUFFER_LEN, "....____\n");
        n = strlen(buffer);
        i = (int) SOS_MSG_TYPE_REGISTER;
        printf("\n i = %d\n", i);
        memcpy(buffer, &i, sizeof(int));
        memcpy((buffer + sizeof(int)), &i, sizeof(int));

        retval = sendto( server_socket_fd, buffer, n, NULL, NULL, NULL );
        if (retval < 0) { dlog(0, "[%s]: ERROR!  Could not write to server socket!  (%s:%s)\n", whoami, SOS.net.server_host, SOS.net.server_port); exit(1); }
        memset(buffer, '\0', SOS_DEFAULT_BUFFER_LEN);

        retval = recvfrom( server_socket_fd, buffer, (SOS_DEFAULT_BUFFER_LEN - 1), NULL, NULL, NULL );
        dlog(2, "[%s]: Server responded: %s\n", whoami, buffer);
        close( server_socket_fd );
        
    } else {
        /* 
         * [sosd] and {control} roles --MUST-- configure their
         * network / upstream target manually. ALL instances of
         * SOS_CLIENT should be connecting to the daemon on the
         * node (localhost) where they are executing.
         */
    }

    SOS.config.argc = *argc;
    SOS.config.argv = *argv;

    SOS.uid.pub = (SOS_uid *) malloc( sizeof(SOS_uid) );
    SOS.uid.sub = (SOS_uid *) malloc( sizeof(SOS_uid) );
    SOS.uid.seq = (SOS_uid *) malloc( sizeof(SOS_uid) );
    
    SOS.uid.pub->next = SOS.uid.sub->next = SOS.uid.seq->next = 0;
    SOS.uid.pub->last = SOS.uid.sub->last = SOS.uid.seq->last = SOS_DEFAULT_UID_MAX;
    pthread_mutex_init( &(SOS.uid.pub->lock), NULL );
    pthread_mutex_init( &(SOS.uid.sub->lock), NULL );
    pthread_mutex_init( &(SOS.uid.seq->lock), NULL );

    SOS.ring.send.read_pos = SOS.ring.send.write_pos = 0;
    SOS.ring.recv.read_pos = SOS.ring.recv.write_pos = 0;
    SOS.ring.send.size = SOS_DEFAULT_RING_SIZE;
    SOS.ring.recv.size = SOS_DEFAULT_RING_SIZE;
    SOS.ring.send.bytes = ( SOS.ring.send.size * sizeof(void *) );
    SOS.ring.recv.bytes = ( SOS.ring.recv.size * sizeof(void *) );
    SOS.ring.send.heap = (void **) malloc( SOS.ring.send.bytes );
    SOS.ring.recv.heap = (void **) malloc( SOS.ring.recv.bytes );
    memset( SOS.ring.send.heap, '\0', SOS.ring.send.bytes ); 
    memset( SOS.ring.recv.heap, '\0', SOS.ring.recv.bytes );
    pthread_mutex_init( &(SOS.ring.send.lock), NULL );
    pthread_mutex_init( &(SOS.ring.recv.lock), NULL );

    /* TODO:{ CHAD, INIT }  Determine whether or not to start these threads for DAEMON... */

    pthread_create( &SOS.task.post, NULL, (void *) SOS_THREAD_post, NULL );
    pthread_create( &SOS.task.read, NULL, (void *) SOS_THREAD_read, NULL );
    pthread_create( &SOS.task.scan, NULL, (void *) SOS_THREAD_scan, NULL );

    /*
     *  TODO:{ CHAD, INIT }
     *       Here we will request certain configuration things
     *       from the daemon, such as our (GUID) client_id.
     */ 

    SOS.client_id = "TEMP_ID";


    return;
}



//SOS_FLOW - ready
void SOS_send_to_daemon( char *msg, int msg_len, char *reply, int max_reply_size ) {
    SOS_SET_WHOAMI(whoami, "SOS_send_to_daemon");

    int server_socket_fd;
    int retval;


    retval = getaddrinfo(SOS.net.server_host, SOS.net.server_port, &SOS.net.server_hint, &SOS.net.result_list );
    if ( retval < 0 ) { dlog(0, "[%s]: Error attempting to locate the SOS daemon.  (%s:%s)\n", whoami, SOS.net.server_host, SOS.net.server_port ); exit(1); }
    
    /* Iterate the possible connections and register with the SOS daemon: */
    for ( SOS.net.server_addr = SOS.net.result_list ; SOS.net.server_addr != NULL ; SOS.net.server_addr = SOS.net.server_addr->ai_next ) {
        server_socket_fd = socket(SOS.net.server_addr->ai_family, SOS.net.server_addr->ai_socktype, SOS.net.server_addr->ai_protocol );
        if ( server_socket_fd == -1 ) continue;
        if ( connect( server_socket_fd, SOS.net.server_addr->ai_addr, SOS.net.server_addr->ai_addrlen) != -1 ) break; /* success! */
        close( server_socket_fd );
    }
    
    freeaddrinfo( SOS.net.result_list );
    
    if (server_socket_fd == NULL) {
        dlog(0, "[%s]: Error attempting to connect to the server.  (%s:%s)\n", whoami, SOS.net.server_host, SOS.net.server_port);
        exit(1);  /* TODO:{ COMM }  Make this a loop that tries X times to connect, doesn't crash app. */
    }

    retval = sendto(server_socket_fd, msg, msg_len, NULL, NULL, NULL );
    if (retval == -1) { dlog(0, "[%s]: Error sending message to daemon.\n", whoami); }

    retval = recvfrom(server_socket_fd, reply, max_reply_size, NULL, NULL, NULL );
    if (retval == -1) { dlog(0, "[%s]: Error receiving message from daemon.\n", whoami); }
    else { dlog(2, "[%s]: Server replied with: %s\n", whoami, reply); }

    close( server_socket_fd );

    return;
}



//SOS_FLOW - ready
void SOS_finalize() {
    SOS_SET_WHOAMI(whoami, "SOS_finalize");
    
    if (SOS.role = SOS_ROLE_CLIENT) {
        /* This will 'notify' any SOS threads to break out of their loops. */
        SOS.status = SOS_STATUS_SHUTDOWN;
        
        pthread_join( &SOS.task.post, NULL );
        pthread_join( &SOS.task.read, NULL );
        pthread_join( &SOS.task.scan, NULL );
        
        pthread_mutex_destroy( &SOS.ring.send.lock  );
        pthread_mutex_destroy( &SOS.ring.recv.lock  );
        pthread_mutex_destroy( &(SOS.uid.pub->lock) );
        pthread_mutex_destroy( &(SOS.uid.sub->lock) );
        pthread_mutex_destroy( &(SOS.uid.seq->lock) );
        pthread_mutex_destroy( &SOS.global_lock     );
        
        free( SOS.ring.send.heap );
        free( SOS.ring.recv.heap );

        freeaddrinfo( SOS.net.server_addr );
    }
    
    return;
}



//SOS_FLOW - ready
void* SOS_THREAD_post( void *args ) {
    SOS_SET_WHOAMI(whoami, "SOS_THREAD_post");
    
    while (SOS.status == SOS_STATUS_RUNNING) {
        /*
         *  Transmit messages to the daemon.
         * ...
         *
         */
    }
    return NULL;
}



//SOS_FLOW - ready
void* SOS_THREAD_read( void *args ) {
    SOS_SET_WHOAMI(whoami, "SOS_THREAD_read");

    while (SOS.status == SOS_STATUS_RUNNING) {
        /*
         *  Read the char* messages we've received and unpack into data structures.
         * ...
         *
         */
    }    
    return NULL;
}




//SOS_FLOW - ready
void* SOS_THREAD_scan( void *args ) {
    SOS_SET_WHOAMI(whoami, "SOS_THREAD_scan");
    
    while (SOS.status == SOS_STATUS_RUNNING) {
        /*
         *  Check out the dirty data and package it for sending.
         * ...
         *
         */
    }
    return NULL;
}





//SOS_FLOW - ready
long SOS_next_id( SOS_uid *id ) {
    long next_serial;

    pthread_mutex_lock( &(id->lock) );
    next_serial = id->next++;
    pthread_mutex_unlock( &(id->lock) );

    return next_serial;
}



SOS_pub* SOS_new_pub(char *title) {
    SOS_SET_WHOAMI(whoami, "SOS_new_pub");
    int i;
    SOS_pub *new_pub;

    dlog(0, "NOT SOS_FLOW READY.\n");
    
/*
    new_pub = malloc(sizeof(SOS_pub));
    memset(new_pub, '\0', sizeof(SOS_pub));

    new_pub->origin_puid   = -1;
    new_pub->announced     = 1;
    new_pub->origin_role   = SOS_ROLE;
    new_pub->origin_rank   = SOS_RANK;
    new_pub->origin_thread = -1;
    new_pub->origin_prog   = SOS_ARGV[0];
    new_pub->target_list   = SOS_DEFAULT_PUB_TARGET_LIST;
    new_pub->pragma_tag    = 0;
    new_pub->elem_count    = 0;
    new_pub->elem_max = SOS_DEFAULT_ELEM_COUNT;
    new_pub->data = malloc(sizeof(SOS_data *) * SOS_DEFAULT_ELEM_COUNT);

    new_pub->title = (char *) malloc(strlen(title) + 1);
    memset(new_pub->title, '\0', (strlen(title) + 1));
    strcpy(new_pub->title, title);

    for (i = 0; i < SOS_DEFAULT_ELEM_COUNT; i++) {
        new_pub->data[i] = malloc(sizeof(SOS_data));
        memset(new_pub->data[i], '\0', sizeof(SOS_data));
    }
    return new_pub;
*/
}



SOS_pub* SOS_new_post(char *title) {
    SOS_SET_WHOAMI(whoami, "SOS_new_post");

    SOS_pub   *new_post;
    char      *tmp_str;
    int        i;

    /*
     *  A 'post' in the sos_flow system is a pub with a single data element.
     *  The SOS_post(...) routine is used in place of SOS_pack/SOS_publish.
     *
     */
    
    new_post = malloc(sizeof(SOS_pub));
    memset(new_post, '\0', sizeof(SOS_pub));

    new_post->pub_id       = SOS_next_id( SOS.uid.pub );
    new_post->node_id      = SOS_STR_COPY_OF( SOS.config.node_id, tmp_str );
    new_post->comm_rank    = -1;
    new_post->process_id   = -1;
    new_post->thread_id    = -1;
    new_post->comm_rank    = -1;
    new_post->prog_name    = -1;
    new_post->prog_ver     = -1;
    new_post->pragma_len   = -1;
    new_post->pragma_msg   = NULL;
    new_post->title = (char *) malloc(strlen(title) + 1);
        memset(new_post->title, '\0', (strlen(title) + 1));
        strcpy(new_post->title, title);
    new_post->announced           = 0;
    new_post->elem_count          = 1;
    new_post->elem_max            = 1;

    new_post->meta.channel     = NULL;
    new_post->meta.layer       = SOS_LAYER_APP;
    new_post->meta.nature      = -1;
    new_post->meta.pri_hint    = SOS_PRI_DEFAULT;
    new_post->meta.scope_hint  = SOS_SCOPE_DEFAULT;
    new_post->meta.retain_hint = SOS_RETAIN_DEFAULT;

    new_post->data                = malloc(sizeof(SOS_data *));

    new_post->data[0] = malloc(sizeof(SOS_data));
        memset(new_post->data[i], '\0', sizeof(SOS_data));
        new_post->data[0]->guid      = -1;
        new_post->data[0]->name      = -1;
        new_post->data[0]->type      = -1;
        new_post->data[0]->len       = -1;
        new_post->data[0]->val.l_val = -1;
        new_post->data[0]->state     = SOS_VAL_STATE_EMPTY;
        new_post->data[0]->time.pack = -1.0;
        new_post->data[0]->time.send = -1.0;
        new_post->data[0]->time.recv = -1.0;

    return new_post;
}

void SOS_expand_data( SOS_pub *pub ) {
    SOS_SET_WHOAMI(whoami, "SOS_expand_data");

    int n;
    SOS_data **expanded_data;

    expanded_data = malloc((pub->elem_max + SOS_DEFAULT_ELEM_COUNT) * sizeof(SOS_data *));
    memcpy(expanded_data, pub->data, (pub->elem_max * sizeof(SOS_data *)));
    for (n = pub->elem_max; n < (pub->elem_max + SOS_DEFAULT_ELEM_COUNT); n++) {
        expanded_data[n] = malloc(sizeof(SOS_data));
        memset(expanded_data[n], '\0', sizeof(SOS_data)); }
    free(pub->data);
    pub->data = expanded_data;
    pub->elem_max = (pub->elem_max + SOS_DEFAULT_ELEM_COUNT);

    return;
}



//SOS_FLOW - ready
void SOS_strip_str( char *str ) {
    int i, len;
    len = strlen(str);

    for (i = 0; i < len; i++) {
        if (str[i] == '\"') str[i] = '\'';
        if (str[i] < ' ' || str[i] > '~') str[i] = '#';
    }
  
    return;
}



void SOS_apply_announce( SOS_pub *pub, char *msg, int msg_size ) {
    SOS_SET_WHOAMI(whoami, "SOS_apply_announce");


    SOS_val_type val_type;
    int val_id;
    int val_name_len;
    char *val_name;
    int ptr;
    int first_announce;


    if (pub->elem_count < 1) {
        first_announce = 1;
        dlog(6, "[%s]: Applying a first announce to this publication.\n", whoami);
    } else {
        first_announce = 0;
        dlog(6, "[%s]: Applying a re-announce.\n", whoami);
    }
    dlog(6, "[%s]: >>> pub->elem_count=%d   pub->elem_max=%d   msg_size=%d\n", whoami, pub->elem_count, pub->elem_max, msg_size);

    ptr = 0;

    /* De-serialize the publication header... */

    if (!first_announce) {
        free(pub->prog_name);
        free(pub->title);
    }

    pub->announced = 0;

    memcpy(&(pub->pub_id), (msg + ptr), sizeof(int));  ptr += sizeof(int);           //[origin_puid]
    memcpy(&(pub->comm_rank), (msg + ptr), sizeof(int));  ptr += sizeof(int);           //[origin_rank]
//    memcpy(&(pub->role), (msg + ptr), sizeof(int));  ptr += sizeof(int);           //[origin_role]

    memcpy(&val_name_len, (msg + ptr), sizeof(int));        ptr += sizeof(int);           //[origin_prog_len]
    pub->prog_name = (char*) malloc(1 + val_name_len);                                  //#
    memset(pub->prog_name, '\0', (1 + val_name_len));                                   //#
    memcpy(pub->prog_name, (msg + ptr), val_name_len);    ptr += (1 + val_name_len);    //[prog_name]
  
    memcpy(&val_name_len, (msg + ptr), sizeof(int));        ptr += sizeof(int);           //[title_len]
    pub->title = (char*) malloc(1 + val_name_len);                                        //#
    memset(pub->title, '\0', (1 + val_name_len));                                         //#
    memcpy(pub->title, (msg + ptr), val_name_len);          ptr += (1 + val_name_len);    //[title]
 
    dlog(7, "[%s]: Publication header information...\n[%s]:\t"
	 "pub->pub_id == %d\n[%s]:\tpub->comm_rank == %d\n[%s]:\tpub->meta.nature == %d\n[%s]:\t"
	 "pub->prog_name == \"%s\"\n[%s]:\tpub->title == \"%s\"\n",
	 whoami, whoami, pub->pub_id, whoami, pub->comm_rank, whoami, pub->meta.nature,
	 whoami, pub->prog_name, whoami, pub->title);

    /* De-serialize the data elements... */

    dlog(6, "[%s]: BEFORE>>> pub->elem_count=%d   pub->elem_max=%d   msg_size=%d\n", whoami, pub->elem_count, pub->elem_max, msg_size);

    while (ptr < msg_size) {
        memcpy(&val_type, (msg + ptr), sizeof(int)); ptr += sizeof(int);
        memcpy(&val_id, (msg + ptr), sizeof(int)); ptr += sizeof(int);
        memcpy(&val_name_len, (msg + ptr), sizeof(int)); ptr+= sizeof(int);
        val_name = (msg + ptr);

        dlog(6, "[%s]: type=%d   id=%d   name_len=%d   name=\"%s\"\n", whoami, val_type, val_id, val_name_len, val_name);

        while (val_id >= pub->elem_max) {
            /* If there is not room for this entry, make room. */
            dlog(5, "[%s]: expanding elem_max from %d ", whoami, pub->elem_max);
            SOS_expand_data(pub);
            dlog(5, "to %d.\n", pub->elem_max);
        }

        if (!first_announce) {
            /* We're "re-announcing": */
            free(pub->data[val_id]->name);
            pub->data[val_id]->name = (char *) malloc(val_name_len + 1);
            memcpy(pub->data[val_id]->name, val_name, val_name_len);
            ptr += (1 + val_name_len);
        } else {
            /* This is a new announcement, so count up the elements: */
            if (val_type == SOS_VAL_TYPE_STRING) pub->data[val_id]->val.c_val = NULL;
            pub->data[val_id]->name = (char *) malloc(val_name_len + 1);
            memset(pub->data[val_id]->name, '\0', (val_name_len + 1));
            memcpy(pub->data[val_id]->name, val_name, val_name_len);
            ptr += (1 + val_name_len);
            pub->elem_count++;
        }

        pub->data[val_id]->name[val_name_len] = '\0';
        pub->data[val_id]->type = val_type;
        pub->data[val_id]->guid = val_id;
    }

    dlog(6, "[%s]: AFTER<<< pub->elem_count=%d   pub->elem_max=%d   msg_size=%d\n", whoami, pub->elem_count, pub->elem_max, msg_size);

    return;

}



void SOS_apply_publish( SOS_pub *pub, char *msg, int msg_len ) {
    SOS_SET_WHOAMI(whoami, "SOS_apply_publish");

    //for extracting values from the msg:
    int ptr;
    int val_id;
    int val_len;
    double val_pack_ts;
    double val_send_ts;
    double val_recv_ts;
    //for inserting a string:
    char *new_str;
    //misc
    int i;


    if (SOS_DEBUG > 6) {
        for (i = 0; i < msg_len; i++) {
            printf("[%s]:   msg[%d] == %d\n", whoami, i, (int)msg[i]);
        }
    }

    dlog(6, "[%s]: Start... (pub->elem_count == %d)\n", whoami, pub->elem_count);

    ptr = 0;

    /* Skip the header... */
    ptr += (sizeof(int) * 3);

    SOS_TIME( val_recv_ts );

    while (ptr < msg_len) {

        memcpy(&val_id,      (msg + ptr), sizeof(int));     ptr += sizeof(int);
        memcpy(&val_pack_ts, (msg + ptr), sizeof(double));  ptr += sizeof(double);
        memcpy(&val_send_ts, (msg + ptr), sizeof(double));  ptr += sizeof(double);
        memcpy(&val_len,     (msg + ptr), sizeof(int));     ptr += sizeof(int);

        if (val_id >= pub->elem_count) {
            dlog(1, "[%s]: Publishing a value that is larger than the receiving data object:\n", whoami);
            dlog(1, "[%s]:    pub{%s}->elem_count == %d;  new_val_id == %d;\n", whoami, pub->title, pub->elem_count, val_id);
            dlog(1, "[%s]: This indicates that novel NAMEs were packed without an ANNOUNCE call.\n", whoami);
            dlog(1, "[%s]: Discarding this entry and doing nothing.\n", whoami);
        } else {
            pub->data[val_id]->time.pack = val_pack_ts;
            pub->data[val_id]->time.send = val_send_ts;
            pub->data[val_id]->time.recv = val_recv_ts;
            pub->data[val_id]->state = SOS_VAL_STATE_DIRTY;
        }

        switch (pub->data[val_id]->type) {
        case SOS_VAL_TYPE_INT : memcpy(   &(pub->data[val_id]->val.i_val), (msg + ptr), val_len); ptr += val_len; break;
        case SOS_VAL_TYPE_LONG : memcpy(  &(pub->data[val_id]->val.l_val), (msg + ptr), val_len); ptr += val_len; break;
        case SOS_VAL_TYPE_DOUBLE : memcpy(&(pub->data[val_id]->val.d_val), (msg + ptr), val_len); ptr += val_len; break;
        case SOS_VAL_TYPE_STRING :
            if (pub->data[val_id]->val.c_val != NULL) { 
                free(pub->data[val_id]->val.c_val);
            }
            pub->data[val_id]->val.c_val = (char *) malloc((1 + val_len) * sizeof(char));
            memset(pub->data[val_id]->val.c_val, '\0', (1 + val_len));
            memcpy(pub->data[val_id]->val.c_val, (msg + ptr), val_len); ptr += (1 + val_len); break;
        }

        switch (pub->data[val_id]->type) {
        case SOS_VAL_TYPE_INT :    dlog(5, "[%s]: >>>> pub(\"%s\")->data[%d]->val == \"%d\"\n",  whoami, pub->title, val_id, pub->data[val_id]->val.i_val); break;
        case SOS_VAL_TYPE_LONG :   dlog(5, "[%s]: >>>> pub(\"%s\")->data[%d]->val == \"%ld\"\n", whoami, pub->title, val_id, pub->data[val_id]->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE : dlog(5, "[%s]: >>>> pub(\"%s\")->data[%d]->val == \"%lf\"\n", whoami, pub->title, val_id, pub->data[val_id]->val.d_val); break;
        case SOS_VAL_TYPE_STRING : dlog(5, "[%s]: >>>> pub(\"%s\")->data[%d]->val == \"%s\"\n",  whoami, pub->title, val_id, pub->data[val_id]->val.c_val); break;
        }

        if (SOS.role == SOS_ROLE_CONTROL) {
            dlog(7, "[%s]:                        ->time.pack == %lf\n", whoami, pub->data[val_id]->time.pack);
            dlog(7, "[%s]:                        ->send_ts == %lf\n", whoami, pub->data[val_id]->time.send);
            dlog(7, "[%s]:                        ->recv_ts == %lf\n", whoami, pub->data[val_id]->time.recv);
        }

    }

    dlog(6, "[%s]: ...done.\n", whoami);

    return;
}



int SOS_pack( SOS_pub *pub, const char *name, SOS_val_type pack_type, SOS_val pack_val ) {
    SOS_SET_WHOAMI(whoami, "SOS_pack");

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
            dlog(6, "[%s]: assigning a new string.   \"%s\"   (insert)\n", whoami, pub_str_ptr);
            pub->data[i]->val = (SOS_val) pub_str_ptr;
            break;

        default :
            dlog(6, "[%s]: assigning a new value.   \"%ld\"   (insert)\n", whoami, pack_val.l_val);
            pub->data[i]->val = pack_val;
            break;
        }

        dlog(6, "[%s]: (%s) data copied in successfully.\n", whoami, name);

        pub->data[i]->guid = i;
        pub->data[i]->name = new_name;
        pub->data[i]->type = pack_type;
        pub->data[i]->state = SOS_VAL_STATE_DIRTY;
        SOS_TIME(pub->data[i]->time.pack);

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
            dlog(6, "[%s]: assigning a new string.   \"%s\"   (expanded)\n", whoami, pack_val.c_val);
            pub->data[i]->val = (SOS_val) pub_str_ptr;
            break;

        default :
            dlog(6, "[%s]: assigning a new value.   \"%ld\"   (expanded)\n", whoami, pack_val.l_val);
            pub->data[i]->val = pack_val;
            break;

        }

        dlog(6, "[%s]: ALMOST DONE....\n", whoami);

        pub->data[i]->guid = i;
        pub->data[i]->name = new_name;
        pub->data[i]->type = pack_type;
        pub->data[i]->state = SOS_VAL_STATE_DIRTY;
        SOS_TIME(pub->data[i]->time.pack);

        dlog(6, "[%s]: (%s) successfully inserted [%s] at position %d. (DONE)\n", whoami, name, pub->data[i]->name, i);
        dlog(6, "[%s]: --------------------------------------------------------------\n", whoami);

        return i;
    }

    //shouln't ever get here.
    return -1;
}

void SOS_repack( SOS_pub *pub, int index, SOS_val pack_val ) {
    SOS_data *data;
    int len;

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
    new_sub->pub = SOS_new_pub("---empty---");

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
    if (SOS_DEBUG != 0 ) {
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
            fprintf(output_to, "| %11d,%12d,%12s,", i, pub->data[i]->guid, SOS_TYPE_LOOKUP[pub->data[i]->type]);
            fprintf(output_to, " %c %20s | = ", ((pub->data[i]->state == SOS_VAL_STATE_DIRTY) ? '*' : ' '), pub->data[i]->name);
            switch (pub->data[i]->type) {
            case SOS_VAL_TYPE_INT : fprintf(output_to, "%d", pub->data[i]->val.i_val); break;
            case SOS_VAL_TYPE_LONG : fprintf(output_to, "%ld", pub->data[i]->val.l_val); break;
            case SOS_VAL_TYPE_DOUBLE : fprintf(output_to, "%lf", pub->data[i]->val.d_val); break;
            case SOS_VAL_TYPE_STRING : fprintf(output_to, "\"%s\"", pub->data[i]->val.c_val); break; }
            fprintf(output_to, "\n");
        }
        fprintf(output_to, "\\---------------------------------------------------------------/\n\n");
    }
    return;
}




/* **************************************** */
/* [pub]                                    */
/* **************************************** */


void SOS_announce( SOS_pub *pub ) {
    SOS_SET_WHOAMI(whoami, "SOS_announce");

    int i, n_byte, name_len;
    int composite_tag;
    int ptr;
    int composite_len;
    char *c_ptr;
    char *buffer;
    //Used for packing, to increase readability:
    int prog_len;
    int title_len;
    int pack_type;
    int pack_id;
    int pack_name_len;
    char *pack_name;

    char SOS_ann_buffer[SOS_DEFAULT_BUFFER_LEN];

    buffer = SOS_ann_buffer;
  
    composite_len = 0;


    /* Make space for fixed header data.
     *
     * NOTE: [ active ][  rank  ][  role  ] are ALWAYS the first
     *       3 (int) components at the head of every pub-related message!
     */

    composite_len += sizeof(int);                       //[puid]
    composite_len += sizeof(int);                       //[rank]
    composite_len += sizeof(int);                       //[role]

    composite_len += sizeof(int);                       //[target_list]
    composite_len += sizeof(int);                       //[prog_len]
    composite_len += 1 + strlen(pub->prog_name);      //[prog]
    composite_len += sizeof(int);                       //[pub_title_len]
    composite_len += 1 + strlen(pub->title);            //[pub_title]

    /* TODO:{ PRAGMA_TAG, THREAD } */
  
    dlog(7, "[%s]: pub->prog_name == \"%s\"\n", whoami, pub->prog_name);

    /* Make space for each element... (structure only) */    

    for (i = 0; i < pub->elem_count; i++) {
        composite_len += sizeof(int);                     //[type]
        composite_len += sizeof(int);                     //[id]
        composite_len += sizeof(int);                     //[name_len]
        composite_len += 1 + strlen(pub->data[i]->name);  //[name]
    }

    //buffer = (char *) malloc(composite_len);
    //if (buffer == NULL) {
    //  dlog(0, "[%s]: FAILURE!! Could not allocate memory.  char *buffer == NULL; /* after malloc()! */", whoami);
    //  //POISON_Abort();
    //  exit(1);
    //}

    memset(buffer, '\0', composite_len);

    dlog(6, "[%s]:  ---- {ANN} NEW\n", whoami);

    ptr = 0;

    /* Insert the header values... */

    dlog(6, "[%s]:   Loading the annoucement with header information...\n[%s]:\t"
         "pub->pub_id == %d\n[%s]:\tpub->comm_rank == %d\n[%s]:\tpub->meta.nature == %d\n[%s]:\t"
         "pub->prog_name == \"%s\"\n[%s]:\tpub->title == \"%s\"\n",
         whoami, whoami, pub->pub_id, whoami, pub->comm_rank, whoami, pub->meta.nature,
         whoami, pub->prog_name, whoami, pub->title);

    prog_len = strlen(pub->prog_name);
    title_len = strlen(pub->title);

    memcpy((buffer + ptr), &(pub->pub_id), sizeof(int));   ptr += sizeof(int);
    memcpy((buffer + ptr), &(pub->comm_rank), sizeof(int));   ptr += sizeof(int);
    memcpy((buffer + ptr), &(pub->meta.nature), sizeof(int));   ptr += sizeof(int);
    memcpy((buffer + ptr), &prog_len, sizeof(int));      ptr += sizeof(int);
    memcpy((buffer + ptr), pub->prog_name, prog_len);  ptr += (1 + prog_len);
    memcpy((buffer + ptr), &title_len, sizeof(int));            ptr += sizeof(int);
    memcpy((buffer + ptr), pub->title, title_len);              ptr += (1 + title_len);

    /* Insert the per-element values... */
  
    for (i = 0; i < pub->elem_count; i++) {
        pack_type     = (int) pub->data[i]->type;
        pack_id       = (int) pub->data[i]->guid;
        pack_name_len = strlen(pub->data[i]->name);
        dlog(6, "[%s]:    [%d]:  type: %d   \tid: %d   \tname_len: %d   \tname:\"%s\"\n", whoami, ptr, pack_type, pack_id, pack_name_len, pub->data[i]->name);   
        memcpy((buffer + ptr), &pack_type, sizeof(int));             ptr += sizeof(int);
        memcpy((buffer + ptr), &pack_id, sizeof(int));               ptr += sizeof(int);
        memcpy((buffer + ptr), &pack_name_len, sizeof(int));         ptr += sizeof(int);
        memcpy((buffer + ptr), pub->data[i]->name, pack_name_len);  ptr += (1 + pack_name_len);
    }

    dlog(6, "[%s]:  ---- {ANN} DONE   LEN = %d  PTR = %d\n", whoami, composite_len, ptr);

    //PPOISON_Send(buffer, composite_len, //POISON_CHAR, target_rank, SOS_MSG_ANNOUNCE, target_comm);

    //free(buffer);

    dlog(6, "[%s]:  ---- free(buffer); completed.\n", whoami);

    /* 
     * Only SOS_APP can automatically set this.
     *
     * Because SOS modules might need to announced this same pub to many different roles, this
     * is not the place to mark it as 'announced'.  Rather, this flag should be manually
     * set by the SOS module's creator to account for their particular contextual requirements.
     *
     * So long as it is set to 0, any call to SOS_publish() will recursively call
     * SOS_announce().  This can save your hide, or be terribly inneficient if you never
     * manually set this after a proper group announcement.
     *
     */

    /* TODO:{ ANNOUNCE, CHAD }
     *
     * Turn this into a bit-field w/per-role discrimination, and then it can be truly automatic.
     */
    
    if (SOS.role == SOS_ROLE_CLIENT) pub->announced = 1;

    return;
}


void SOS_publish( SOS_pub *pub ) {
    SOS_SET_WHOAMI(whoami, "SOS_publish");

    int i, n_byte, name_len;
    int ptr;
    int composite_len;
    int composite_tag;
    char *c_ptr;
    char *buffer;
    int pack_id;
    int pack_len;
    double pack_pack_ts;
    double pack_send_ts;
    SOS_val pack_val;
    //misc
    int dbgcnt;
    int oldptrloc;

    char SOS_pub_buffer[SOS_DEFAULT_BUFFER_LEN];

    buffer = SOS_pub_buffer;
  
    if (pub->announced == 0) {
        dlog(7, "[%s]: Attempting to publish an SOS_pub announcing.\n[%s]: This is "    \
             "probably forgetting to manually set \"pub->announced = 1;\" within an sos_cloud" \
             " module.\n[%s]: Recursively calling SOS_announce_to() so we're safe,"            \
             "but this is not efficient!\n", whoami, whoami, whoami);

        /* TODO:{ ANNOUNCE, CHAD }
         *
         * Turn this into a bit field per role, like the target list, and then it is truly automatic.
         */

        SOS_announce( pub );
    }

    /*
     *  There are some conditionals missing because the use cases has changed.
     *  That is, the sos library is now flattened to a single layer of
     *  one-way dumping to the DAEMON process, which does 'something else'
     *  to move the data onto the backplane SQL database.
     *
     *  Essentially, functions that are packing/announcing/publishing stuff
     *  are now ONLY ever called by the front-end SOS_ROLE_CLIENT's.
     *
     */


        SOS_TIME( pack_send_ts );
        dlog(5, "[%s]: pack_send_ts == %lf\n", whoami, pack_send_ts);


    /* Determine the required buffer length for all dirty values. */

    composite_len = 0;

    composite_len += sizeof(int);        // [puid]     /* Standard pub-message three element header. */
    composite_len += sizeof(int);        // [rank]
    composite_len += sizeof(int);        // [role]

    for (i = 0; i < pub->elem_count; i++) {
        if (pub->data[i]->state == SOS_VAL_STATE_CLEAN) continue;

            /* Update the sent_ts value only if we're the original sender. */
            pub->data[i]->time.send = pack_send_ts;

        composite_len += sizeof(int);      // [   id   ]
        composite_len += sizeof(double);   // [pack_ts ]
        composite_len += sizeof(double);   // [ send_ts]
        composite_len += sizeof(int);      // [ length ]
        switch ( pub->data[i]->type ) {    // [  data  ]
        case SOS_VAL_TYPE_INT :    composite_len += sizeof(int); break;
        case SOS_VAL_TYPE_LONG :   composite_len += sizeof(long); break;
        case SOS_VAL_TYPE_DOUBLE : composite_len += sizeof(double); break;
        case SOS_VAL_TYPE_STRING : composite_len += (1 + strlen(pub->data[i]->val.c_val)); break; }
    }

    /* Fill the buffer with the dirty values. */
 
    //buffer = (char *) malloc(composite_len);

    //if (buffer == NULL) {
    //  dlog(0, "[%s]: FAILURE!! Could not allocate memory.  char *buffer == NULL; /* after malloc()! */", whoami);
    //  //POISON_Abort();
    //  exit(1);
    // }  
  
    memset(buffer, '\0', (composite_len));

    ptr = 0;

    memcpy((buffer + ptr), &(pub->pub_id), sizeof(int));    ptr += sizeof(int);
    memcpy((buffer + ptr), &(pub->comm_rank), sizeof(int));    ptr += sizeof(int);
    memcpy((buffer + ptr), &(pub->meta.nature), sizeof(int));    ptr += sizeof(int);

    for (i = 0; i < pub->elem_count; i++) {
        if (pub->data[i]->state == SOS_VAL_STATE_CLEAN) continue;

        pack_id      = pub->data[i]->guid;
        pack_pack_ts = pub->data[i]->time.pack;
        pack_send_ts = pub->data[i]->time.send;
        pack_val     = pub->data[i]->val;
        switch ( pub->data[i]->type ) {
        case SOS_VAL_TYPE_INT :    pack_len = sizeof(int);    break;
        case SOS_VAL_TYPE_LONG :   pack_len = sizeof(long);   break;
        case SOS_VAL_TYPE_DOUBLE : pack_len = sizeof(double); break;
        case SOS_VAL_TYPE_STRING : pack_len = (int)(strlen( pub->data[i]->val.c_val)); break; }

        dlog(6, "[%s]:    ----\n", whoami);
        dlog(6, "[%s]:      [%d] + (int):%d    \t\"%d\"\n", whoami, ptr, (int)sizeof(int), pack_id);
        dlog(6, "[%s]:      [%d] + (int):%d    \t\"%d\"\n", whoami, (int)(ptr + sizeof(int)), (int)sizeof(int), pack_len);
        switch ( pub->data[i]->type ) {
        case SOS_VAL_TYPE_INT :    dlog(6, "[%s]:      [%d] + (int):%d    \t\"%d\"\n",  whoami, (int)(ptr + (sizeof(int) * 2)), pack_len, pub->data[i]->val.i_val); break;
        case SOS_VAL_TYPE_LONG :   dlog(6, "[%s]:      [%d] + (long):%d   \t\"%ld\"\n", whoami, (int)(ptr + (sizeof(int) * 2)), pack_len, pub->data[i]->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE : dlog(6, "[%s]:      [%d] + (double):%d \t\"%lf\"\n", whoami, (int)(ptr + (sizeof(int) * 2)), pack_len, pub->data[i]->val.d_val); break;
        case SOS_VAL_TYPE_STRING : dlog(6, "[%s]:      [%d] + (string):%d \t\"%s\"\n",  whoami, (int)(ptr + (sizeof(int) * 2)), pack_len, pub->data[i]->val.c_val); break; }

        memcpy((buffer + ptr), &pack_id, sizeof(int));         ptr += sizeof(int);
        memcpy((buffer + ptr), &pack_pack_ts, sizeof(double)); ptr += sizeof(double);
        memcpy((buffer + ptr), &pack_send_ts, sizeof(double)); ptr += sizeof(double);
        memcpy((buffer + ptr), &pack_len, sizeof(int));        ptr += sizeof(int);

        /* Above we write in the value of the pack_len, which is as long as an int.
         * Below we write in the data element's value, that has length of pack_len's value. */

        switch (pub->data[i]->type) {
        case SOS_VAL_TYPE_INT :    memcpy((buffer + ptr), &pack_val.i_val, pack_len);   ptr += pack_len;       break;
        case SOS_VAL_TYPE_LONG :   memcpy((buffer + ptr), &pack_val.l_val, pack_len);   ptr += pack_len;       break;
        case SOS_VAL_TYPE_DOUBLE : memcpy((buffer + ptr), &pack_val.d_val, pack_len);   ptr += pack_len;       break;
        case SOS_VAL_TYPE_STRING  : memcpy((buffer + ptr), pack_val.c_val,  pack_len);   ptr += (1 + pack_len); break; }

        /*
         *  NOTE: Only SOS_APP roles can safely clear the dirty flag on a send.
         *        Within the sos_cloud roles, there are a variety of valid communication
         *        patterns, and this requires that people clear dirty flags manually.
         *
         *        Dirty doesn't necessarily mean "hasn't been sent to ROLE____, for some
         *        modules, it might mean it has not been comitted to a SQL store, or
         *        perhaps fed into some automatic analysis engine.
         */

        if (SOS.role == SOS_ROLE_CLIENT) pub->data[i]->state = SOS_VAL_STATE_CLEAN;

    }

    //PPOISON_Send(buffer, composite_len, //POISON_CHAR, target_rank, SOS_MSG_PUBLISH, target_comm);

    //free(buffer);

    dlog(6, "[%s]:  free(buffer); completed\n", whoami);
    return;
}


void SOS_unannounce( SOS_pub *pub ) {

    /* TODO:{ UNANNOUNCE, CHAD } */

    return;
}




/* **************************************** */
/* [sub]                                    */
/* **************************************** */


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
