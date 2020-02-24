#ifndef SOSD_CLOUD_ZEROMQ_H
#define SOSD_CLOUD_ZEROMQ_H

#include <string.h>

#include "sos.h"
#include "sos_types.h"
#include "sos_debug.h"
#include "sosd.h"

#include "czmq.h"

int   SOSD_cloud_init(int *argc, char ***argv);
int   SOSD_cloud_start(void);
int   SOSD_cloud_send(SOS_buffer *buffer, SOS_buffer *reply);
void  SOSD_cloud_send_to_topology(SOS_buffer *buffer, SOS_topology topology);
void  SOSD_cloud_enqueue(SOS_buffer *buffer);
void  SOSD_cloud_fflush(void);
int   SOSD_cloud_finalize(void);
void  SOSD_cloud_shutdown_notice(void);
void  SOSD_cloud_listen_loop(void);
void  SOSD_cloud_handle_triggerpull(SOS_buffer *msg);
void  SOSD_cloud_process_buffer(SOS_buffer *msg);
void  SOSD_cloud_handle_daemon_registration(SOS_buffer *msg);

void *SOSD_THREAD_ZEROMQ_listen_wrapper(void *not_used);

typedef struct _buffer_rec {
        int            size;
        SOS_msg_type   type;
        unsigned char *data;
} buffer_rec, *buffer_rec_ptr;

SOSD_zeromq *zmq;
#endif




/*
 *
// FROM sosd.h
//
//

#ifdef SOSD_CLOUD_SYNC_WITH_ZEROMQ
    void               *zeromq;
#endif
} SOSD_runtime;

#ifdef SOSD_CLOUD_SYNC_WITH_ZEROMQ
typedef struct {
    void               *conn;
    char                conn_str[512];
    char               *remote_host;
    int                 remote_port;
    SOS_role            role;
} SOSD_zeromq_node;
#endif



//
//
//
#include <zmq.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

int main (void)
{
    printf ("Connecting to hello world server…\n");
    void *context = zmq_ctx_new ();
    void *requester = zmq_socket (context, ZMQ_REQ);
    zmq_connect (requester, "tcp://localhost:5555");

    int request_nbr;
    for (request_nbr = 0; request_nbr != 10; request_nbr++) {
        char buffer [10];
        printf ("Sending Hello %d…\n", request_nbr);
        zmq_send (requester, "Hello", 5, 0);
        zmq_recv (requester, buffer, 10, 0);
        printf ("Received World %d\n", request_nbr);
    }
    zmq_close (requester);
    zmq_ctx_destroy (context);
    return 0;
}

 *
 *
 *
//  Hello World server

#include <zmq.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>

int main (void)
{
    //  Socket to talk to clients
    void *context = zmq_ctx_new ();
    void *responder = zmq_socket (context, ZMQ_REP);
    int rc = zmq_bind (responder, "tcp://*:5555");
    assert (rc == 0);

    while (1) {
        char buffer [10];
        zmq_recv (responder, buffer, 10, 0);
        printf ("Received Hello\n");
        sleep (1);          //  Do some 'work'
        zmq_send (responder, "World", 5, 0);
    }
    return 0;
}
   */


