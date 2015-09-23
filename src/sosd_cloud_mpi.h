#ifndef SOSD_CLOUD_MPI_H
#define SOSD_CLOUD_MPI_H

#include <mpi.h>

#include "sos.h"
#include "sos_debug.h"
#include "sosd.h"

int SOSD_cloud_init(int *argc, char ***argv);
int SOSD_cloud_send(char *msg, int msg_len);
int SOSD_cloud_finalize();

#endif
