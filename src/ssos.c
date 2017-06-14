

/*
 * ssos.c               "Simple SOS" common case SOSflow API wrapper,
 *                      useful for ports to additional languages. 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include "sos.h"
#include "sosa.h"
#include "sos_types.h"
#include "ssos.h"

int                 g_sos_is_online = 0;
int                 g_results_are_ready = 0;
SSOS_query_results *g_results = NULL;
SOS_runtime        *g_sos = NULL;
SOS_pub            *g_pub = NULL;


#define SSOS_CONFIRM_ONLINE(__where)                        \
{                                                           \
    if (g_sos_is_online == 0) {                             \
        fprintf(stderr, "SSOS (PID:%d) -- %s called, but "  \
            "SSOS is not yet online.  Doing nothing.\n",    \
            getpid(), __where);                             \
        return;                                             \
    };                                                      \
};

void SSOS_feedback_handler(int payload_type,
        int payload_size, void *payload_data);


void
SSOS_sense_trigger(
        char  *sense_handle,
        int    payload_size,
        void  *payload_data)
{
    SSOS_CONFIRM_ONLINE("SSOS_sense_trigger");
    SOS_SET_CONTEXT(g_sos, "SSOS_sense_trigger");

    SOS_sense_trigger(g_sos, sense_handle,
            payload_data, payload_size);
    
    return;
}


void SSOS_set_option(int option_key, char *option_value) {
    SSOS_CONFIRM_ONLINE("SSOS_set_option");
    SOS_SET_CONTEXT(g_sos, "SSOS_set_option");

    switch (option_key) {

        case SSOS_OPT_PROG_VERSION:
            strncpy(g_pub->prog_ver, option_value, SOS_DEFAULT_STRING_LEN);
            break;

        case SSOS_OPT_COMM_RANK:
            g_pub->comm_rank = atoi(option_value);
            g_sos->config.comm_rank = g_pub->comm_rank;
            break;


        default:
            fprintf(stderr, "SSOS (PID:%d) -- Invalid option_key (%d) used to set"
                    " option (%s).\n", getpid(), option_key, option_value);
            fflush(stderr);
            break;
    }

    return;
}


void
SSOS_query_exec(char *sql, SSOS_query_results *results,
       char *target_host, int target_port)
{
    SSOS_CONFIRM_ONLINE("SSOS_query_exec");
    SOS_SET_CONTEXT(g_sos, "SSOS_query_exec");

    SOSA_results_init(g_sos, (SOSA_results **) &results);
    g_results = results;
    g_results_are_ready = 0;
    
    SOSA_exec_query(g_sos, sql, target_host, target_port);

    fflush(stdout);

    return;
}

void SSOS_is_query_done(int *addr_of_YN_int_flag) {
    *addr_of_YN_int_flag = g_results_are_ready;
    return;
}



void
SSOS_query_exec_blocking(char *sql, SSOS_query_results *results,
       char *target_host, int target_port) {
    SSOS_CONFIRM_ONLINE("SSOS_query_exec_blocking");
    SOS_SET_CONTEXT(g_sos, "SSOS_query_exec_blocking");
    SSOS_query_exec(sql, results, target_host, target_port);
    while (g_results_are_ready != 1) {
        usleep(10000);
    }
    return;
}


void SSOS_results_destroy(SSOS_query_results *results) {
    SSOS_CONFIRM_ONLINE("SSOS_results_destroy");
    SOS_SET_CONTEXT(g_sos, "SSOS_results_destroy");

    int row = 0;
    int col = 0;
    for (row = 0; row < results->row_count; row++) {
        for (col = 0; col < results->col_count; col++) {
            free(results->data[row][col]);
        }
    }

    for (row = 0; row < results->row_max; row++) {
        free(results->data[row]);
    }

    free(results->data);

    for (col = 0; col < results->col_max; col++) {
        if (results->col_names[col] != NULL) {
            free(results->col_names[col]);
        }
    }
    free(results->col_names);
    return;
}

void SSOS_feedback_handler(
        int   payload_type,
        int   payload_size,
        void *payload_data)
{
    SOS_SET_CONTEXT(g_sos, "SSOS_feedback_handler");
    if (payload_type != SOS_FEEDBACK_TYPE_QUERY) {
        fprintf(stderr, "SSOS (PID:%d) --\n"
                "\tFeedback was received by the SSOS library that\n"
                "\twas marked (%d) as non-query results. Only SQL\n"
                "\tresults are supported in the current version of\n"
                "\tSSOS. For more options, please see the features\n"
                "\tprovided by the full SOS API.\n",
                getpid(), payload_type);
        fflush(stderr);
        g_results_are_ready = 0;
        return;
    }
    SOSA_results_from_buffer((SOSA_results *)g_results, payload_data);
    g_results_are_ready = 1;
 
    return;
}


void SSOS_init(char *prog_name) {
    g_sos_is_online = 0;
    g_results_are_ready = 0;
    int attempt = 0;

    g_sos = NULL;
    while (g_sos == NULL) {

        SOS_init(NULL, NULL,
            &g_sos,
            SOS_ROLE_CLIENT,
            SOS_RECEIVES_DIRECT_MESSAGES,
            SSOS_feedback_handler);

        if (g_sos == NULL) {
            attempt += 1;
            fprintf(stderr, "SSOS (PID:%d) -- Failed to connect to daemon."
                " (#: %d)\n", getpid(), attempt);
            sleep(SSOS_ATTEMPT_DELAY);
        }

        if (attempt > SSOS_ATTEMPT_MAX) {
            fprintf(stderr, "SSOS (PID:%d) -- Maximum attempts reached."
                " Giving up.\n", getpid());
            return;
        }
    }

    g_pub = NULL;
    SOS_pub_create(g_sos, &g_pub, "ssos.client", SOS_NATURE_DEFAULT);

    if (g_pub == NULL) {
        fprintf(stderr, "SSOS (PID:%d) -- Failed to create pub handle.\n",
            getpid());
    } else {
        strncpy(g_pub->prog_name, prog_name, SOS_DEFAULT_STRING_LEN);
        g_sos_is_online = 1;
    }

    return;
}

void SSOS_is_online(int *addr_of_flag) {
    *addr_of_flag = g_sos_is_online;
    return;
}



void SSOS_pack(char *name, int pack_type, void *pack_val) {
    SSOS_CONFIRM_ONLINE("SSOS_pack");
    SOS_SET_CONTEXT(g_sos, "SSOS_pack");

    SOS_val_type is_a;
    switch(pack_type) {
    case SSOS_TYPE_INT:    is_a = SOS_VAL_TYPE_INT;    break;
    case SSOS_TYPE_LONG:   is_a = SOS_VAL_TYPE_LONG;   break;
    case SSOS_TYPE_DOUBLE: is_a = SOS_VAL_TYPE_DOUBLE; break;
    case SSOS_TYPE_STRING: is_a = SOS_VAL_TYPE_STRING; break;
    default:
        fprintf(stderr, "SSOS (PID:%d) -- Invalid pack_type for SSOS_pack:"
            " %d   (Aborting pack)\n", getpid(), pack_type);
        return;
    }
    SOS_pack(g_pub, name, is_a, pack_val);
    return;
}

void SSOS_announce(void) {
    SSOS_CONFIRM_ONLINE("SSOS_announce");
    SOS_announce(g_pub);
    return;
}

void SSOS_publish(void) {
    SSOS_CONFIRM_ONLINE("SSOS_publish");
    SOS_publish(g_pub);
    return;
}

void SSOS_finalize(void) {
    SSOS_CONFIRM_ONLINE("SSOS_finalize");
    g_sos_is_online = 0;
    SOS_finalize(g_sos);
    return;
}


