
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "sos.h"
#include "sos_types.h"
#include "sos_buffer.h"
#include "sos_target.h"

#define MODE_CLIENT 1
#define MODE_SERVER 2

#define USAGE ""                                                \
    "Usage:  ./test_target --mode <[client] | [server]>\n"      \
    "                      --port <number>\n"                   \
    ""

int  g_MODE;
int  g_PORT;

SOS_runtime *g_sos;

void testClient();
void testServer();

void dirtyCustomServerInit(SOS_socket **tgt_ptr);

//SERVER -----
void testServer(void) {
    SOS_buffer *msg = NULL;
    SOS_buffer_init_sized_locking(g_sos, &msg, 1024, false);

    SOS_socket *net = NULL;

    //NOTE: This is NOT called by the daemon:
    SOS_target_init(g_sos, &net, "localhost", g_PORT);

    //Daemon code is duplicated here:
    //dirtyCustomServerInit(&net);
    
    SOS_target_setup_for_accept(net);

    SOS_target_accept_connection(net);
    SOS_target_recv_msg(net, msg);

    int offset = 0;
    char *recv = NULL;
    SOS_buffer_unpack_safestr(msg, &offset, &recv);

    printf("SERVER received: %s    (done)\n", recv);
    fflush(stdout);

    return;
}



//CLIENT -----
void testClient(void) {

    SOS_buffer *msg = NULL;
    SOS_buffer_init_sized_locking(g_sos, &msg, 1024, false);

    char   sentence[]  = { "Hello from client!" };
    int    offset     = 0;

    offset += SOS_buffer_pack(msg, &offset, "s", sentence); 

    SOS_socket *tgt = NULL;
    SOS_target_init(g_sos, &tgt, "localhost", g_PORT);
    SOS_target_connect(tgt);

    SOS_target_send_msg(tgt, msg); 

    SOS_target_disconnect(tgt);
    SOS_target_destroy(tgt);

    printf("CLIENT sent message.     (done)\n");

    return;
}


//DAEMON's CUSTOM SOCKET INIT -----
void dirtyCustomServerInit(SOS_socket **tgt_ptr) {
    //NOTE: This is duplicated from SOS_target_init() since we're starting
    // up by initializing all this stuff manually before even SOS_init()
    SOS_socket *tgt = (SOS_socket *) calloc(1, sizeof(SOS_socket));
    *tgt_ptr = tgt;

    tgt->send_lock = (pthread_mutex_t *) calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_lock(tgt->send_lock);

    // Grab the port from the environment variable SOS_CMD_PORT
    snprintf(tgt->local_port, NI_MAXSERV, "%d", g_PORT);
    tgt->port_number = g_PORT;

    tgt->listen_backlog           = 20;
    tgt->buffer_len               = SOS_DEFAULT_BUFFER_MAX;
    tgt->timeout                  = SOS_DEFAULT_MSG_TIMEOUT;

    //Set standard SOS hints:
    tgt->local_hint.ai_family     = AF_UNSPEC;   // Allow IPv4 or IPv6
    tgt->local_hint.ai_socktype   = SOCK_STREAM; // _STREAM/_DGRAM/_RAW
    tgt->local_hint.ai_protocol   = 0;           // 0: All   IPPROTO_TCP: TCP only
    tgt->local_hint.ai_flags      = AI_PASSIVE;
                                    // AI_PASSIVE: Be able to accept connections.
                                    // AI_NUMERICSERV: Don't invoke namserv.
                                    //                 BUT cannot use "localhost"!
    tgt->sos_context = g_sos;
    
    pthread_mutex_unlock(tgt->send_lock);
    // --- end duplication of SOS_target_init();

    return;
}

//MAIN for this test program...
int main(int argc, char **argv) {

    g_MODE = -1;
    g_PORT = -1;

    int elem;
    int next_elem;
    for (elem = 1; elem < argc;) {
        if ((next_elem = elem + 1) == argc) {
            fprintf(stderr, "%s\n", USAGE);
            exit(EXIT_FAILURE);
        }

        if (strcmp(argv[elem], "--mode"  ) == 0) {
            if (strcmp(argv[next_elem], "client") == 0) {
                g_MODE = MODE_CLIENT;
            } else if (strcmp(argv[next_elem], "server") == 0) {
                g_MODE = MODE_SERVER;
            }
        } else if (strcmp(argv[elem], "--port") == 0) {
            g_PORT = atoi(argv[next_elem]);                
        } else {
            fprintf(stderr, "Unknown flag: %s %s\n", argv[elem], argv[next_elem]);
        }
        elem = next_elem + 1;
    }

    if (g_MODE == -1) {
        fprintf(stderr, "Please select a valid testing mode.\n");
        exit(EXIT_FAILURE);
    }

    if (g_PORT == -1) {
        fprintf(stderr, "Please select a valid port.\n");
        exit(EXIT_FAILURE);
    }

    SOS_init(&g_sos, SOS_ROLE_OFFLINE_TEST_MODE, SOS_RECEIVES_NO_FEEDBACK, NULL); 
    if (g_sos == NULL) {
        fprintf(stderr, "test_target: Unable to connect to SOS daemon.  Terminating.\n");
        exit(EXIT_FAILURE);
    }


    switch (g_MODE) {
    case MODE_CLIENT: testClient(); break;
    case MODE_SERVER: testServer(); break;
    default:
        fprintf(stderr, "Invalid testing mode: %d\n", g_MODE);
        exit(EXIT_FAILURE);
    }

    SOS_finalize(g_sos);

    return EXIT_SUCCESS;
}
