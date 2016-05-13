
/*
 * sosd_probe.c
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <mpi.h>

#define USAGE "./sosd_probe -d <loop_delay_seconds> [-o json]"

#define OUTPUT_CSV   1
#define OUTPUT_JSON  2

#include "sos.h"
#include "sosd.h"

int GLOBAL_sleep_delay;
int GLOBAL_output_type;

int main(int argc, char *argv[]) {
    int   i;
    int   elem;
    int   next_elem;

    MPI_Init(&argc, &argv);

    /* Process command-line arguments */
    if ( argc < 3 )  { fprintf(stderr, "%s\n", USAGE); exit(1); }

    GLOBAL_sleep_delay = -1;
    GLOBAL_output_type = OUTPUT_CSV;

    for (elem = 1; elem < argc; ) {
        if ((next_elem = elem + 1) == argc) {
            fprintf(stderr, "%s\n", USAGE);
            exit(1);
        }
        if ( strcmp(argv[elem], "-d"  ) == 0) {
            GLOBAL_sleep_delay  = atoi(argv[next_elem]);
        } else if ( strcmp(argv[elem], "-o"  ) == 0) {
            if ( strcmp(argv[next_elem], "json" ) == 0) {
                GLOBAL_output_type = OUTPUT_JSON;
            } else {
                printf("WARNING: Unknown output type specified.  Defaulting to CSV.   (%s)\n", argv[next_elem]);
                GLOBAL_output_type = OUTPUT_CSV;
            }
        } else {
            fprintf(stderr, "Unknown flag: %s %s\n", argv[elem], argv[next_elem]);
        }
        elem = next_elem + 1;
    }

    if (GLOBAL_sleep_delay < 1) { fprintf(stderr, "%s\n", USAGE); exit(1); }

    SOS_runtime *my_sos;
    my_sos = SOS_init( &argc, &argv, SOS_ROLE_RUNTIME_UTILITY, SOS_LAYER_ENVIRONMENT);
    srandom(my_sos->my_guid);

    SOS_buffer *request;
    SOS_buffer *reply;
    SOS_buffer_init_sized_locking(my_sos, &request, SOS_DEFAULT_BUFFER_LEN, false);
    SOS_buffer_init_sized_locking(my_sos, &reply, SOS_DEFAULT_BUFFER_LEN, false);

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


    while (getenv("SOS_SHUTDOWN") == NULL) {
        sleep(GLOBAL_sleep_delay);
        SOS_buffer_wipe(reply);
        
        SOS_send_to_daemon(request, reply);

        if (reply->len < sizeof(SOS_msg_header)) {
            printf("ERROR: Received short (useless) message from daemon!   (reply->len == %d\n", reply->len);
            continue;
        }

        offset = 0;
        SOS_buffer_unpack(reply, &offset, "iigg",
                          &header.msg_size,
                          &header.msg_type,
                          &header.msg_from,
                          &header.pub_guid);

        uint64_t queue_depth_local = 0;
        uint64_t queue_depth_cloud = 0;
        uint64_t queue_depth_db    = 0;

        SOS_buffer_unpack(reply, &offset, "ggg",
                          &queue_depth_local,
                          &queue_depth_cloud,
                          &queue_depth_db);

        SOSD_counts current;
        SOS_buffer_unpack(reply, &offset, "ggggggggggggggggggggg",
                          &current.thread_local_wakeup,
                          &current.thread_cloud_wakeup,
                          &current.thread_db_wakeup,
                          &current.feedback_checkin_messages,
                          &current.socket_messages,
                          &current.socket_bytes_recv,
                          &current.socket_bytes_sent,
                          &current.mpi_sends,
                          &current.mpi_bytes,
                          &current.db_transactions,
                          &current.db_insert_announce,
                          &current.db_insert_announce_nop,
                          &current.db_insert_publish,
                          &current.db_insert_publish_nop,
                          &current.db_insert_val_snaps,
                          &current.db_insert_val_snaps_nop,
                          &current.buffer_creates,
                          &current.buffer_bytes_on_heap,
                          &current.buffer_destroys,
                          &current.pipe_creates,
                          &current.pub_handles);

        double time_now = 0.0;
        SOS_TIME(time_now);
        
        switch(GLOBAL_output_type) {
        case OUTPUT_CSV:    //--------------------------------------------------
            printf("%lf,"
                   "%" SOS_GUID_FMT ",%" SOS_GUID_FMT ",%" SOS_GUID_FMT ","
                   "%" SOS_GUID_FMT ",%" SOS_GUID_FMT ",%" SOS_GUID_FMT ","
                   "%" SOS_GUID_FMT ",%" SOS_GUID_FMT ",%" SOS_GUID_FMT ","
                   "%" SOS_GUID_FMT ",%" SOS_GUID_FMT ",%" SOS_GUID_FMT ","
                   "%" SOS_GUID_FMT ",%" SOS_GUID_FMT ",%" SOS_GUID_FMT ","
                   "%" SOS_GUID_FMT ",%" SOS_GUID_FMT ",%" SOS_GUID_FMT ","
                   "%" SOS_GUID_FMT ",%" SOS_GUID_FMT ",%" SOS_GUID_FMT ","
                   "%" SOS_GUID_FMT ",%" SOS_GUID_FMT ",%" SOS_GUID_FMT "\n",
                   time_now,
                   queue_depth_local,
                   queue_depth_cloud,
                   queue_depth_db,
                   current.thread_local_wakeup,
                   current.thread_cloud_wakeup,
                   current.thread_db_wakeup,
                   current.feedback_checkin_messages,
                   current.socket_messages,
                   current.socket_bytes_recv,
                   current.socket_bytes_sent,
                   current.mpi_sends,
                   current.mpi_bytes,
                   current.db_transactions,
                   current.db_insert_announce,
                   current.db_insert_announce_nop,
                   current.db_insert_publish,
                   current.db_insert_publish_nop,
                   current.db_insert_val_snaps,
                   current.db_insert_val_snaps_nop,
                   current.buffer_creates,
                   current.buffer_bytes_on_heap,
                   current.buffer_destroys,
                   current.pipe_creates,
                   current.pub_handles);
            break;



        case OUTPUT_JSON:  //--------------------------------------------------

            printf("{\"sosd_probe\": {\n");
            printf("\t\"time_stamp\": \"%lf\",\n", time_now);
            printf("\t\"queue_depth_local\": \"%"         SOS_GUID_FMT "\",\n", queue_depth_local);
            printf("\t\"queue_depth_cloud\": \"%"         SOS_GUID_FMT "\",\n", queue_depth_cloud);
            printf("\t\"queue_depth_db\": \"%"            SOS_GUID_FMT "\",\n", queue_depth_db);
            printf("\t\"thread_local_wakeup\": \"%"       SOS_GUID_FMT "\",\n", current.thread_local_wakeup);
            printf("\t\"thread_cloud_wakeup\": \"%"       SOS_GUID_FMT "\",\n", current.thread_cloud_wakeup);
            printf("\t\"thread_db_wakeup\": \"%"          SOS_GUID_FMT "\",\n", current.thread_db_wakeup);
            printf("\t\"feedback_checkin_messages\": \"%" SOS_GUID_FMT "\",\n", current.feedback_checkin_messages);
            printf("\t\"socket_messages\": \"%"           SOS_GUID_FMT "\",\n", current.socket_messages);
            printf("\t\"socket_bytes_recv\": \"%"         SOS_GUID_FMT "\",\n", current.socket_bytes_recv);
            printf("\t\"socket_bytes_sent\": \"%"         SOS_GUID_FMT "\",\n", current.socket_bytes_sent);
            printf("\t\"mpi_sends\": \"%"                 SOS_GUID_FMT "\",\n", current.mpi_sends);
            printf("\t\"mpi_bytes\": \"%"                 SOS_GUID_FMT "\",\n", current.mpi_bytes);
            printf("\t\"db_transactions\": \"%"           SOS_GUID_FMT "\",\n", current.db_transactions);
            printf("\t\"db_insert_announce\": \"%"        SOS_GUID_FMT "\",\n", current.db_insert_announce);
            printf("\t\"db_insert_announce_nop\": \"%"    SOS_GUID_FMT "\",\n", current.db_insert_announce_nop);
            printf("\t\"db_insert_publish\": \"%"         SOS_GUID_FMT "\",\n", current.db_insert_publish);
            printf("\t\"db_insert_publish_nop\": \"%"     SOS_GUID_FMT "\",\n", current.db_insert_publish_nop);
            printf("\t\"db_insert_val_snaps\": \"%"       SOS_GUID_FMT "\",\n", current.db_insert_val_snaps);
            printf("\t\"db_insert_val_snaps_nop\": \"%"   SOS_GUID_FMT "\",\n", current.db_insert_val_snaps_nop);
            printf("\t\"buffer_creates\": \"%"            SOS_GUID_FMT "\",\n", current.buffer_creates);
            printf("\t\"buffer_bytes_on_heap\": \"%"      SOS_GUID_FMT "\",\n", current.buffer_bytes_on_heap);
            printf("\t\"buffer_destroys\": \"%"           SOS_GUID_FMT "\",\n", current.buffer_destroys);
            printf("\t\"pipe_creates\": \"%"              SOS_GUID_FMT "\",\n", current.pipe_creates);
            printf("\t\"pub_handles\": \"%"               SOS_GUID_FMT "\",\n", current.pub_handles);
            printf("}}\n\n");
            break;

        default:
            printf("ERROR: Invalid GLOBAL_output_type specified.  (%d)\n", GLOBAL_output_type);
            break;
        }


    }//while
    SOS_buffer_destroy(request);
    SOS_buffer_destroy(reply);
    SOS_finalize(my_sos);
    MPI_Finalize();

    return (EXIT_SUCCESS);
}
