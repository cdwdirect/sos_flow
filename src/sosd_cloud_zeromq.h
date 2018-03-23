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
void  SOSD_cloud_enqueue(SOS_buffer *buffer);
void  SOSD_cloud_fflush(void);
int   SOSD_cloud_finalize(void);
void  SOSD_cloud_shutdown_notice(void);
void  SOSD_cloud_listen_loop(void);
void  SOSD_cloud_handle_triggerpull(SOS_buffer *msg);

void  SOSD_aggregator_register_listener(SOS_buffer *msg);

typedef struct _buffer_rec {
        int            size;
        SOS_msg_type   type;
        unsigned char *data;
} buffer_rec, *buffer_rec_ptr;


#endif
