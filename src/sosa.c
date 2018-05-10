/*
 *   sosa.c   Library functions for writing SOS analytics modules.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>

#include "sos.h"
#include "sosa.h"
#include "sos_re.h"
#include "sos_types.h"
#include "sos_debug.h"
#include "sos_target.h"

void SOSA_cache_to_results(
        SOS_runtime *sos_context,
        SOSA_results *results,
        char *pub_filter_regex,
        char *val_filter_regex,
        int frame_head,
        int frame_depth_limit)
{
    SOS_SET_CONTEXT(sos_context, "SOSA_cache_to_results");
    double start_time = 0.0;
    double stop_time  = 0.0;
    SOS_TIME(start_time);
  
    SOS_re_t pub_regex = SOS_re_compile(pub_filter_regex);
    SOS_re_t val_regex = SOS_re_compile(val_filter_regex);

    SOS_pub *pub = NULL;
    SOS_list_entry *entry = SOSD.pub_list_head;

    int row = 0;

    int frames_grabbed = 0;

    int i = 0;

    SOSA_results_put_name(results, 0,  "process_id");
    SOSA_results_put_name(results, 1,  "node_id");
    SOSA_results_put_name(results, 2,  "pub_title");
    SOSA_results_put_name(results, 3,  "pub_guid");
    SOSA_results_put_name(results, 4,  "comm_rank");
    SOSA_results_put_name(results, 5,  "prog_name");
    SOSA_results_put_name(results, 6,  "time_pack");
    SOSA_results_put_name(results, 7,  "time_recv");
    SOSA_results_put_name(results, 8,  "frame");
    SOSA_results_put_name(results, 9,  "relation_id");
    SOSA_results_put_name(results, 10, "val_name");
    SOSA_results_put_name(results, 11, "val_type");
    SOSA_results_put_name(results, 12, "val_guid");
    SOSA_results_put_name(results, 13, "val");

    char comm_rank_str    [128] = {0};
    char process_id_str   [128] = {0};
    char time_pack_str    [128] = {0};
    char time_recv_str    [128] = {0};
    char val_frame_str    [128] = {0};
    char val_relation_str [128] = {0};
    char val_guid_str     [128] = {0};
    char val_type_str     [128] = {0};

    char *val_str;
    char val_numeric_str  [128] = {0};

    printf("filters...  pub: %s    val: %s\n",
            pub_filter_regex,
            val_filter_regex);

    while (entry != NULL) {
        pub = (SOS_pub *) entry->ref;
        if (pub == NULL) break;
        //if ((pub->cache_depth > 0)  //NOTE: Regex isn't working right yet.
        //    && (SOS_re_matchp(pub_regex, pub->title)))
        if ((pub->cache_depth > 0)
            && (strcmp(pub_filter_regex, pub->title) == 0))
        {
            for (i = 0; i < pub->elem_count; i++) {
                //if (SOS_re_matchp(val_regex, pub->data[i]->name)) {
                if (strcmp(val_filter_regex, pub->data[i]->name) == 0) {
                    SOS_val_snap *snap = pub->data[i]->cached_latest;
                    frames_grabbed = 0;

                    while (snap != NULL) {
                        // Apply our frame constraints, if any:
                        if (   (frame_head >= 0)
                            && (snap->frame > frame_head)) {
                            // If the request is starting at a specific frame
                            // and we've not hit it yet, move to the next snap.
                            // NOTE: Snaps are stored as a push-down stack, so
                            //       cached_latest is the largest frame #, and
                            //       snap->next_snap is a lower/earlier frame.
                            
                            snap = snap->next_snap;
                            continue;
                        }
                        if (   (frame_depth_limit > 0)
                            && (frames_grabbed >= frame_depth_limit)) {
                            // If we're limiting results, and we're past the
                            // limit, move on to the next value in the pub.
                            
                            break;
                        }

                        //If we're here, we have a snapshot that matches
                        //all of the filtering/frame constraints.
                        
                        //Put all the numeric fields into strings:
                        snprintf(process_id_str,   128, "%d", pub->process_id);
                        snprintf(comm_rank_str,    128, "%d", pub->comm_rank);
                        snprintf(time_pack_str,    128, "%lf", snap->time.pack);
                        snprintf(time_recv_str,    128, "%lf", snap->time.recv);
                        snprintf(val_frame_str,    128, "%ld", snap->frame);
                        snprintf(val_relation_str, 128, "%"SOS_GUID_FMT,
                                snap->relation_id);
                        snprintf(val_type_str,     128, "%d", snap->type);
                        snprintf(val_guid_str,     128, "%"SOS_GUID_FMT,
                                snap->guid);


                        switch(snap->type) {
                        case SOS_VAL_TYPE_INT:
                            snprintf(val_numeric_str, 128, "%d", snap->val.i_val);
                            val_str = val_numeric_str; break;
                        case SOS_VAL_TYPE_LONG:
                            snprintf(val_numeric_str, 128, "%ld", snap->val.l_val);
                            val_str = val_numeric_str; break;
                        case SOS_VAL_TYPE_DOUBLE:
                            snprintf(val_numeric_str, 128, "%lf", snap->val.d_val);
                            val_str = val_numeric_str; break;
                        case SOS_VAL_TYPE_STRING:
                            if (snap->val.c_val != NULL) {
                                val_str = snap->val.c_val; break;
                            } else {
                                snprintf(val_numeric_str, 128, "(null)");
                                val_str = val_numeric_str; break;
                            }
                        case SOS_VAL_TYPE_BYTES:
                            snprintf(val_numeric_str, 128, "(bytes)");
                            val_str = val_numeric_str; break;
                        default:
                            snprintf(val_numeric_str, 128, "(unknown type)");
                            val_str = val_numeric_str; break;
                        }

                        // SOSA_results_put makes a copy of these values:
                        SOSA_results_put(results, 0,  row, process_id_str);
                        SOSA_results_put(results, 1,  row, pub->node_id);
                        SOSA_results_put(results, 2,  row, pub->title);
                        SOSA_results_put(results, 3,  row, pub->guid_str);
                        SOSA_results_put(results, 4,  row, comm_rank_str);
                        SOSA_results_put(results, 5,  row, pub->prog_name);
                        SOSA_results_put(results, 6,  row, time_pack_str);
                        SOSA_results_put(results, 7,  row, time_recv_str);
                        SOSA_results_put(results, 8,  row, val_frame_str);
                        SOSA_results_put(results, 9,  row, val_relation_str);
                        SOSA_results_put(results, 10, row, pub->data[i]->name);
                        SOSA_results_put(results, 11, row, val_type_str);
                        SOSA_results_put(results, 12, row, val_guid_str);
                        SOSA_results_put(results, 13, row, val_str);

                        frames_grabbed++;

                        snap = snap->next_snap;
                        row++;
                        results->row_count = row;
                    }//while: snap
                }//for: vals
            }//end:if[name]
        }//end:for[elems]

        entry = entry->next_entry; 
    }//while: pubs

    SOS_TIME(stop_time);
    results->exec_duration = (stop_time - start_time);

    return;
}



SOS_guid
SOSA_cache_grab(
        SOS_runtime *sos_context,
        char *pub_filter_regex,
        char *val_filter_regex,
        int frame_head,
        int frame_depth_limit,
        char *target_host,
        int target_port)
{
    SOS_SET_CONTEXT(sos_context, "SOSA_cache_grab");

    dlog(7, "Submitting request for cached values matching"
            " pub == \"%s\" && val == \"%s\"   ...\n",
                pub_filter_regex, val_filter_regex);

    SOS_buffer *msg;
    SOS_buffer *reply;
    SOS_buffer_init_sized_locking(SOS, &msg,   4096, false);
    SOS_buffer_init_sized_locking(SOS, &reply, 2048, false);

    SOS_msg_header header;
    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_MATCH_PUBS;
    header.msg_from = SOS->config.comm_rank;
    header.ref_guid = 0;

    dlog(7, "   ... creating msg.\n");

    int offset = 0;
    SOS_msg_zip(msg, header, 0, &offset);
    
    SOS_guid request_guid;
    if (SOS->role == SOS_ROLE_CLIENT) {
        // NOTE: This guid is returned by the function so it can
        // be tracked by clients.  They can blast out a bunch
        // of queries to different daemons that get returned
        // asynchronously, and can do some internal bookkeeping by
        // uniting the results with the original query submission.
        request_guid = SOS_uid_next(SOS->uid.my_guid_pool);
    } else {
        // Or...
        // this generally should not happen unless the daemon is
        // submitting queries internally, which is downright
        // funky and shouldn't be happening, IMO.  -CW
        request_guid = -99999;
    }
    dlog(7, "   ... assigning request_guid = %" SOS_GUID_FMT "\n",
            request_guid);
    
    SOS_buffer_pack(msg, &offset, "s", SOS->config.node_id);
    SOS_buffer_pack(msg, &offset, "i", SOS->config.receives_port);
    SOS_buffer_pack(msg, &offset, "s", pub_filter_regex);
    SOS_buffer_pack(msg, &offset, "s", val_filter_regex);
    SOS_buffer_pack(msg, &offset, "i", frame_head);
    SOS_buffer_pack(msg, &offset, "i", frame_depth_limit);
    SOS_buffer_pack(msg, &offset, "g", request_guid);

    header.msg_size = offset;
    offset = 0;
    SOS_msg_zip(msg, header, 0, &offset);

    dlog(7, "   ... sending match request to daemon.\n");
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
    return request_guid;
}



SOS_guid
SOSA_exec_query(SOS_runtime *sos_context, char *query,
        char *target_host, int target_port)
{
    SOS_SET_CONTEXT(sos_context, "SOSA_exec_query");

    dlog(7, "Submitting query (%25s) ...\n", query);

    SOS_buffer *msg;
    SOS_buffer *reply;
    SOS_buffer_init_sized_locking(SOS, &msg,   4096, false);
    SOS_buffer_init_sized_locking(SOS, &reply, 2048, false);


    SOS_msg_header header;
    header.msg_size = -1;
    header.msg_type = SOS_MSG_TYPE_QUERY;
    header.msg_from = SOS->config.comm_rank;
    header.ref_guid = 0;

    dlog(7, "   ... creating msg.\n");

    int offset = 0;
    SOS_msg_zip(msg, header, 0, &offset);
    
    SOS_guid query_guid;
    if (SOS->role == SOS_ROLE_CLIENT) {
        // NOTE: This guid is returned by the function so it can
        // be tracked by clients.  They can blast out a bunch
        // of queries to different daemons that get returned
        // asynchronously, and can do some internal bookkeeping by
        // uniting the results with the original query submission.
        query_guid = SOS_uid_next(SOS->uid.my_guid_pool);
    } else {
        // Or...
        // this generally should not happen unless the daemon is
        // submitting queries internally, which is downright
        // funky and shouldn't be happening, IMO.  -CW
        query_guid = -99999;
    }
    dlog(7, "   ... assigning query_guid = %" SOS_GUID_FMT "\n",
            query_guid);
    
    int rc = 0;
    rc = SOS_buffer_pack(msg, &offset, "s", SOS->config.node_id);
    if (rc <= 0) { /* simple error check */ return -99; }
    rc = SOS_buffer_pack(msg, &offset, "i", SOS->config.receives_port);
    if (rc <= 0) { /* simple error check */ return -99; }
    rc = SOS_buffer_pack(msg, &offset, "s", query);
    if (rc <= 0) { /* simple error check */ return -99; }
    rc = SOS_buffer_pack(msg, &offset, "g", query_guid);
    if (rc <= 0) { /* simple error check */ return -99; }

    header.msg_size = offset;
    offset = 0;
    SOS_msg_zip(msg, header, 0, &offset);

    dlog(7, "   ... sending to daemon.\n");
    SOS_socket *target = NULL;
    rc = SOS_target_init(SOS, &target, target_host, target_port);
    if (rc < 0) { /* simple error check */ return -99; }
    rc = SOS_target_connect(target);
    if (rc < 0) { /* simple error check */ return -99; }
    rc = SOS_target_send_msg(target, msg);
    if (rc < 0) { /* simple error check */ return -99; }
    rc = SOS_target_recv_msg(target, reply);
    if (rc < 0) { /* simple error check */ return -99; }
    rc = SOS_target_disconnect(target);
    if (rc < 0) { /* simple error check */ return -99; }
    rc = SOS_target_destroy(target);
    if (rc < 0) { /* simple error check */ return -99; }

    SOS_buffer_destroy(msg);
    SOS_buffer_destroy(reply);

    dlog(7, "   ... done.\n");
    return query_guid;
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

    if (results->row_count < (row + 1)) { results->row_count = (row + 1); }
    if (results->col_count < (col + 1)) { results->col_count = (col + 1); }

    results->data[row][col] = strdup((const char *) strval);

    return;
}


void SOSA_results_put_name(SOSA_results *results, int col, const char *name) {
    SOS_SET_CONTEXT(results->sos_context, "SOSA_results_put_name");

    if (col >= results->col_max) {
        SOSA_results_grow_to(results, col, 0);
    }

    if (results->col_names[col] != NULL) { free(results->col_names[col]); }

    dlog(5, "Resultset(%d x %d) column[%2d].name == \"%s\"\n",
            results->col_max, results->row_max, col, name);

    results->col_names[col] = strdup((const char *) name);

    return;
}


void SOSA_results_label(SOSA_results *results, SOS_guid guid, const char *sql) {
    if (results == NULL) {
        fprintf(stderr, "CRITICAL ERROR: Attempting to label a NULL results set.\n");
        fprintf(stderr, "                Doing nothing and returning.\n");
        return;
    }
    SOS_SET_CONTEXT(results->sos_context, "SOSA_results_label");

    dlog(7, "Tagging results with guid and sql metadata:\n");
    dlog(7, "   ... results->guid = %" SOS_GUID_FMT "\n", guid);
    results->query_guid = guid;

    if (results->query_sql != NULL) {
        dlog(7, "   ... free'ing existing SQL string: \"%s\"\n",
                results->query_sql);
        free(results->query_sql);
        results->query_sql = NULL;
    }
    if (sql == NULL) {
        dlog(7, "   ... results->query_sql = \"%s\"\n", sql);
        results->query_sql = strdup("[none]");
    } else {
        results->query_sql = strdup(sql);
    }

    dlog(7, "Done.\n");

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

    if (results->query_sql == NULL) {
        dlog(1, "WARNING: Encoding query results that have not been labelled.\n");
    }

    if (strlen(results->query_sql) < 1) {
        results->query_sql = strdup("<none>");
    }
 
    SOS_buffer_pack(buffer, &offset, "sgd",
                    results->query_sql,
                    results->query_guid,
                    results->exec_duration);

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

    dlog(9, "Stripping out the header...\n");
    SOS_msg_unzip(buffer, &header, 0, &offset);

    dlog(9, "Unpacking the query's SQL and guid...\n");
    results->query_sql = NULL;
    SOS_buffer_unpack_safestr(buffer, &offset, &results->query_sql);
    dlog(9, "   ... SQL: %s\n", results->query_sql);
    SOS_buffer_unpack(buffer, &offset, "g", &results->query_guid);
    dlog(9, "   ... guid: %" SOS_GUID_FMT "\n", results->query_guid);
    SOS_buffer_unpack(buffer, &offset, "d", &results->exec_duration);
    dlog(9, "   ... exec_duration: %3.12lf\n", results->exec_duration);

    // Start unrolling the data.
    SOS_buffer_unpack(buffer, &offset, "ii",
                      &col_incoming,
                      &row_incoming);

    dlog(9, "Query contains %d rows and %d columns...\n",
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
        results->col_names[col] = NULL;
        SOS_buffer_unpack_safestr(buffer, &offset, &results->col_names[col]);
    }

    dlog(7, "   ... data.\n");
    for (row = 0; row < row_incoming; row++) {
        for (col = 0; col < col_incoming; col++) {
            results->data[row][col] = NULL;
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
        
        fprintf(fptr, "{\n \"title\"      : \"%s\",\n",  title);
        fprintf(fptr, " \"time_stamp\" : \"%lf\",\n", time_now);
//        fprintf(fptr, " \"query_guid\" : \"%" SOS_GUID_FMT "\",\n", results->query_guid);
//        fprintf(fptr, " \"query_sql\"  : \"%s\",\n", results->query_sql);
        fprintf(fptr, " \"exec_duration\" : \"%3.12lf\",\n", results->exec_duration);
        fprintf(fptr, " \"col_count\"  : \"%d\",\n",  results->col_count);
        fprintf(fptr, " \"row_count\"  : \"%d\",\n",  results->row_count);
        fprintf(fptr, " \"data\"       :\n [\n");

        for (row = 0; row < results->row_count; row++) {
            fprintf(fptr, "\t{\n"); //row:begin

            fprintf(fptr, "\t\t\"result_row\": \"%d\",\n", row);
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
            //fprintf(fptr, "title      : \"%s\"\n",  title);
            //fprintf(fptr, "time_stamp : \"%lf\"\n", time_now);
            //fprintf(fptr, "query_guid : \"%" SOS_GUID_FMT "\"\n", results->query_guid);
            //fprintf(fptr, "query_sql  : \"%s\"\n", results->query_sql);
            //fprintf(fptr, "col_count  : \"%d\"\n",  results->col_count);
            //fprintf(fptr, "row_count  : \"%d\"\n",  results->row_count);
            //fprintf(fptr, "----------\n");
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

    printf("\n");
    printf("Query executed in %lf seconds.\n",
            results->exec_duration);

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

    // These get set manually w/in the daemons:
    results->query_guid    = -1;
    results->query_sql     = NULL;   //strdup("NONE");
    results->exec_duration = 0.0;

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


void SOSA_results_grow_to(SOSA_results *results, int new_col_ask, int new_row_ask) {
    SOS_SET_CONTEXT(results->sos_context, "SOSA_results_grow_to");
    int row;
    int col;

    int new_col_max = new_col_ask;
    int new_row_max = new_row_ask;


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

    if (new_col_ask >= results->col_max) {

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
    

    if (new_row_ask >= results->row_max) {
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

    results->query_guid = -1;
    if (results->query_sql != NULL) {
        free(results->query_sql);
        results->query_sql = NULL;
    }

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

    dlog(7, "Destroying results set for results->query_guid == %" SOS_GUID_FMT "\n",
            results->query_guid);
    dlog(7, "    ... results->col_count == %d of %d\n", results->col_count, results->col_max);
    dlog(7, "    ... results->row_count == %d of %d\n", results->row_count, results->row_max);

    int row = 0;
    int col = 0;
    dlog(7, "    ... free'ing cells...\n");
    for (row = 0; row < results->row_count; row++) {
        for (col = 0; col < results->col_count; col++) {
            free(results->data[row][col]);
        }
    }

    dlog(7, "    ... free'ing columns...\n");
    for (row = 0; row < results->row_max; row++) {
        free(results->data[row]);
    }

    dlog(7, "    ... free'ing rows...\n");
    free(results->data);

    dlog(7, "    ... free'ing column names...\n");
    for (col = 0; col < results->col_max; col++) {
        if (results->col_names[col] != NULL) {
            free(results->col_names[col]);
        }
    }
    free(results->col_names);

    results->query_guid = -1;
    if (results->query_sql != NULL) {
        dlog(7, "    ... free'ing SQL string...\n");
        free(results->query_sql);
        results->query_sql = NULL;
    }

    dlog(7, "    ... free'ing results object itself...\n");
    free(results);

    dlog(7, "Done.\n");

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



