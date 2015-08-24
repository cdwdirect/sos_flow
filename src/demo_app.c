
/*
 * demo_app.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include "sos.h"

int demo_run;


void* repub_thread(void *arg) {

  while (demo_run) {

  }

  return NULL;
}


int main(int argc, char *argv[]) {
  int i, thread_support;
  char pub_title[SOS_DEFAULT_STRING_LEN];
  SOS_pub *pub;
  SOS_sub *sub;
  pthread_t repub_t;
  double timenow;

  demo_run = 1;
  pthread_create(&repub_t, NULL, repub_thread, (void*)pub);

  while (demo_run) {
      sleep(1);
  }


  pthread_join(repub_t, NULL);

  return (EXIT_SUCCESS);
}
