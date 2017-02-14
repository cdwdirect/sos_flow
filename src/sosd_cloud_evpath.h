#ifndef SOS_CLOUD_EVPATH_H
#define SOS_CLOUD_EVPATH_H

#include "sos.h"
#include "sos_debug.h"
#include "sosd.h"
#include "evpath.h"
#include "ev_dfg.h"


int   SOSD_cloud_init(int *argc, char ***argv);
int   SOSD_cloud_start(void);
int   SOSD_cloud_send(SOS_buffer *buffer, SOS_buffer *reply);
void  SOSD_cloud_enqueue(SOS_buffer *buffer);
void  SOSD_cloud_fflush(void);
int   SOSD_cloud_finalize(void);
void  SOSD_cloud_shutdown_notice(void);
void  SOSD_cloud_listen_loop(void);

typedef struct _buffer_rec {
        int            size;
        unsigned char *data;
} buffer_rec, *buffer_rec_ptr;

static FMField buffer_field_list[] =
{
        {"size", "integer", sizeof(int),            FMOffset(buffer_rec_ptr, size)},
        {"data", "string",  sizeof(unsigned char*), FMOffset(buffer_rec_ptr, data)},
        {NULL, NULL, 0, 0}
};

static FMStructDescRec buffer_format_list[] =
{
        {"buffer", buffer_field_list, sizeof(buffer_rec), NULL},
        {NULL, NULL}
};


#endif
