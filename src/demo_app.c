
/*
 * demo_app.c
 *
 * MPI+pthreads application to demo use of SOS.  (STUBS for now...)
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <mpi.h>

#include "sos.h"

void* repub_thread(void *arg) {

  while (demo_run) {

  }

  return NULL;
}


int main(int argc, char *argv[]) {
  int i, thread_support;
  char pub_title[SOS_DEFAULT_STRING_LEN];
  SOS_pub_handle *pub;
  SOS_sub_handle *sub;
  MPI_Status status;
  pthread_t repub_t;
  double timenow;


  MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &thread_support);


  demo_run = 1;
  pthread_create(&repub_t, NULL, repub_thread, (void*)pub);
  while (demo_run) { sleep(1); }
  pthread_join(repub_t, NULL);



  MPI_Finalize();
 
  return 0;
}
