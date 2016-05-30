/*
 *   sosa.c   Library functions for writing SOS analytics modules.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#include <mpi.h>

#include "sos.h"
#include "sosd.h"
#include "sosa.h"
#include "sos_types.h"
#include "sos_debug.h"

SOSA_runtime SOSA;


// NOTE: Only call this when you already hold the uid->lock
void SOSA_guid_request(SOS_uid *uid) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_guid_request");

    dlog(7, "Obtaining new guid range...\n");

    SOS_buffer *msg;
    SOS_buffer *reply;
    SOS_buffer_init_sized_locking(SOS, &msg,   256, false);
    SOS_buffer_init_sized_locking(SOS, &reply, 256, false);

    SOS_msg_header header;
    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_GUID_BLOCK;
    header.msg_from = SOSA.world_rank;
    header.pub_guid = 0;

    int offset = 0;
    SOS_buffer_pack(msg, &offset, "iigg",
                    header.msg_size,
                    header.msg_type,
                    header.msg_from,
                    header.pub_guid);

    header.msg_size = offset;
    offset = 0;
    SOS_buffer_pack(msg, &offset, "i",
                    header.msg_size);

    SOSA_send_to_target_db(msg, reply);

    if (reply->len < (2 * sizeof(double))) {
        dlog(0, "WARNING: Malformed UID reply from sosd (db) ...\n");
        uid->next = -1;
        uid->last = 0;
    } else {
        offset = 0;
        SOS_buffer_unpack(reply, &offset, "gg",
                          &uid->next,
                          &uid->last);
    }

    dlog(7, "    ... %" SOS_GUID_FMT " -> %" SOS_GUID_FMT " assigned.  Done.\n", uid->next, uid->last);

    SOS_buffer_destroy(msg);
    SOS_buffer_destroy(reply);

    return;
}




void SOSA_exec_query(char *query, SOSA_results *result) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_exec_query");

    return;
}



void SOSA_results_init(SOS_runtime *sos_context, SOSA_results **results_object_ptraddr) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_results_init");

    return;
}


void SOSA_results_to_buffer(SOS_buffer *buffer, SOSA_results *results) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_results_to_buffer");

    return;
}



void SOSA_results_from_buffer(SOSA_results *results, SOS_buffer *buffer) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_results_from_buffer");

    return;
}




void SOSA_results_output_to(FILE *file, SOSA_results *results, int options) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_results_output_to");

    return;
}


void SOSA_results_wipe(SOSA_results *results_object) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_results_wipe");

    return;
}


void SOSA_results_destroy(SOSA_results *results_object) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_results_destroy");

    return;
}





void SOSA_finalize(void) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_finalize");    

    free(SOSA.analytics_locales);
    free(SOSA.world_roles);
    free(SOSA.world_hosts);

    return;
}



void SOSA_send_to_target_db(SOS_buffer *msg, SOS_buffer *reply) {
    SOS_SET_CONTEXT(SOSA.sos_context, "SOSA_send_to_target_db");

    if ((msg == NULL) || (reply == NULL)) {
        dlog(0, "ERROR: Buffer pointer supplied with NULL value!\n");
        exit(EXIT_FAILURE);
    }

    SOS_buffer *wrapper;
    SOS_buffer_init_sized_locking(SOS, &wrapper, (1 + msg->len + sizeof(int)), false);

    // All messages to DB ranks come with a wrapper that supports multiple-message
    // packing. We treat this function as a 1:1::call:message packager, so just
    // pack a 1 in it and call it good.
    int offset = 0;
    SOS_buffer_pack(wrapper, &offset, "i", 1);

    // Copy the message memory directly into the wrapper's data area:
    memcpy((wrapper->data + offset), msg->data, msg->len);
    wrapper->len += msg->len;

    dlog(7, "Sending message of %d bytes...\n", wrapper->len);
    MPI_Ssend((void *) wrapper->data, wrapper->len, MPI_CHAR, SOSA.world_db_target_rank, 0, MPI_COMM_WORLD);


    dlog(7, "Waiting for a reply...\n");
    MPI_Status status;
    int msg_waiting = 0;
    do {
        MPI_Iprobe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &msg_waiting, &status);
        usleep(1000);
    } while (msg_waiting == 0);

    int mpi_reply_len = -1;
    MPI_Get_count(&status, MPI_CHAR, &mpi_reply_len);

    while(reply->max < mpi_reply_len) {
        SOS_buffer_grow(reply, (1 + (mpi_reply_len - reply->max)), SOS_WHOAMI);
    }

    MPI_Recv((void *) reply->data, mpi_reply_len, MPI_CHAR, status.MPI_SOURCE, status.MPI_TAG, MPI_COMM_WORLD, &status);
    reply->len = mpi_reply_len;
    dlog(7, "  ... reply of %d bytes received from rank %d!\n", mpi_reply_len, status.MPI_SOURCE);
    dlog(7, "Done.\n");

    return;
}


SOS_runtime* SOSA_init(int *argc, char ***argv, int unique_color) {
    SOSA.sos_context = (SOS_runtime *) malloc(sizeof(SOS_runtime));

    SOSA.sos_context->role = SOS_ROLE_ANALYTICS;
    SOSA.sos_context->status = SOS_STATUS_RUNNING;
    SOSA.sos_context->config.argc = *argc;
    SOSA.sos_context->config.argv = *argv;

    MPI_Init_thread(argc, argv, MPI_THREAD_MULTIPLE, &SOSA.sos_context->config.comm_support);

    int world_size = -1;
    int world_rank = -1;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // Information about this rank.
    int   my_role;
    char *my_host;
    int   my_host_name_len;

    // Target arrays for MPI_Allgather of roles and host names.
    int  *world_roles;
    char *world_hosts;

    // WORLD DISCOVER: ----------
    //   (includes ANALYTICS ranks)
    my_host     = (char *) calloc(MPI_MAX_PROCESSOR_NAME, sizeof(char));
    world_hosts = (char *) calloc(world_size * (MPI_MAX_PROCESSOR_NAME), sizeof(char));
    world_roles =  (int *) calloc(world_size, sizeof(int));
    my_role = SOSA.sos_context->role;
    MPI_Get_processor_name(my_host, &my_host_name_len);
    MPI_Allgather((void *) &my_role, 1, MPI_INT, world_roles, 1, MPI_INT, MPI_COMM_WORLD);
    MPI_Allgather((void *) my_host, MPI_MAX_PROCESSOR_NAME, MPI_CHAR,
                  (void *) world_hosts, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, MPI_COMM_WORLD);

    SOSA.world_rank  = world_rank;
    SOSA.world_size  = world_size;
    SOSA.world_roles = world_roles;
    SOSA.world_hosts = world_hosts;

    // SPLIT: -------------------
    //   (ANALYTICS ranks peel off into their own communicator)
    MPI_Comm_split(MPI_COMM_WORLD, (unique_color + SOS_ROLE_ANALYTICS), world_rank, &SOSA.comm);
    MPI_Comm_size(SOSA.comm, &SOSA.sos_context->config.comm_size);
    MPI_Comm_rank(SOSA.comm, &SOSA.sos_context->config.comm_rank);

    SOSA.world_db_target_rank       = -1;
    SOSA.sos_context->config.locale = -1;

    // See if we're aligned with a database rank:
    int i;
    for (i = 0; i < world_size; i++) {
        if (world_roles[i] == SOS_ROLE_DB) {
            if (strncmp(my_host, (world_hosts + (i * MPI_MAX_PROCESSOR_NAME)), MPI_MAX_PROCESSOR_NAME) == 0) {
                // We're on the same node as this database...
                SOSA.world_db_target_rank = i;
                break;
            }
        }
    }

    if (SOSA.world_db_target_rank == -1) {
        SOSA.sos_context->config.locale = SOS_LOCALE_INDEPENDENT;
        printf("[ANALYTICS(%d)]: Independent.\n", SOSA.sos_context->config.comm_rank);
        
    } else {
        SOSA.sos_context->config.locale = SOS_LOCALE_DAEMON_DBMS;
        printf("[ANALYTICS(%d)]: Co-located with SOS_ROLE_DB at MPI_COMM_WORLD rank %d on host %s.\n",
               SOSA.sos_context->config.comm_rank, SOSA.world_db_target_rank, my_host);
    }

    // ANALYTICS discover: ------
    SOSA.analytics_locales = (int *) calloc(SOSA.sos_context->config.comm_size, sizeof(int));
    MPI_Allgather((void *) &SOSA.sos_context->config.locale, 1, MPI_INT,
                  (void *) SOSA.analytics_locales, 1, MPI_INT, SOSA.comm);
    
    return SOSA.sos_context;
}




