#ifndef SOS_CLOUD_SOCKET_H
#define SOS_CLOUD_SOCKET_H

#include "sos.h"
#include "sos_types.h"
#include "sos_debug.h"
#include "sosd.h"
#include "string.h"


int   SOSD_cloud_init(int *argc, char ***argv);
int   SOSD_cloud_start(void);
int   SOSD_cloud_send(SOS_buffer *buffer, SOS_buffer *reply);
void  SOSD_cloud_enqueue(SOS_buffer *buffer);
void  SOSD_cloud_fflush(void);
int   SOSD_cloud_finalize(void);
void  SOSD_cloud_shutdown_notice(void);
void  SOSD_cloud_listen_loop(void);

void  SOSD_cloud_process_buffer(SOS_buffer *msg);

void  SOSD_sockets_register_connection(SOS_buffer *msg);
void  SOSD_cloud_handle_triggerpull(SOS_buffer *msg);

typedef struct _buffer_rec {
        int            size;
        SOS_msg_type   type;
        unsigned char *data;
} buffer_rec, *buffer_rec_ptr;

#endif
