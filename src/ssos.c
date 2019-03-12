

/*
 * ssos.c               "Simple SOS" common case SOSflow API wrapper,
 *                      useful for ports to additional languages. 
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <pthread.h>
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#else
#include <semaphore.h>
#endif

#include "sos.h"
#include "sosa.h"
#include "sos_types.h"
#include "ssos.h"

typedef struct {
    SOS_buffer  *buffer;
    void        *next;
} SSOS_result_pool_entry;

int                      g_sos_is_online = 0;

pthread_mutex_t         *g_result_pool_lock;
int                      g_result_pool_size;
SSOS_result_pool_entry  *g_result_pool_head;

SOS_runtime             *g_sos = NULL;
SOS_pub                 *g_pub = NULL;

#ifdef __APPLE__
dispatch_semaphore_t    g_results_ready;
#else
sem_t                   g_results_ready;
#endif

#define SSOS_CONFIRM_ONLINE(__where)                        \
{                                                           \
    if (g_sos_is_online == 0) {                             \
        fprintf(stderr, "SSOS (PID:%d) -- %s called, but "  \
            "SSOS is not yet online.  Doing nothing.\n",    \
            getpid(), __where);                             \
        return;                                             \
    };                                                      \
};


void
SSOS_feedback_handler(
        void       *sos_context,
        int         payload_type,
        int         payload_size,
        void       *payload_data)
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
        return;
    }

    //printf("SSOS_feedback_handler:\n"
    //       "    payload_type = %d\n"
    //       "    payload_size = %d\n"
    //       "    payload_data = %p   (pointer)\n",
    //       payload_type, payload_size, payload_data);
    //fflush(stdout);

    //What was passed in is a SOS_buffer*, not buf->data pointer.
    SOS_buffer *incoming_buffer = (SOS_buffer *) payload_data;

    //Create an entry for the result pool.
    SSOS_result_pool_entry *entry =
        (SSOS_result_pool_entry *) calloc(1, sizeof(SSOS_result_pool_entry));
    entry->buffer = NULL;
    SOS_buffer_init_sized_locking(g_sos, &entry->buffer, payload_size + 1, false);
    memcpy(entry->buffer->data, incoming_buffer->data, payload_size);
    entry->buffer->len = payload_size;

    //Add the entry to the result pool. 
    pthread_mutex_lock(g_result_pool_lock);
    entry->next = g_result_pool_head;
    g_result_pool_head = entry;
    g_result_pool_size++;

    //printf( "Adding result #%d of length %d to the result pool...\n",
    //        g_result_pool_size, entry->buffer->len);

    pthread_mutex_unlock(g_result_pool_lock);
#ifdef __APPLE__
    dispatch_semaphore_signal(g_results_ready);
#else
    sem_post(&g_results_ready);
#endif

    //Done.  (Results are claimed with SSOS_result_claim() function.)

    return;
}


void
SSOS_result_pool_size(
    int            *addr_of_counter_int)
{
    *addr_of_counter_int = g_result_pool_size;
    return;
}



void
SSOS_result_claim(
    SSOS_query_results     *results)
{
    SSOS_CONFIRM_ONLINE("SSOS_query_claim_results");
    SOS_SET_CONTEXT(g_sos, "SSOS_query_claim_results");

    //This function 'soft-blocks' until results are available.

    while (g_sos_is_online) {
#ifdef __APPLE__
        dispatch_semaphore_wait(g_results_ready, DISPATCH_TIME_FOREVER);
#else
        sem_wait(&g_results_ready);
#endif
        //Grab the lock
        pthread_mutex_lock(g_result_pool_lock);
        //Check to make sure that our test for results
        //existing is still valid, to avoid race between
        //the test and having grabbed the lock...
        //...this allows good behavior AND correctness.
        if (g_result_pool_size < 1) {
            pthread_mutex_unlock(g_result_pool_lock);
            continue;
        }

        // printf( "Processing the results...\n");

        //If we're here, we hold the lock AND there are results.
        //Grab the head of the result pool and release the lock:
        SSOS_result_pool_entry *entry = g_result_pool_head;
        g_result_pool_head = entry->next;
        g_result_pool_size--;
        pthread_mutex_unlock(g_result_pool_lock);

        //The pool is now open for other threads and we can
        //process this entry.
        // printf( "Initializing the results object...\n");
        SOSA_results_init(g_sos, (SOSA_results **) &results);

        // printf( "Building results from buffer...\n");
        SOSA_results_from_buffer((SOSA_results *) results, entry->buffer);

        // printf( "Destroying the buffer object...\n");
        SOS_buffer_destroy(entry->buffer);

        // printf( "Free'ing the entry...\n");
        free(entry);

        //Leave the loop and return to the client.
        break;
    }

    return;
}

void
SSOS_result_destroy(
        SSOS_query_results     *results)
{
    SSOS_CONFIRM_ONLINE("SSOS_results_destroy");
    SOS_SET_CONTEXT(g_sos, "SSOS_results_destroy");

    uint32_t row = 0;
    uint32_t col = 0;
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

void
SSOS_get_guid(void *addr_of_uint64)
{
    SSOS_CONFIRM_ONLINE("SSOS_get_guid");
    SOS_SET_CONTEXT(g_sos, "SSOS_get_guid");
    uint64_t *place = addr_of_uint64;
    *place = SOS_uid_next(g_sos->uid.my_guid_pool);
    return;
}


void
SSOS_request_pub_manifest(
        SSOS_query_results     *manifest_var,
        int                    *max_frame_overall_var,
        const char             *pub_title_filter,
        const char             *target_host,
        int                     target_port)
{
    SSOS_CONFIRM_ONLINE("SSOS_request_pub_manifest");
    SOS_SET_CONTEXT(g_sos, "SSOS_request_pub_manifest");

    SOSA_results_init(g_sos, (SOSA_results **) &manifest_var);

    SOSA_request_pub_manifest(
            g_sos,
            (SOSA_results *) manifest_var,
            max_frame_overall_var,
            pub_title_filter,
            target_host,
            target_port);

    return;
}


void
SSOS_cache_grab(
        const char             *pub_filter,
        const char             *val_filter,
        int                     frame_head,
        int                     frame_depth_limit,
        const char             *target_host,
        int                     target_port)
{
    SSOS_CONFIRM_ONLINE("SSOS_cache_grab");
    SOS_SET_CONTEXT(g_sos, "SSOS_cache_grab");

    SOSA_cache_grab(g_sos,
            pub_filter, val_filter,
            frame_head, frame_depth_limit,
            target_host, target_port);

    return;
}


void
SSOS_query_exec(
        const char     *sql,
        const char     *target_host,
        int             target_port)
{
    SSOS_CONFIRM_ONLINE("SSOS_query_exec");
    SOS_SET_CONTEXT(g_sos, "SSOS_query_exec");

    //How this works NOW, supporting parallel submission of queries:
    //  1. Submit the query and return
    //  2. Results come back in and go into the pool.
    //  3. Client/lib wrapper claims them w/indeterminate order.

    //What's next...
    //TODO: Keep track of all the GUIDs assigned to submitted queries
    //      and allow the client/lib wrapper to wait to claim a specific
    //      query ID. Note that this could cause concerns with
    //      SOME results being claimed under the "give me whatever comes
    //      in first" function call mixed in with some thread asking
    //      for some specific one, where the one asking for anything
    //      claims a specific ID that someone else is requesting.
    //TODO: The request/reply pool is process-specific, so as long as
    //pt.2  the client uses one OR the other type of function call
    //      things are going to be fine. This should be tracked with
    //      a flag set depending on which kind of call is made first,
    //      with a loud warning displayed if type B is called after
    //      type A, etc.
    //TODO: Since we'd be tracking all outstanding queries, we should
    //pt.3  have an API call to return that manifest, including time
    //      of submission and whether it was sent on or off node,
    //      to an aggregator or a listener, etc. This could be used
    //      later on to facilitate automatic distributed query
    //      throughput/balancing. We could track a rolling average of
    //      delay per destination.

    //Send the query to the daemon:
    int rc = SOSA_exec_query(g_sos, sql, target_host, target_port);
    if (rc < 0 && g_sos_is_online) {
        // bad news.
        fprintf(stderr, "Error: the connection to the daemon has dropped. Exiting.\n");
        exit(-1);
    }

    return;
}



void
SSOS_init(
        const char     *prog_name)
{
    g_sos_is_online = 0;

#ifdef __APPLE__
    g_results_ready = dispatch_semaphore_create(0);
#else
    sem_init(&g_results_ready, 0, 1);
#endif

    g_sos = NULL;
    while (g_sos == NULL) {

        SOS_init(&g_sos,
            SOS_ROLE_CLIENT,
            SOS_RECEIVES_DIRECT_MESSAGES,
            SSOS_feedback_handler);

        if (g_sos == NULL) {
            fprintf(stderr, "SSOS (PID:%d) -- Failed to connect to daemon.",
                    getpid());
            return;
        }
    }

    g_result_pool_lock = (pthread_mutex_t *) calloc(1, sizeof(pthread_mutex_t));
    g_result_pool_size = 0;
    g_result_pool_head = NULL;

    pthread_mutex_init(g_result_pool_lock, NULL); 

    g_pub = NULL;
    SOS_pub_init(g_sos, &g_pub, "ssos.client", SOS_NATURE_DEFAULT);

    if (g_pub == NULL) {
        fprintf(stderr, "SSOS (PID:%d) -- Failed to create pub handle.\n",
            getpid());
    } else {
        strncpy(g_pub->prog_name, prog_name, SOS_DEFAULT_STRING_LEN);
        g_sos_is_online = 1;
    }

    return;
}

void
SSOS_is_online(
        int         *addr_of_flag)
{
    *addr_of_flag = g_sos_is_online;
    return;
}



void
SSOS_pack(
        const char     *name,
        int             pack_type,
        void           *pack_val)
{
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

void
SSOS_announce(void)
{
    SSOS_CONFIRM_ONLINE("SSOS_announce");
    SOS_announce(g_pub);
    return;
}

void
SSOS_publish(void)
{
    SSOS_CONFIRM_ONLINE("SSOS_publish");
    SOS_publish(g_pub);
    return;
}

void
SSOS_finalize(void)
{
    SSOS_CONFIRM_ONLINE("SSOS_finalize");
    g_sos_is_online = 0;

    //Drain the result pool:
    pthread_mutex_lock(g_result_pool_lock);
    g_result_pool_size = 0;
    SSOS_result_pool_entry *entry = g_result_pool_head;
    SSOS_result_pool_entry *next_entry = NULL;
    g_result_pool_head = NULL;
    while(entry != NULL) {
        SOS_buffer_destroy(entry->buffer);
        next_entry = entry->next;
        free(entry);
        entry = next_entry;
    }
#ifdef __APPLE__
#else
    sem_destroy(&g_results_ready);
#endif
    pthread_mutex_destroy(g_result_pool_lock);

    SOS_finalize(g_sos);
    return;
}

void
SSOS_sense_trigger(
    const char         *sense_handle,
    int                 payload_size,
    void               *payload_data)
{
    SSOS_CONFIRM_ONLINE("SSOS_sense_trigger");
    SOS_SET_CONTEXT(g_sos, "SSOS_sense_trigger");

    SOS_sense_trigger(g_sos, sense_handle,
            payload_data, payload_size);
    
    return;
}


void
SSOS_set_option(
    int                 option_key,
    const char         *option_value)
{
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

