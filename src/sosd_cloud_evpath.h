#ifndef SOS_CLOUD_EVPATH_H
#define SOS_CLOUD_EVPATH_H

#include "sos.h"
#include "sos_debug.h"
#include "sosd.h"


int   SOSD_cloud_init(int *argc, char ***argv);
int   SOSD_cloud_start(void);
int   SOSD_cloud_send(SOS_buffer *buffer, SOS_buffer *reply);
void  SOSD_cloud_enqueue(SOS_buffer *buffer);
void  SOSD_cloud_fflush(void);
int   SOSD_cloud_finalize(void);
void  SOSD_cloud_shutdown_notice(void);
void  SOSD_cloud_listen_loop(void);


#endif
