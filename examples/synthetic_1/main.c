/******************************************************************************
*   SOS_Flow example.
******************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include <mpi.h>
#include "worker.h"

int main (int argc, char *argv[]) 
{

  int rc = MPI_SUCCESS;
  int provided = 0;
  rc = MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
  printf("MPI_Init_thread: provided = %d, MPI_THREAD_MULTIPLE=%d\n", provided, MPI_THREAD_MULTIPLE);
  if (rc != MPI_SUCCESS) {
    char *errorstring;
    int length = 0;
    MPI_Error_string(rc, errorstring, &length);
    printf("Error: MPI_Init failed, rc = %d\n%s\n", rc, errorstring);
    exit(1);
  }

  worker();

  MPI_Finalize();
  printf ("Done.\n");

  return 0;
}

