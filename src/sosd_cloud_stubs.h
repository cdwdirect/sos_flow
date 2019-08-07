#ifndef SOS_CLOUD_STUBS_H
#define SOS_CLOUD_STUBS_H

#include "sos.h"
#include "sos_debug.h"
#include "sosd.h"

int SOSD_cloud_init(int *argc, char ***argv);
int SOSD_cloud_send(char *msg, int msg_len);
int SOSD_cloud_finalize();

#endif
