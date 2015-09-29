
#include <mpi.h>

#include "sos.h"
#include "sos_debug.h"
#include "sos_error.h"
#include "sosd.h"
#include "sosd_cloud_mpi.h"

int SOSD_cloud_send(char *msg, int msg_len) {
    char  mpi_err[MPI_MAX_ERROR_STRING];
    int   mpi_err_len = MPI_MAX_ERROR_STRING;


    return 0;
}

int SOSD_cloud_init(int *argc, char ***argv) {
    char  whoami[] = "daemon.X.SOSD_cloud_init(MPI)";
    char  mpi_err[MPI_MAX_ERROR_STRING];
    int   mpi_err_len = MPI_MAX_ERROR_STRING;
    int   rc;

    if (SOSD_ECHO_TO_STDOUT) printf("[%s]: Configuring this daemon with MPI:\n", whoami);
    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... calling MPI_Init_thread();\n", whoami);
    rc = MPI_Init_thread( argc, argv, MPI_THREAD_SERIALIZED, &SOS.config.comm_support );
    if (rc != MPI_SUCCESS) {
        MPI_Error_string( rc, mpi_err, &mpi_err_len );
        printf("[%s]:   ... MPI_Init_thread() did not complete successfully!\n", whoami);
        printf("[%s]:   ... Error %d: %s\n", whoami, rc, mpi_err);
        exit( EXIT_FAILURE );
    }
    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... safely returned.\n", whoami);
    
    switch (SOS.config.comm_support) {
    case MPI_THREAD_SINGLE:     if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... supported: MPI_THREAD_SINGLE (could cause problems)\n", whoami); break;
    case MPI_THREAD_FUNNELED:   if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... supported: MPI_THREAD_FUNNELED (could cause problems)\n", whoami); break;
    case MPI_THREAD_SERIALIZED: if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... supported: MPI_THREAD_SERIALIZED\n", whoami); break;
    case MPI_THREAD_MULTIPLE:   if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... supported: MPI_THREAD_MULTIPLE\n", whoami); break;
    default:                    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... WARNING!  The supported threading model (%d) is unrecognized!\n", whoami, SOS.config.comm_support); break;
    }
    rc = MPI_Comm_rank( MPI_COMM_WORLD, &SOS.config.comm_rank );
    rc = MPI_Comm_size( MPI_COMM_WORLD, &SOS.config.comm_size );
    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... rank: %d\n", whoami, SOS.config.comm_rank);
    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... size: %d\n", whoami, SOS.config.comm_size);
    if (SOSD_ECHO_TO_STDOUT) printf("[%s]:   ... done.\n", whoami);

    return 0;
}


int SOSD_cloud_finalize() {
    SOS_SET_WHOAMI(whoami, "SOSD_cloud_finalize(MPI)");
    char  mpi_err[MPI_MAX_ERROR_STRING];
    int   mpi_err_len = MPI_MAX_ERROR_STRING;
    int   rc;

    dlog(1, "[%s]: Leaving the MPI communicator...\n", whoami);
    rc = MPI_Finalize();
    if (rc != MPI_SUCCESS) {
        MPI_Error_string( rc, mpi_err, &mpi_err_len );
        dlog(1, "[%s]:   ... MPI_Finalize() did not complete successfully!\n", whoami);
        dlog(1, "[%s]:   ... Error %d: %s\n", whoami, rc, mpi_err);
    }
    dlog(1, "[%s]:   ... Clearing the SOS.config.comm_* fields.\n", whoami);
    SOS.config.comm_rank    = -1;
    SOS.config.comm_size    = -1;
    SOS.config.comm_support = -1;
    dlog(1, "[%s]:   ... done.\n", whoami);

    return 0;
}
