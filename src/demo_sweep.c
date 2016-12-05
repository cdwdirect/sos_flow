
/*
 * demo_app.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#if (SOSD_CLOUD_SYNC > 0)
#include <mpi.h>
#endif

#define DEFAULT_MAX_SEND_COUNT 2400
#define DEFAULT_ITERATION_SIZE 25

#define USAGE ""                                        \
  "./demo_sweep\n"                                      \
  "\t\t-imin <iter> -imax <iter> -istep <iters>\n"      \
  "\t\t-smin <size> -smax <size> -sstep <sizes>\n"      \
  "\t\t-dmin <usec> -dmax <usec> -dstep <usecs>"

#define zlog(...) if (rank == 0) { printf(__VA_ARGS__); fflush(stdout); }

#include "sos.h"
#include "sos_debug.h"

void SWEEP_wait_for_empty_queue(SOS_runtime *my_sos);


int  rank;
char host[MPI_MAX_PROCESSOR_NAME];
int  host_len;

int main(int argc, char *argv[]) {

    SOS_runtime *my_sos;
    SOS_pub *pub;

    int    IMIN, IMAX, ISTEP;
    int    SMIN, SMAX, SSTEP;
    int    DMIN, DMAX, DSTEP;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Get_processor_name(host, &host_len);

    if (rank == 0) {
      printf("\"rank\",\"host\",\"pid\"\n");
      fflush(stdout);
    } else {
      usleep(10000);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    printf("%d, \"%s\", %d\n", rank, host, getpid());
    fflush(stdout);

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
      printf("----------\n");
      fflush(stdout);
    } else {
      usleep(10000);
    }

    /* Process command-line arguments */
    if ( argc < 10 ) { fprintf(stderr, "Error: Invalid number of command line arguments.\n\n%s\n", USAGE); exit(1); }

    IMIN = IMAX = ISTEP = -1;
    SMIN = SMAX = SSTEP = -1;
    DMIN = DMAX = DSTEP = -1;

    int elem, next_elem;

    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) {
            fprintf(stderr, "%s\n", USAGE);
            exit(1);
        }

        if ( strcmp(argv[elem], "-imin"  ) == 0) {
            IMIN  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-imax"  ) == 0) {
            IMAX  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-istep"  ) == 0) {
            ISTEP = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-smin"  ) == 0) {
            SMIN  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-smax"  ) == 0) {
            SMAX  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-sstep"  ) == 0) {
            SSTEP = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-dmin"  ) == 0) {
            DMIN  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-dmax"  ) == 0) {
            DMAX  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-dstep"  ) == 0) {
            DSTEP = atoi(argv[next_elem]);
        } else {
            fprintf(stderr, "Unknown flag: %s %s\n", argv[elem], argv[next_elem]);
        }
        elem = next_elem + 1;
    }

    if (    (IMIN  < 0)
         || (IMAX  < 0)
         || (ISTEP < 0)
         || (SMIN  < 0)
         || (SMAX  < 0)
         || (SSTEP < 0)
         || (DMIN  < 0)
         || (DMAX  < 0)
         || (DSTEP < 0) ) {
      fprintf(stderr, "%s\n", USAGE); exit(1);
    }

    my_sos = SOS_init( &argc, &argv, SOS_ROLE_CLIENT, SOS_LAYER_APP);
    SOS_SET_CONTEXT(my_sos, "demo_sweep.main");
    srandom(my_sos->my_guid);

    zlog("demo_sweep starting...\n");

    int iter_now = 0;
    int size_now = 0;
    int usec_now = 0;

    char pub_title[SOS_DEFAULT_STRING_LEN] = {0};
    char elem_name[SOS_DEFAULT_STRING_LEN] = {0};

    SOS_val some_value;
    some_value.d_val = 0.0;
    
    for (usec_now = DMIN; usec_now <= DMAX; usec_now += DSTEP) {
      for (iter_now = IMIN; iter_now <= IMAX; iter_now += ISTEP) {
        for (size_now = SMIN; size_now <= SMAX; size_now += SSTEP) {

          snprintf(pub_title, SOS_DEFAULT_STRING_LEN, "%diter x %dvals @ %dusec", 
                   iter_now, size_now, usec_now);
          zlog("Running: %s\n", pub_title);

          pub = SOS_pub_create(my_sos, pub_title, SOS_NATURE_CREATE_OUTPUT);

          int iter_walk = 0;
          int elem_walk = 0;
          for (iter_walk = 0; iter_walk < iter_now; iter_walk++) {
            for (elem_walk = 0; elem_walk < size_now; elem_walk++) {
              snprintf(elem_name, SOS_DEFAULT_STRING_LEN, "elem_%05d", elem_walk);
              SOS_pack(pub, elem_name, SOS_VAL_TYPE_DOUBLE, (void *) &some_value);
              some_value.d_val += 0.0001;
            } //elem_walk
            SOS_publish(pub);
            usleep(usec_now);
          } //iter_walk

          SOS_pub_destroy(pub);
          SWEEP_wait_for_empty_queue(my_sos);
          MPI_Barrier(MPI_COMM_WORLD);

        } //size          
      } //iter
    } //usec

    zlog("Done.\n");

    SOS_finalize(my_sos);
    MPI_Finalize();

    return (EXIT_SUCCESS);
}


void SWEEP_wait_for_empty_queue(SOS_runtime *my_sos) {
    SOS_SET_CONTEXT(my_sos, "SWEEP_wait_for_empty_queue");

    SOS_buffer *request;
    SOS_buffer *reply;
    SOS_buffer_init_sized_locking(my_sos, &request, SOS_DEFAULT_BUFFER_MAX, false);
    SOS_buffer_init_sized_locking(my_sos, &reply, SOS_DEFAULT_BUFFER_MAX, false);

    SOS_msg_header header;
    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_PROBE;
    header.msg_from = my_sos->my_guid;
    header.pub_guid = 0;

    int offset = 0;
    SOS_buffer_pack(request, &offset, "iigg",
                    header.msg_size,
                    header.msg_type,
                    header.msg_from,
                    header.pub_guid);
    header.msg_size = offset;
    offset = 0;

    SOS_buffer_pack(request, &offset, "i", header.msg_size);

    int waited_count = 0;

    bool STUFF_IN_DB_QUEUE = true;
    while (STUFF_IN_DB_QUEUE == true) {

        SOS_buffer_wipe(reply);

        SOS_send_to_daemon(request, reply);

        if (reply->len < sizeof(SOS_msg_header)) {
            fprintf(stderr, "ERROR: Received short (useless) message from daemon!   (reply->len == %d\n", reply->len);
            continue;
        }

        offset = 0;
        SOS_buffer_unpack(reply, &offset, "iigg",
                          &header.msg_size,
                          &header.msg_type,
                          &header.msg_from,
                          &header.pub_guid);

        uint64_t queue_depth_local       = 0;
        uint64_t queue_depth_cloud       = 0;
        uint64_t queue_depth_db_tasks    = 0;
        uint64_t queue_depth_db_snaps    = 0;

        SOS_buffer_unpack(reply, &offset, "gggg",
                          &queue_depth_local,
                          &queue_depth_cloud,
                          &queue_depth_db_tasks,
                          &queue_depth_db_snaps);

        if ((queue_depth_db_tasks    == 0)
            && (queue_depth_db_snaps == 0)
            && (queue_depth_local    == 0)
            && (queue_depth_cloud    == 0)) {
          STUFF_IN_DB_QUEUE = false;
          if (waited_count > 0) { printf("\t\t...[%04d @ %s] Queue is now EMPTY!  Pausing for network flush.  (%1.4F sec)\n",
                                         rank, host, (float) (waited_count / 10)); fflush(stdout); }
          usleep(waited_count * 100000);
          continue;
        } else {
          waited_count++;
          if (waited_count > 25) {
            printf("\t\t...[%04d @ %s] Carrying on anyway!\n", rank, host);
            fflush(stdout);
            STUFF_IN_DB_QUEUE = false;
            continue;
          }
          printf("\t[%04d @ %s] @ (%d of 25) Wait for queue drain ... local: %" SOS_GUID_FMT
                 " cloud: %" SOS_GUID_FMT
                 " tasks: %" SOS_GUID_FMT
                 " snaps: %" SOS_GUID_FMT"\n",
                 rank, host, waited_count, queue_depth_local, queue_depth_cloud, queue_depth_db_tasks, queue_depth_db_snaps);
          fflush(stdout);
          usleep(200000);
        }
    } //while

    SOS_buffer_destroy(request);
    SOS_buffer_destroy(reply);

    return;
}
