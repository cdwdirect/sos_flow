/*
 *   sosa.c   Library functions for writing SOS analytics modules.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#ifdef USE_MPI
#include <mpi.h>
#endif

#include "sos.h"
//#include "sosd.h"
#include "sosa.h"
#include "sos_types.h"
#include "sos_debug.h"

void
SOSA_exec_query(SOS_runtime *sos_context, char *query,
        char *target_host, int target_port)
{
    SOS_SET_CONTEXT(sos_context, "SOSA_exec_query");

    dlog(7, "Submitting query (%25s) ...\n", query);

    SOS_buffer *msg;
    SOS_buffer *reply;
    SOS_buffer_init_sized_locking(SOS, &msg,   1024, false);
    SOS_buffer_init_sized_locking(SOS, &reply, 2048, false);


    SOS_msg_header header;
    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_QUERY;
    header.msg_from = SOS->config.comm_rank;
    header.ref_guid = 0;

    dlog(7, "   ... creating msg.\n");

    int offset = 0;
    SOS_msg_zip(msg, header, 0, &offset);
    
    
    SOS_buffer_pack(msg, &offset, "s", SOS->config.node_id);
    SOS_buffer_pack(msg, &offset, "i", SOS->config.receives_port);
    SOS_buffer_pack(msg, &offset, "s", query);

    header.msg_size = offset;
    offset = 0;
    SOS_msg_zip(msg, header, 0, &offset);

    dlog(7, "   ... sending to daemon.\n");
    SOS_socket *target = NULL;
    SOS_target_init(SOS, &target, target_host, target_port);
    SOS_target_connect(target);
    SOS_target_send_msg(target, msg);
    SOS_target_recv_msg(target, reply);
    SOS_target_disconnect(target);
    SOS_target_destroy(target);
    
    SOS_buffer_destroy(msg);
    SOS_buffer_destroy(reply);

    dlog(7, "   ... done.\n");
    return;
}


void SOSA_results_put(SOSA_results *results, int col, int row, const char *val) {
    SOS_SET_CONTEXT(results->sos_context, "SOSA_results_put");
    
    const char nullstr[] = "NULL";
    const char *strval   = (const char *) val;
    if (strval == NULL) {
        strval = nullstr;
    }

    dlog(9, "put(row,col)== %d, %d\t\t-> result.max(row,col) == %d, %d\n",
        row, col, results->row_max, results->col_max);

    if ((col >= results->col_max) || (row >= results->row_max)) {
        SOSA_results_grow_to(results, col, row);
    }

    if (results->data[row][col] != NULL) { free(results->data[row][col]); }

    results->data[row][col] = strdup((const char *) strval);

    return;
}


void SOSA_results_put_name(SOSA_results *results, int col, const char *name) {
    SOS_SET_CONTEXT(results->sos_context, "SOSA_results_col_name");

    if (col >= results->col_max) {
        SOSA_results_grow_to(results, col, results->row_max);
    }

    if (results->col_names[col] != NULL) { free(results->col_names[col]); }

    results->col_names[col] = strdup((const char *) name);

    return;
}


void SOSA_results_to_buffer(SOS_buffer *buffer, SOSA_results *results) {
    SOS_SET_CONTEXT(results->sos_context, "SOSA_results_to_buffer");

    if (results == NULL) {
        dlog(0, "ERROR: (SOSA_resutls *) results == NULL!\n");
        exit(EXIT_FAILURE);
    }
    if (buffer == NULL) {
        dlog(0, "ERROR: (SOS_buffer *) buffer == NULL!\n");
        exit(EXIT_FAILURE);
    }

    dlog(7, "Packing %d rows of %d columns into buffer...\n", results->row_count, results->col_count);


    SOS_msg_header header;
    header.msg_size = -1;
    header.msg_type = SOS_FEEDBACK_TYPE_QUERY;
    header.msg_from = SOS->config.comm_rank;
    header.ref_guid = 0;
    int offset = 0;
    SOS_msg_zip(buffer, header, 0, &offset);

    SOS_buffer_pack(buffer, &offset, "ii",
                    results->col_count,
                    results->row_count);

    int col = 0;
    int row = 0;

    dlog(7, "   ... packing column names.\n");
    for (col = 0; col < results->col_count; col++) {
        SOS_buffer_pack(buffer, &offset, "s", results->col_names[col]);
    }

    dlog(7, "   ... packing data.\n");
    for (row = 0; row < results->row_count; row++) {
        for (col = 0; col < results->col_count; col++) {
            SOS_buffer_pack(buffer, &offset, "s", results->data[row][col]);
        }
    }

    header.msg_size = offset;
    offset = 0;
    SOS_msg_zip(buffer, header, 0, &offset);
    dlog(7, "   ... done.\n");

    return;
}



void SOSA_results_from_buffer(SOSA_results *results, SOS_buffer *buffer) {
    SOS_SET_CONTEXT(buffer->sos_context, "SOSA_results_from_buffer");

    if (results == NULL) {
        dlog(0, "ERROR: (SOSA_resutls *) results == NULL!\n");
        exit(EXIT_FAILURE);
    }
    if (buffer == NULL) {
        dlog(0, "ERROR: (SOS_buffer *) buffer == NULL!\n");
        exit(EXIT_FAILURE);
    }


    int col_incoming = 0;
    int row_incoming = 0;

    SOS_msg_header header;
    int offset = 0;

    // Strip out the header...
    SOS_msg_unzip(buffer, &header, 0, &offset);

    // Start unrolling the data.
    SOS_buffer_unpack(buffer, &offset, "ii",
                      &col_incoming,
                      &row_incoming);

    dlog(9, "Unpacking a buffer with query results that contains %d rows and %d columns...\n",
        row_incoming, col_incoming);
    dlog(9, "results_befor (row_max,col_max) == %d, %d\n", results->row_max, results->col_max);
    SOSA_results_grow_to(results, col_incoming, row_incoming);
    dlog(9, "results_after (row_max,col_max) == %d, %d\n", results->row_max, results->col_max);
    SOSA_results_wipe(results);
    dlog(9, "results_wiped (row_max,col_max) == %d, %d\n", results->row_max, results->col_max);

    int col = 0;
    int row = 0;

    dlog(7, "Unpacking %d columns for %d rows...\n", col_incoming, row_incoming);
    dlog(7, "   ... headers.\n");
    for (col = 0; col < col_incoming; col++) {
        SOS_buffer_unpack_safestr(buffer, &offset, &results->col_names[col]);
    }

    dlog(7, "   ... data.\n");
    for (row = 0; row < row_incoming; row++) {
        for (col = 0; col < col_incoming; col++) {
            SOS_buffer_unpack_safestr(buffer, &offset, &results->data[row][col]);
        }
    }

    results->col_count = col_incoming;
    results->row_count = row_incoming;

    fflush(stdout);

    dlog(7, "   ... done.\n");
    return;
}






void SOSA_results_output_to(FILE *fptr, SOSA_results *results, char *title, int options) {
    SOS_SET_CONTEXT(results->sos_context, "SOSA_results_output_to");

    if (fptr == NULL) {
        dlog(0, "ERROR: (FILE *) fptr == NULL!\n");
        exit(EXIT_FAILURE);
    }
    if (results == NULL) {
        dlog(0, "ERROR: (SOSA_results *) results == NULL!\n");
        exit(EXIT_FAILURE);
    }
    if (title == NULL) {
        dlog(0, "ERROR: (char *) title == NULL!\n");
        exit(EXIT_FAILURE);
    }

    int output_mode;
    if (options & SOSA_OUTPUT_JSON) {
        output_mode = SOSA_OUTPUT_JSON;
    } else {
        output_mode = SOSA_OUTPUT_DEFAULT;  // CSV
    }

    int    row = 0;
    int    col = 0;
    double time_now = 0.0;
    SOS_TIME(time_now);

    switch(output_mode) {
    case SOSA_OUTPUT_JSON:
        
        fprintf(fptr, "{\"title\"      : \"%s\",\n",  title);
        fprintf(fptr, " \"time_stamp\" : \"%lf\",\n", time_now);
        fprintf(fptr, " \"col_count\"  : \"%d\",\n",  results->col_count);
        fprintf(fptr, " \"row_count\"  : \"%d\",\n",  results->row_count);
        fprintf(fptr, " \"data\"       : [\n");

        for (row = 0; row < results->row_count; row++) {
            fprintf(fptr, "\t{\n"); //row:begin

            fprintf(fptr, "\t\t\"result_row\": \"%d\"\n", row);
            for (col = 0; col < results->col_count; col++) {
                fprintf(fptr, "\t\t\"%s\": \"%s\"", results->col_names[col], results->data[row][col]);
                if (col < (results->col_count - 1)) {
                    fprintf(fptr, ",\n");
                } else {
                    fprintf(fptr, "\n");
                }//if

            }//for:col
            fprintf(fptr, "\t}"); //row:end
            if (row < (results->row_count - 1)) {
                fprintf(fptr, ",\n");
            } else {
                fprintf(fptr, "\n");
            }//if
        }//for:row
        fprintf(fptr, " ]\n");  //data
        fprintf(fptr, "}\n\n\n");   //json

        fflush(fptr);

        break;

        

    default://OUTPUT_CSV:
        // Display header (optional)
        if (options & SOSA_OUTPUT_W_HEADER) {
            fprintf(fptr, "\"result_row\",");
            for (col = 0; col < results->col_count; col++) {
                fprintf(fptr, "\"%s\"", results->col_names[col]);
                if (col == (results->col_count - 1)) { fprintf(fptr, "\n"); }
                else { fprintf(fptr, ","); }
            }//for:col
        }//if:header

        // Display data
        for (row = 0; row < results->row_count; row++) {
            fprintf(fptr, "\"%d\",",  row);
            for (col = 0; col < results->col_count; col++) {
                fprintf(fptr, "\"%s\"", results->data[row][col]);
                if (col == (results->col_count - 1)) { fprintf(fptr, "\n"); }
                else { fprintf(fptr, ","); }
            }//for:col
        }//for:row
        break;

    }//select

    return;
}







// NOTE: Only call this when you already hold the uid->lock
void SOSA_guid_request(SOS_runtime *sos_context, SOS_uid *uid) {
    SOS_SET_CONTEXT(sos_context, "SOSA_guid_request");

    dlog(7, "Obtaining new guid range...\n");

    SOS_buffer *msg;
    SOS_buffer *reply;
    SOS_buffer_init_sized_locking(SOS, &msg,   256, false);
    SOS_buffer_init_sized_locking(SOS, &reply, 256, false);

    SOS_msg_header header;
    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_GUID_BLOCK;
    header.msg_from = SOS->config.comm_rank;
    header.ref_guid = 0;

    int offset = 0;
    SOS_msg_zip(msg, header, 0, &offset);

    header.msg_size = offset;
    offset = 0;
    SOS_msg_zip(msg, header, 0, &offset);
    
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





void SOSA_results_init(SOS_runtime *sos_context, SOSA_results **results_object_ptraddr) {
    SOS_SET_CONTEXT(sos_context, "SOSA_results_init");
    int col = 0;
    int row = 0;

    dlog(7, "Allocating space for a new results set...\n");

    if (results_object_ptraddr != NULL) {
        if (*results_object_ptraddr == NULL) {
            *results_object_ptraddr = (SOSA_results *) calloc(1, sizeof(SOSA_results));
        }
    }

    SOSA_results *results = *results_object_ptraddr; 
    results->sos_context = SOS;

    results->col_count   = 0;
    results->row_count   = 0;
    results->col_max     = SOSA_DEFAULT_RESULT_COL_MAX;
    results->row_max     = SOSA_DEFAULT_RESULT_ROW_MAX;

    results->data = (char ***) calloc(results->row_max, sizeof(char **));
    for (row = 0; row < results->row_max; row++) {
        results->data[row] = (char **) calloc(results->col_max, sizeof(char *));
        for (col = 0; col < results->col_max; col++) {
            results->data[row][col] = NULL;
        }
    }

    results->col_names = (char **) calloc(results->col_max, sizeof(char *));
    for (col = 0; col < results->col_max; col++) {
        results->col_names[col] = NULL;
    }

    dlog(7, "    ... results->col_max = %d\n", results->col_max);
    dlog(7, "    ... results->row_max = %d\n", results->row_max);
    dlog(7, "    ... done.\n");

    return;
}


void SOSA_results_grow_to(SOSA_results *results, int new_col_max, int new_row_max) {
    SOS_SET_CONTEXT(results->sos_context, "SOSA_results_grow_to");
    int row;
    int col;

    if ((new_col_max < results->col_max) && (new_row_max < results->row_max)) {
        dlog(7, "NOTE: results->data[%d][%d] can already handle requested size[%d][%d].\n",
             results->row_max, results->col_max,
             new_row_max, new_col_max );
        dlog(7, "NOTE: Nothing to do, returning.\n");
        return;
    }

    // Grow by a minimum amount to make sure we don't realloc too often.
    if (new_col_max < (results->col_max + SOSA_DEFAULT_RESULT_COL_MAX)) {
        new_col_max =  results->col_max + SOSA_DEFAULT_RESULT_COL_MAX;
    }
    if (new_row_max < (results->row_max + SOSA_DEFAULT_RESULT_ROW_MAX)) {
        new_row_max =  results->row_max + SOSA_DEFAULT_RESULT_ROW_MAX;
    }


    dlog(7, "Growing results->data[row_max:%d][col_max:%d] to handle size[row_max:%d][col_max:%d] ...\n",
         results->row_max, results->col_max,
         new_row_max, new_col_max );

    if (new_col_max > results->col_max) {

        // Add column space to column names...
        results->col_names = (char **) realloc(results->col_names, (new_col_max * sizeof(char *)));
        // Initialize it.
        for (col = results->col_max; col < new_col_max; col++) {
            results->col_names[col] = NULL;
        }

        // Add column space to existing rows...
        for (row = 0; row < results->row_max; row++) {
            results->data[row] = (char **) realloc(results->data[row], (new_col_max * sizeof(char *)));
            // Initialize it.
            for (col = results->col_max; col < new_col_max; col++) {
                results->data[row][col] = NULL;
            }
        }
        results->col_max = new_col_max;
    }
    

    if (new_row_max > results->row_max) {
        // Add additional rows space
        results->data = (char ***) realloc(results->data, (new_row_max * sizeof(char **)));
        // For each new row...
        for (row = results->row_max; row < new_row_max; row++) {
            // ...add space for columns
            results->data[row] = (char **) calloc(results->col_max, sizeof(char **));
            for (col = 0; col < results->col_max; col++) {
                // ...and initialize each one.
                results->data[row][col] = NULL;
            }
        }
        results->row_max = new_row_max;
    }

    dlog(7, "    ... done.\n");
    return;
}


void SOSA_results_wipe(SOSA_results *results) {
    SOS_SET_CONTEXT(results->sos_context, "SOSA_results_wipe");

    int row = 0;
    int col = 0;

    dlog(7, "Wiping results set...\n");
    dlog(7, "    ... results->col_max = %d\n", results->col_max);
    dlog(7, "    ... results->row_max = %d\n", results->row_max);

    for (row = 0; row < results->row_max; row++) {
        for (col = 0; col < results->col_max; col++) {
            if (results->data[row][col] != NULL) {
                free(results->data[row][col]);
                results->data[row][col] = NULL;
            }
        }
    }

    for (col = 0; col < results->col_max; col++) {
        if (results->col_names[col] != NULL) {
            free(results->col_names[col]);
            results->col_names[col] = NULL;
        }
    }

    results->col_count = 0;
    results->row_count = 0;

    dlog(7, "    ... done.\n");

    return;
}


// NOTE: Better to wipe and re-use a small results table if possible rather
//       than malloc/free a lot.  But... whatever works best.
void SOSA_results_destroy(SOSA_results *results) {
    SOS_SET_CONTEXT(results->sos_context, "SOSA_results_destroy");

    dlog(7, "Destroying results set...\n");
    dlog(7, "    ... results->col_count == %d of %d\n", results->col_count, results->col_max);
    dlog(7, "    ... results->row_count == %d of %d\n", results->row_count, results->row_max);

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
    free(results);

    dlog(7, "    ... done.\n");

    return;
}


void SOSA_send_to_target_db(SOS_buffer *msg, SOS_buffer *reply) {
    SOS_SET_CONTEXT(msg->sos_context, "SOSA_send_to_target_db");

    if ((msg == NULL) || (reply == NULL)) {
        dlog(0, "ERROR: Buffer pointer supplied with NULL value!\n");
        exit(EXIT_FAILURE);
    }
    SOS_send_to_daemon(msg, reply);
    return;
}



