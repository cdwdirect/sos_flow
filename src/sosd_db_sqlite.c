/*
 *  sosd_db_sqlite.c
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sqlite3.h"

#include "sos.h"
#include "sos_debug.h"
#include "sosd_db_sqlite.h"
#include "sosa.h"

#define SOSD_DB_PUBS_TABLE_NAME "tblPubs"
#define SOSD_DB_DATA_TABLE_NAME "tblData"
#define SOSD_DB_VALS_TABLE_NAME "tblVals"
#define SOSD_DB_ENUM_TABLE_NAME "tblEnums"
#define SOSD_DB_SOSD_TABLE_NAME "tblSOSDConfig"


void SOSD_db_insert_enum(const char *type, const char **var_strings, int max_index);


sqlite3      *database;
sqlite3_stmt *stmt_insert_pub;
sqlite3_stmt *stmt_insert_data;
sqlite3_stmt *stmt_insert_val;
sqlite3_stmt *stmt_insert_enum;
sqlite3_stmt *stmt_insert_sosd;

#if (SOS_CONFIG_DB_ENUM_STRINGS > 0)
    #define __ENUM_DB_TYPE " STRING "
    #define __ENUM_C_TYPE const char*
    #define __ENUM_VAL(__source, __enum_handle) \
        SOS_ENUM_STR( __source, __enum_handle )
    #define __BIND_ENUM(__statement, __position, __variable) \
        CALL_SQLITE( bind_text(__statement, __position, __variable, 1 + strlen(__variable), SQLITE_STATIC))
#else
    #define __ENUM_DB_TYPE " INTEGER "
    #define __ENUM_C_TYPE int
    #define __ENUM_VAL(__source, __enum_handle) \
        __source
    #define __BIND_ENUM(__statement, __position, __variable) \
        CALL_SQLITE(bind_int(__statement, __position, __variable))
#endif

char *sql_create_table_pubs = ""                                        \
    "CREATE TABLE IF NOT EXISTS " SOSD_DB_PUBS_TABLE_NAME " ( "         \
    " row_id "          " INTEGER PRIMARY KEY, "                        \
    " guid "            " UNSIGNED BIG INT, "                           \
    " title "           " STRING, "                                     \
    " process_id "      " INTEGER, "                                    \
    " thread_id "       " INTEGER, "                                    \
    " comm_rank "       " INTEGER, "                                    \
    " node_id "         " STRING, "                                     \
    " prog_name "       " STRING, "                                     \
    " prog_ver "        " STRING, "                                     \
    " meta_channel "    " INTEGER, "                                    \
    " meta_nature "     __ENUM_DB_TYPE ", "                                \
    " meta_layer "      __ENUM_DB_TYPE ", "                                \
    " meta_pri_hint "   __ENUM_DB_TYPE ", "                                \
    " meta_scope_hint " __ENUM_DB_TYPE ", "                                \
    " meta_retain_hint "__ENUM_DB_TYPE ", "                                \
    " pragma "          " STRING); ";

char *sql_create_table_data = ""                                        \
    "CREATE TABLE IF NOT EXISTS " SOSD_DB_DATA_TABLE_NAME " ( "         \
    " row_id "          " INTEGER PRIMARY KEY, "                        \
    " pub_guid "        " UNSIGNED BIG INT, "                           \
    " guid "            " UNSIGNED BIG INT, "                           \
    " name "            " STRING, "                                     \
    " val_type "        __ENUM_DB_TYPE ", "                                \
    " meta_freq "       __ENUM_DB_TYPE ", "                                \
    " meta_class "      __ENUM_DB_TYPE ", "                                \
    " meta_pattern "    __ENUM_DB_TYPE ", "                                \
    " meta_compare "    __ENUM_DB_TYPE ");";

char *sql_create_table_vals = ""                                        \
    "CREATE TABLE IF NOT EXISTS " SOSD_DB_VALS_TABLE_NAME " ( "         \
    " row_id "          " INTEGER PRIMARY KEY, "                        \
    " guid "            " UNSIGNED BIG INT, "                           \
    " val "             " STRING, "                                     \
    " frame "           " INTEGER, "                                    \
    " meta_semantic "   __ENUM_DB_TYPE ", "                                \
    " meta_mood "       __ENUM_DB_TYPE ", "                                \
    " time_pack "       " DOUBLE, "                                     \
    " time_send "       " DOUBLE, "                                     \
    " time_recv "       " DOUBLE); ";


char *sql_create_table_enum = ""                                        \
    "CREATE TABLE IF NOT EXISTS " SOSD_DB_ENUM_TABLE_NAME " ( "         \
    " row_id "          " INTEGER PRIMARY KEY, "                        \
    " type "            " STRING, "                                     \
    " text "            " STRING, "                                     \
    " enum_val "        " INTEGER); ";

char *sql_create_table_sosd_config = ""                \
    "CREATE TABLE IF NOT EXISTS " SOSD_DB_SOSD_TABLE_NAME " ( " \
    " row_id "          " INTEGER PRIMARY KEY, "                \
    " daemon_id "       " STRING, "                             \
    " key "             " STRING, "                             \
    " value "           " STRING); ";



char *sql_create_index_tblvals = "CREATE INDEX tblVals_GUID ON tblVals.guid;";
char *sql_create_index_tbldata = "CREATE INDEX tblData_GUID ON tblData.guid;";
char *sql_create_index_tblpubs = "CREATE INDEX tblPubs_GUID ON tblPubs.guid;";


char *sql_insert_pub = ""                                               \
    "INSERT INTO " SOSD_DB_PUBS_TABLE_NAME " ("                         \
    " guid,"                                                            \
    " title,"                                                           \
    " process_id,"                                                      \
    " thread_id,"                                                       \
    " comm_rank,"                                                       \
    " node_id,"                                                         \
    " prog_name,"                                                       \
    " prog_ver,"                                                        \
    " meta_channel,"                                                    \
    " meta_nature,"                                                     \
    " meta_layer,"                                                      \
    " meta_pri_hint,"                                                   \
    " meta_scope_hint,"                                                 \
    " meta_retain_hint,"                                                \
    " pragma "                                                          \
    ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?); ";

const char *sql_insert_data = ""                                        \
    "INSERT INTO " SOSD_DB_DATA_TABLE_NAME " ("                         \
    " pub_guid,"                                                        \
    " guid,"                                                            \
    " name,"                                                            \
    " val_type,"                                                        \
    " meta_freq,"                                                       \
    " meta_class,"                                                      \
    " meta_pattern,"                                                    \
    " meta_compare "                                                    \
    ") VALUES (?,?,?,?,?,?,?,?); ";

const char *sql_insert_val = ""                                         \
    "INSERT INTO " SOSD_DB_VALS_TABLE_NAME " ("                         \
    " guid,"                                                            \
    " val, "                                                            \
    " frame, "                                                          \
    " meta_semantic, "                                                  \
    " meta_mood, "                                                      \
    " time_pack,"                                                       \
    " time_send,"                                                       \
    " time_recv "                                                       \
    ") VALUES (?,?,?,?,?,?,?,?); ";


const char *sql_insert_enum = ""                \
    "INSERT INTO " SOSD_DB_ENUM_TABLE_NAME " (" \
    " type,"                                    \
    " text,"                                    \
    " enum_val "                                \
    ") VALUES (?,?,?); ";

const char *sql_insert_sosd_config = ""         \
    "INSERT INTO " SOSD_DB_SOSD_TABLE_NAME " (" \
    " daemon_id, "                              \
    " key, "                                    \
    " value "                                   \
    ") VALUES (?,?,?); ";

char *sql_cmd_begin_transaction    = "BEGIN DEFERRED TRANSACTION;";
char *sql_cmd_commit_transaction   = "COMMIT TRANSACTION;";


void SOSD_db_init_database() {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_init_database");
    int     retval;
    int     flags;

    dlog(1, "Opening database...\n");

    SOSD.db.ready = 0;
    SOSD.db.file  = (char *) malloc(SOS_DEFAULT_STRING_LEN);
    memset(SOSD.db.file, '\0', SOS_DEFAULT_STRING_LEN);

    SOSD.db.lock  = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init( SOSD.db.lock, NULL );
    pthread_mutex_lock( SOSD.db.lock );

    flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;

    #if (SOSD_CLOUD_SYNC > 0)
    snprintf(SOSD.db.file, SOS_DEFAULT_STRING_LEN, "%s/%s.%05d.db", SOSD.daemon.work_dir, SOSD.daemon.name, SOS->config.comm_rank);
    #else
    snprintf(SOSD.db.file, SOS_DEFAULT_STRING_LEN, "%s/%s.local.db", SOSD.daemon.work_dir, SOSD.daemon.name);
    #endif

    /*
     *   "unix-none"     =no locking (NOP's)
     *   "unix-excl"     =single-access only
     *   "unix-dotfile"  =uses a file as the lock.
     */

    retval = sqlite3_open_v2(SOSD.db.file, &database, flags, "unix-dotfile");
    if( retval ){
        dlog(0, "ERROR!  Can't open database: %s   (%s)\n", SOSD.db.file, sqlite3_errmsg(database));
        sqlite3_close(database);
        exit(EXIT_FAILURE);
    } else {
        dlog(1, "Successfully opened database.\n");
    }

    sqlite3_exec(database, "PRAGMA synchronous   = ON;",       NULL, NULL, NULL); // OFF = Let the OS handle flushes.
    sqlite3_exec(database, "PRAGMA cache_size    = 31250;",    NULL, NULL, NULL); // x 2048 def. page size = 64MB cache
    sqlite3_exec(database, "PRAGMA cache_spill   = FALSE;",    NULL, NULL, NULL); // Spilling goes exclusive, it's wasteful.
    sqlite3_exec(database, "PRAGMA temp_store    = MEMORY;",   NULL, NULL, NULL); // If we crash, we crash.
    sqlite3_exec(database, "PRAGMA journal_mode  = DELETE;",   NULL, NULL, NULL); // Default
  //sqlite3_exec(database, "PRAGMA journal_mode  = MEMORY;",   NULL, NULL, NULL); // ...ditto.  Speed prevents crashes.
  //sqlite3_exec(database, "PRAGMA journal_mode  = WAL;",      NULL, NULL, NULL); // This is the fastest file-based journal option.

    SOS_pipe_init(SOS, &SOSD.db.snap_queue, sizeof(SOS_val_snap *));

    if (SOSD.db.snap_queue->elem_count == 0) {
      SOSD.db.snap_queue->sync_pending = 0;
    }

    SOSD_db_create_tables();

    sqlite3_exec(database, sql_create_index_tblvals, NULL, NULL, NULL);
    sqlite3_exec(database, sql_create_index_tbldata, NULL, NULL, NULL);
    sqlite3_exec(database, sql_create_index_tblpubs, NULL, NULL, NULL);


    dlog(2, "Preparing transactions...\n");

    dlog(2, "  --> \"%.50s...\"\n", sql_insert_pub);
    retval = sqlite3_prepare_v2(database, sql_insert_pub, strlen(sql_insert_pub) + 1, &stmt_insert_pub, NULL);
    if (retval) { dlog(2, "  ... error (%d) was returned.\n", retval); }

    dlog(2, "  --> \"%.50s...\"\n", sql_insert_data);
    retval = sqlite3_prepare_v2(database, sql_insert_data, strlen(sql_insert_data) + 1, &stmt_insert_data, NULL);
    if (retval) { dlog(2, "  ... error (%d) was returned.\n", retval); }

    dlog(2, "  --> \"%.50s...\"\n", sql_insert_val);
    retval = sqlite3_prepare_v2(database, sql_insert_val, strlen(sql_insert_val) + 1, &stmt_insert_val, NULL);
    if (retval) { dlog(2, "  ... error (%d) was returned.\n", retval); }

    dlog(2, "  --> \"%.50s...\"\n", sql_insert_enum);
    retval = sqlite3_prepare_v2(database, sql_insert_enum, strlen(sql_insert_enum) + 1, &stmt_insert_enum, NULL);
    if (retval) { dlog(2, "  ... error (%d) was returned.\n", retval); }

    dlog(2, "  --> \"%.50s...\"\n", sql_insert_sosd_config);
    retval = sqlite3_prepare_v2(database, sql_insert_sosd_config, strlen(sql_insert_sosd_config) + 1, &stmt_insert_sosd, NULL);
    if (retval) { dlog(2, "  ... error (%d) was returned.\n", retval); }


    SOSD.db.ready = 1;
    pthread_mutex_unlock(SOSD.db.lock);

    SOSD_db_transaction_begin();
    dlog(2, "  Inserting the enumeration table...\n");

    /* TODO: {DB, ENUM}  Make this a macro expansion... */
    SOSD_db_insert_enum("ROLE",          SOS_ROLE_string,          SOS_ROLE___MAX          );
    SOSD_db_insert_enum("TARGET",        SOS_TARGET_string,        SOS_TARGET___MAX        );
    SOSD_db_insert_enum("STATUS",        SOS_STATUS_string,        SOS_STATUS___MAX        );
    SOSD_db_insert_enum("MSG_TYPE",      SOS_MSG_TYPE_string,      SOS_MSG_TYPE___MAX      );
    SOSD_db_insert_enum("FEEDBACK",      SOS_FEEDBACK_string,      SOS_FEEDBACK___MAX      );
    SOSD_db_insert_enum("PRI",           SOS_PRI_string,           SOS_PRI___MAX           );
    SOSD_db_insert_enum("VAL_TYPE",      SOS_VAL_TYPE_string,      SOS_VAL_TYPE___MAX      );
    SOSD_db_insert_enum("VAL_STATE",     SOS_VAL_STATE_string,     SOS_VAL_STATE___MAX     );
    SOSD_db_insert_enum("VAL_SYNC",      SOS_VAL_SYNC_string,      SOS_VAL_SYNC___MAX      );
    SOSD_db_insert_enum("VAL_FREQ",      SOS_VAL_FREQ_string,      SOS_VAL_FREQ___MAX      );
    SOSD_db_insert_enum("VAL_SEMANTIC",  SOS_VAL_SEMANTIC_string,  SOS_VAL_SEMANTIC___MAX  );
    SOSD_db_insert_enum("VAL_PATTERN",   SOS_VAL_PATTERN_string,   SOS_VAL_PATTERN___MAX   );
    SOSD_db_insert_enum("VAL_COMPARE",   SOS_VAL_COMPARE_string,   SOS_VAL_COMPARE___MAX   );
    SOSD_db_insert_enum("VAL_CLASS",     SOS_VAL_CLASS_string,     SOS_VAL_CLASS___MAX     );
    SOSD_db_insert_enum("MOOD",          SOS_MOOD_string,          SOS_MOOD___MAX          );
    SOSD_db_insert_enum("SCOPE",         SOS_SCOPE_string,         SOS_SCOPE___MAX         );
    SOSD_db_insert_enum("LAYER",         SOS_LAYER_string,         SOS_LAYER___MAX         );
    SOSD_db_insert_enum("NATURE",        SOS_NATURE_string,        SOS_NATURE___MAX        );
    SOSD_db_insert_enum("RETAIN",        SOS_RETAIN_string,        SOS_RETAIN___MAX        );

    SOSD_db_transaction_commit();

    dlog(2, "  ... done.\n");

    return;
}


void SOSD_db_close_database() {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_close_database");

    dlog(2, "Closing database.   (%s)\n", SOSD.db.file);
    //pthread_mutex_lock( SOSD.db.lock );
    dlog(2, "  ... finalizing statements.\n");
    SOSD.db.ready = -1;
    CALL_SQLITE (finalize(stmt_insert_pub));
    CALL_SQLITE (finalize(stmt_insert_data));
    CALL_SQLITE (finalize(stmt_insert_val));
    CALL_SQLITE (finalize(stmt_insert_enum));
    CALL_SQLITE (finalize(stmt_insert_sosd));
    dlog(2, "  ... closing database file.\n");
    sqlite3_close(database);
    dlog(2, "  ... destroying the mutex.\n");
    pthread_mutex_destroy(SOSD.db.lock);
    free(SOSD.db.lock);
    free(SOSD.db.file);
    dlog(2, "  ... done.\n");

    return;
}


void SOSD_db_transaction_begin() {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_transaction_begin");
    int   rc;
    char *err = NULL;

    if (SOSD.db.ready == -1) {
        dlog(0, "ERROR: Attempted to begin a transaction during daemon shutdown.\n");
        return;
    }

    pthread_mutex_lock(SOSD.db.lock);

    dlog(6, " > > > > > > > > > > > > > > > > > > > > > > > > > > > > \n");
    dlog(6, ">>>>>>>>>> >> >> >> BEGIN TRANSACTION >> >> >> >>>>>>>>>>\n");
    dlog(6, " > > > > > > > > > > > > > > > > > > > > > > > > > > > > \n");
    rc = sqlite3_exec(database, sql_cmd_begin_transaction, NULL, NULL, &err);
    if (rc) { dlog(2, "##### ERROR ##### : (%d)\n", rc); }

    return;
}


void SOSD_db_transaction_commit() {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_transaction_commit");
    int   rc;
    char *err = NULL;

    dlog(6, " < < < < < < < < < < < < < < < < < < < < < < < < < < < <  \n");
    dlog(6, "<<<<<<<<<< << << << COMMIT TRANSACTION << << << <<<<<<<<<<\n");
    dlog(6, " < < < < < < < < < < < < < < < < < < < < < < < < < < < <  \n");
    rc = sqlite3_exec(database, sql_cmd_commit_transaction, NULL, NULL, &err);
    if (rc) { dlog(2, "##### ERROR ##### : (%d)\n", rc); }

    pthread_mutex_unlock(SOSD.db.lock);

    return;
}



void SOSD_db_handle_sosa_query(SOS_buffer *msg, SOS_buffer *response) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_handle_sosa_query");

    //pthread_mutex_lock(SOSD.db.lock);

    sqlite3 *sosa_conn;
    int flags = SQLITE_OPEN_READONLY;

    sqlite3_open_v2(SOSD.db.file, &sosa_conn, flags, "unix-none");

    SOS_msg_header   header;
    char            *sosa_query      = NULL;
    sqlite3_stmt    *sosa_statement  = NULL;

    int offset = 0;
    SOS_buffer_unpack(msg, &offset, "iigg",
        &header.msg_size,
        &header.msg_type,
        &header.msg_from,
        &header.pub_guid);

    SOS_buffer_unpack_safestr(msg, &offset, &sosa_query);

    int rc = 0;
    rc = sqlite3_prepare_v2( sosa_conn, sosa_query, -1, &sosa_statement, NULL);
    /*    if (rc != SQLITE_OK) {
    dlog(0, "ERROR: Unable to prepare statement for analytics(rank:%" SOS_GUID_FMT ")'s query:    (%d: %s)\n\n\t%s\n\n",
        header.msg_from, rc, sqlite3_errstr(rc), sosa_query);
        exit(EXIT_FAILURE);
        }*/

    dlog(7, "Building result set...\n");

    SOSA_results *results;
    SOSA_results_init(SOS, &results);

    int col = 0;
    int col_incoming = sqlite3_column_count( sosa_statement );

    dlog(7, "   ... col_incoming == %d\n", col_incoming);
    results->col_count = col_incoming;

    SOSA_results_grow_to(results, col_incoming, results->row_max);
    for (col = 0; col < col_incoming; col++) {
        dlog(7, "   ... results->col_names[%d] == \"%s\"\n", col, sqlite3_column_name(sosa_statement, col));
        SOSA_results_put_name(results, col, sqlite3_column_name(sosa_statement, col));
    }

    int row_incoming = 0;
    const char *val  = NULL;

    while( sqlite3_step( sosa_statement ) == SQLITE_ROW ) {
        dlog(7, "   ... results->data[%d][ | | | ... | ]\n", row_incoming);
        SOSA_results_grow_to(results, col_incoming, row_incoming);
        for (col = 0; col < col_incoming; col++) {
            val = (const char *) sqlite3_column_text(sosa_statement, col);
            SOSA_results_put(results, col, row_incoming, val);
        }//for:col
        row_incoming++;
        results->row_count = row_incoming;
    }//while:rows

    sqlite3_finalize(sosa_statement);
    sqlite3_close(sosa_conn);
    //pthread_mutex_unlock(SOSD.db.lock);

    SOSA_results_to_buffer(response, results);

    return;
}





void SOSD_db_insert_pub( SOS_pub *pub ) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_insert_pub");
    int i;

    if (pub->announced == 1) {
        dlog(1, "Skipping database insertion, this pub is already marked as announced.\n");

        SOSD_countof(db_insert_announce_nop++);
        return;
    }

    SOSD_countof(db_insert_announce++);

    pthread_mutex_lock( pub->lock );

    dlog(5, "Inserting pub(%" SOS_GUID_FMT ")->data into database(%s).\n", pub->guid, SOSD.db.file);

    /*
     *  NOTE: SQLite3 behaves strangely unless you pass it variables stored on the stack.
     */

    SOS_guid       guid              = pub->guid;
    char          *title             = pub->title;
    int            process_id        = pub->process_id;
    int            thread_id         = pub->thread_id;
    int            comm_rank         = pub->comm_rank;
    char          *node_id           = pub->node_id;
    char          *prog_name         = pub->prog_name;
    char          *prog_ver          = pub->prog_ver;
    int            meta_channel      = pub->meta.channel;
    __ENUM_C_TYPE  meta_nature       = __ENUM_VAL(pub->meta.nature, SOS_NATURE);
    __ENUM_C_TYPE  meta_layer        = __ENUM_VAL(pub->meta.layer, SOS_LAYER);
    __ENUM_C_TYPE  meta_pri_hint     = __ENUM_VAL(pub->meta.pri_hint, SOS_PRI);
    __ENUM_C_TYPE  meta_scope_hint   = __ENUM_VAL(pub->meta.scope_hint, SOS_SCOPE);
    __ENUM_C_TYPE  meta_retain_hint  = __ENUM_VAL(pub->meta.retain_hint, SOS_RETAIN);
    unsigned char *pragma            = pub->pragma_msg;
    int            pragma_len        = pub->pragma_len;
    unsigned char  pragma_empty[2];    memset(pragma_empty, '\0', 2);

    dlog(5, "  ... binding values into the statement\n");
    dlog(6, "     ... pragma_len = %d\n", pragma_len);
    dlog(6, "     ... pragma     = \"%s\"\n", pragma);

    CALL_SQLITE (bind_int64  (stmt_insert_pub, 1,  guid         ));
    CALL_SQLITE (bind_text   (stmt_insert_pub, 2,  title,            1 + strlen(title), SQLITE_STATIC  ));
    CALL_SQLITE (bind_int    (stmt_insert_pub, 3,  process_id   ));
    CALL_SQLITE (bind_int    (stmt_insert_pub, 4,  thread_id    ));
    CALL_SQLITE (bind_int    (stmt_insert_pub, 5,  comm_rank    ));
    CALL_SQLITE (bind_text   (stmt_insert_pub, 6,  node_id,          1 + strlen(node_id), SQLITE_STATIC  ));
    CALL_SQLITE (bind_text   (stmt_insert_pub, 7,  prog_name,        1 + strlen(prog_name), SQLITE_STATIC  ));
    CALL_SQLITE (bind_text   (stmt_insert_pub, 8,  prog_ver,         1 + strlen(prog_ver), SQLITE_STATIC  ));
    CALL_SQLITE (bind_int    (stmt_insert_pub, 9,  meta_channel ));
    __BIND_ENUM (stmt_insert_pub, 10, meta_nature  );
    __BIND_ENUM (stmt_insert_pub, 11, meta_layer   );
    __BIND_ENUM (stmt_insert_pub, 12, meta_pri_hint);
    __BIND_ENUM (stmt_insert_pub, 13, meta_scope_hint );
    __BIND_ENUM (stmt_insert_pub, 14, meta_retain_hint );
    if (pragma_len > 0) {
        /* Only bind the pragma if there actually is something to insert... */
        CALL_SQLITE (bind_text   (stmt_insert_pub, 15, (char const *) pragma,           pub->pragma_len, SQLITE_STATIC  ));
    } else {
        CALL_SQLITE (bind_text   (stmt_insert_pub, 15, (char const *) pragma_empty,    2, SQLITE_STATIC  ));
    }

    dlog(5, "  ... executing the query\n");

    CALL_SQLITE_EXPECT (step (stmt_insert_pub), DONE);  /* Execute the query. */

    dlog(5, "  ... success!  resetting the statement.\n");

    CALL_SQLITE (reset (stmt_insert_pub));
    CALL_SQLITE (clear_bindings (stmt_insert_pub));

    pthread_mutex_unlock( pub->lock );

    dlog(5, "  ... done.  returning to loop.\n");
    return;
}

/* tblVals : The snap_queue of a pub handle. */
/* NOTE: re_queue can be NULL, and snaps are then free()'ed. */
void SOSD_db_insert_vals( SOS_pipe *queue, SOS_pipe *re_queue ) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_insert_vals");
    SOS_val_snap **snap_list;
    int            snap_index;
    int            snap_count;
    int            count;

    dlog(5, "Flushing SOSD.db.snap_queue into database...\n");
    pthread_mutex_lock( queue->sync_lock );
    snap_count = queue->elem_count;
    if (queue->elem_count < 1) {
        dlog(5, "  ... nothing in the queue, returning.\n");
        pthread_mutex_unlock( queue->sync_lock);
        SOSD_countof(db_insert_val_snaps_nop++);
        return;
    }

    SOSD_countof(db_insert_val_snaps++);

    snap_list = (SOS_val_snap **) malloc(snap_count * sizeof(SOS_val_snap *));
    dlog(5, "  ... [bbb] grabbing %d snaps from the queue.\n", snap_count);
    count = pipe_pop_eager(queue->outlet, (void *) snap_list, snap_count);
    dlog(5, "      [bbb] %d snaps were returned from the queue on request for %d.\n", count, snap_count);
    queue->elem_count -= count;
    snap_count = count;

    if (queue->elem_count == 0) { 
      queue->sync_pending = 0;
    }
    dlog(5, "  ... [bbb] releasing queue->lock\n");
    pthread_mutex_unlock(queue->sync_lock);

    dlog(5, "  ... processing snaps extracted from the queue\n");

    int           elem;
    char         *val, *val_alloc;
    SOS_guid      guid;
    double        time_pack;
    double        time_send;
    double        time_recv;
    long          frame;
    __ENUM_C_TYPE semantic;
    __ENUM_C_TYPE mood;

    SOS_val_type      val_type;


    val_alloc = (char *) malloc(SOS_DEFAULT_STRING_LEN);

    for (snap_index = 0; snap_index < snap_count ; snap_index++) {

        elem              = snap_list[snap_index]->elem;
        guid              = snap_list[snap_index]->guid;
        time_pack         = snap_list[snap_index]->time.pack;
        time_send         = snap_list[snap_index]->time.send;
        time_recv         = snap_list[snap_index]->time.recv;
        frame             = snap_list[snap_index]->frame;
        semantic          = __ENUM_VAL( snap_list[snap_index]->semantic, SOS_VAL_SEMANTIC );
        mood              = __ENUM_VAL( snap_list[snap_index]->mood, SOS_MOOD );

        val_type = snap_list[snap_index]->type;
        SOS_TIME( time_recv );

        if (val_type != SOS_VAL_TYPE_STRING) {
            val = val_alloc;
            memset(val, '\0', SOS_DEFAULT_STRING_LEN);
        }


        switch (val_type) {
        case SOS_VAL_TYPE_INT:    snprintf(val, SOS_DEFAULT_STRING_LEN, "%d",  snap_list[snap_index]->val.i_val); break;
        case SOS_VAL_TYPE_LONG:   snprintf(val, SOS_DEFAULT_STRING_LEN, "%ld", snap_list[snap_index]->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE: snprintf(val, SOS_DEFAULT_STRING_LEN, "%.17lf", snap_list[snap_index]->val.d_val); break;
        case SOS_VAL_TYPE_STRING: val = snap_list[snap_index]->val.c_val; break; 
        default:
            dlog(5, "     ... error: invalid value type.  (%d)\n", val_type); break;
        }

        dlog(5, "     ... binding values\n");

        CALL_SQLITE (bind_int64  (stmt_insert_val, 1,  guid         ));
        if (val != NULL) {
            CALL_SQLITE (bind_text   (stmt_insert_val, 2,  val, 1 + strlen(val), SQLITE_STATIC ));
        } else {
            CALL_SQLITE (bind_text   (stmt_insert_val, 2,  "", 1, SQLITE_STATIC ));
        }
        CALL_SQLITE (bind_int    (stmt_insert_val, 3,  frame        ));
        __BIND_ENUM (stmt_insert_val, 4,  semantic     );
        __BIND_ENUM (stmt_insert_val, 5,  mood         );
        CALL_SQLITE (bind_double (stmt_insert_val, 6,  time_pack    ));
        CALL_SQLITE (bind_double (stmt_insert_val, 7,  time_send    ));
        CALL_SQLITE (bind_double (stmt_insert_val, 8,  time_recv    ));

        dlog(5, "     ... executing the query\n");

        CALL_SQLITE_EXPECT (step (stmt_insert_val), DONE);  /* Execute the query. */
        
        dlog(5, "     ... success!  resetting the statement.\n");

        CALL_SQLITE (reset (stmt_insert_val));
        CALL_SQLITE (clear_bindings (stmt_insert_val));

        dlog(5, "     ... grabbing the next snap.\n");
    }

    if (re_queue == NULL) {
        // Roll through and free the snaps.
        for (snap_index = 0; snap_index < snap_count ; snap_index++) {
            switch(val_type) {
            case SOS_VAL_TYPE_STRING: free(snap_list[snap_index]->val.c_val); break;
            case SOS_VAL_TYPE_BYTES:  free(snap_list[snap_index]->val.bytes); break;
            default: break;
            }
            free(snap_list[snap_index]);
        }
    } else {
       // Inject this snap queue into the next one en masse.
       pthread_mutex_lock(re_queue->sync_lock);
       pipe_push(re_queue->intake, (void *) snap_list, snap_count);
       re_queue->elem_count += snap_count;
       pthread_mutex_unlock(re_queue->sync_lock);
    }

    free(snap_list);
    free(val_alloc);

    dlog(5, "  ... done.  returning to loop.\n");

    return;
}

/* tblData : Data definitions / metadata that comes with a SOS_publish() call. */
void SOSD_db_insert_data( SOS_pub *pub ) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_insert_data");
    int i;
    int inserted_count = 0;

    pthread_mutex_lock( pub->lock );
    pub->sync_pending = 0;

    dlog(5, "Inserting pub(%" SOS_GUID_FMT ")->data into database(%s).\n", pub->guid, SOSD.db.file);

    inserted_count = 0;
    for (i = 0; i < pub->elem_count; i++) {

        if (pub->data[i]->sync != SOS_VAL_SYNC_RENEW) {
            dlog(1, "Skipping pub->data[%d]->sync == %s\n", i, SOS_ENUM_STR(pub->data[i]->sync, SOS_VAL_SYNC));
            continue;
        } else {
            inserted_count++;
        }


        /*
         *  NOTE: SQLite3 behaves strangely unless you pass it variables stored on the stack.
         */
        SOS_guid      pub_guid          = pub->guid;
        SOS_guid      guid              = pub->data[i]->guid;
        const char   *name              = pub->data[i]->name;
        char         *val;
        __ENUM_C_TYPE val_type          = __ENUM_VAL( pub->data[i]->type, SOS_VAL_TYPE );
        __ENUM_C_TYPE meta_freq         = __ENUM_VAL( pub->data[i]->meta.freq, SOS_VAL_FREQ );
        __ENUM_C_TYPE meta_class        = __ENUM_VAL( pub->data[i]->meta.classifier, SOS_VAL_CLASS );
        __ENUM_C_TYPE meta_pattern      = __ENUM_VAL( pub->data[i]->meta.pattern, SOS_VAL_PATTERN );
        __ENUM_C_TYPE meta_compare      = __ENUM_VAL( pub->data[i]->meta.compare, SOS_VAL_COMPARE );

        char          val_num_as_str[SOS_DEFAULT_STRING_LEN];
        memset( val_num_as_str, '\0', SOS_DEFAULT_STRING_LEN);

        switch (pub->data[i]->type) {
        case SOS_VAL_TYPE_INT:    val = val_num_as_str; snprintf(val, SOS_DEFAULT_STRING_LEN, "%d",  pub->data[i]->val.i_val); break;
        case SOS_VAL_TYPE_LONG:   val = val_num_as_str; snprintf(val, SOS_DEFAULT_STRING_LEN, "%ld", pub->data[i]->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE: val = val_num_as_str; snprintf(val, SOS_DEFAULT_STRING_LEN, "%lf", pub->data[i]->val.d_val); break;
        case SOS_VAL_TYPE_STRING: val = pub->data[i]->val.c_val; break;
        default:
            dlog(5, "ERROR: Attempting to insert an invalid data type.  (%d)  Continuing...\n", pub->data[i]->type);
            break;
            continue;
        }

        CALL_SQLITE (bind_int64  (stmt_insert_data, 1,  pub_guid     ));
        CALL_SQLITE (bind_int64  (stmt_insert_data, 2,  guid         ));
        CALL_SQLITE (bind_text   (stmt_insert_data, 3,  name,             1 + strlen(name), SQLITE_STATIC     ));
        __BIND_ENUM (stmt_insert_data, 4,  val_type     );
        __BIND_ENUM (stmt_insert_data, 5,  meta_freq    );
        __BIND_ENUM (stmt_insert_data, 6,  meta_class   );
        __BIND_ENUM (stmt_insert_data, 7,  meta_pattern );
        __BIND_ENUM (stmt_insert_data, 8,  meta_compare );

        dlog(5, "  ... executing insert query   pub->data[%d].(%s)\n", i, pub->data[i]->name);

        CALL_SQLITE_EXPECT (step (stmt_insert_data), DONE);

        dlog(6, "  ... success!  resetting the statement.\n");
        CALL_SQLITE (reset(stmt_insert_data));
        CALL_SQLITE (clear_bindings (stmt_insert_data));

        pub->data[i]->sync = SOS_VAL_SYNC_LOCAL;
    }
    
    dlog(5, "  ... done.  returning to loop.\n");

    if (inserted_count > 0) {
        SOSD_countof(db_insert_publish++);
    } else {
        SOSD_countof(db_insert_publish_nop++);
    }

    pthread_mutex_unlock( pub->lock );

    return;
}




void SOSD_db_insert_enum(const char *var_type, const char **var_name, int var_max_index) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_insert_enum");

    int i;
    const char *type = var_type;
    const char *name;

    for (i = 0; i < var_max_index; i++) {
        int pos = i;
        name = var_name[i];

        CALL_SQLITE (bind_text   (stmt_insert_enum, 1,  type,         1 + strlen(type), SQLITE_STATIC     ));
        CALL_SQLITE (bind_text   (stmt_insert_enum, 2,  name,         1 + strlen(name), SQLITE_STATIC     ));
        CALL_SQLITE (bind_int    (stmt_insert_enum, 3,  pos ));

        dlog(2, "  --> SOS_%s_string[%d] == \"%s\"\n", type, i, name);

        CALL_SQLITE_EXPECT (step (stmt_insert_enum), DONE);

        CALL_SQLITE (reset(stmt_insert_enum));
        CALL_SQLITE (clear_bindings (stmt_insert_enum));

    }

    return;
}



void SOSD_db_create_tables(void) {
    SOS_SET_CONTEXT(SOSD.sos_context, "SOSD_db_create_tables");
    int rc;
    char *err = NULL;

    dlog(1, "Creating tables in the database...\n");

    rc = sqlite3_exec(database, sql_create_table_pubs, NULL, NULL, &err);
    if( err != NULL ) {
        dlog(0, "ERROR!  Can't create " SOSD_DB_PUBS_TABLE_NAME " in the database!  (%s)\n", err);
        sqlite3_close(database); exit(EXIT_FAILURE);
    } else {
        dlog(0, "  ... Created: %s\n", SOSD_DB_PUBS_TABLE_NAME);
    }

    rc = sqlite3_exec(database, sql_create_table_data, NULL, NULL, &err);
    if( err != NULL ) {
        dlog(0, "ERROR!  Can't create " SOSD_DB_DATA_TABLE_NAME " in the database!  (%s)\n", err);
        sqlite3_close(database); exit(EXIT_FAILURE);
    } else {
        dlog(0, "  ... Created: %s\n", SOSD_DB_DATA_TABLE_NAME);
    }

    rc = sqlite3_exec(database, sql_create_table_vals, NULL, NULL, &err);
    if( err != NULL ) {
        dlog(0, "ERROR!  Can't create " SOSD_DB_VALS_TABLE_NAME " in the database!  (%s)\n", err);
        sqlite3_close(database); exit(EXIT_FAILURE);
    } else {
        dlog(0, "  ... Created: %s\n", SOSD_DB_VALS_TABLE_NAME);
    }

    rc = sqlite3_exec(database, sql_create_table_enum, NULL, NULL, &err);
    if ( err != NULL ) {
        dlog(0, "ERROR!  Can't create " SOSD_DB_ENUM_TABLE_NAME " in the database!  (%s)\n", err);
        sqlite3_close(database); exit(EXIT_FAILURE);
    } else {
        dlog(0, "  ... Created: %s\n", SOSD_DB_ENUM_TABLE_NAME);
    }

    rc = sqlite3_exec(database, sql_create_table_sosd_config, NULL, NULL, &err);
    if ( err != NULL ) {
        dlog(0, "ERROR!  Can't create " SOSD_DB_SOSD_TABLE_NAME " in the database!  (%s)\n", err);
        sqlite3_close(database); exit(EXIT_FAILURE);
    } else {
        dlog(0, "  ... Created: %s\n", SOSD_DB_SOSD_TABLE_NAME);
    }

    dlog(1, "  ... done.\n");
    return;



}

