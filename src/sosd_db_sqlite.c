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

#include <sqlite3.h>

#include "sos.h"
#include "sos_debug.h"
#include "sosd_db_sqlite.h"


#define SOSD_DB_PUB_TABLE_NAME  "tblPubs"
#define SOSD_DB_DATA_TABLE_NAME "tblData"


sqlite3 *database;



/* At the moment, this is not used anywhere... -CW */
static int max_pub_id = 0;
static int callback(void *NotUsed, int argc, char **argv, char **azColName){
    SOS_SET_WHOAMI(whoami, "{sosd_db_sqlite}.callback");
    int i;

    for(i=0; i<argc; i++){
        //printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
        if (strcmp(azColName[i], "pub_id") == 0) max_pub_id = atoi(argv[i]);
    }
    printf("\n");
    return 0;
}



void SOSD_db_init_database() {
    SOS_SET_WHOAMI(whoami, "SOSD_db_init_database");
    int     retval;
    int     flags;

    dlog(1, "[%s]: Opening database...\n", whoami);
    
    flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    snprintf(SOSD.db_file, SOS_DEFAULT_STRING_LEN, "%s/sosd.db", SOSD.work_dir);

    /*
     *   In unix-none mode, the database is accessible from multiple processes
     *   but you need to do (not currently in our code) explicit calls to SQLite
     *   locking functions (need to find out what they are.)   -CW
     *
     *   "unix-none"     =no locking (NOP's)
     *   "unix-excl"     =single-access only
     *   "unix-dotfile"  =uses a file as the lock.
     */

    retval = sqlite3_open_v2(SOSD.db_file, &database, flags, "unix-dotfile");
    if( retval ){
        dlog(0, "[%s]: ERROR!  Can't open database: %s   (%s)\n", whoami, SOSD.db_file, sqlite3_errmsg(database));
        sqlite3_close(database);
        exit(EXIT_FAILURE);
    } else {
        dlog(1, "[%s]: Successfully opened database.\n", whoami);
    }

    return;
}


void SOSD_db_close_database() {

    SOSD.db_ready = 0;
    sqlite3_close_v2(database);

    return;
}


void SOSD_db_insert_pub( SOS_pub *pub ) {
    SOS_SET_WHOAMI(whoami, "SOSD_db_insert_dirty");
    int i;
    sqlite3_stmt *stmt;

    char *sql_pub = "INSERT INTO " SOSD_DB_PUB_TABLE_NAME " ("   \
        " guid,"                                                \
        " title,"                                               \
        " process_id,"                                          \
        " thread_id,"                                           \
        " comm_rank,"                                           \
        " node_id,"                                             \
        " prog_name,"                                           \
        " prog_ver,"                                            \
        " meta_channel,"                                        \
        " meta_nature,"                                         \
        " meta_layer,"                                          \
        " meta_pri_hint,"                                       \
        " meta_scope_hint,"                                     \
        " meta_retain_hint,"                                    \
        " pragma "                                              \
        ") VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);";

    dlog(5, "[%s]: Inserting pub(%ld)->data[] into database(%s).\n", whoami, pub->guid, SOSD.db_file);
    dlog(5, "[%s]:   ... preparing sql statement\n", whoami);

    while (!SOSD.db_ready) { ; }

    CALL_SQLITE (prepare(database, sql_pub, strlen(sql_pub) + 1, &stmt, NULL));

    /*
     *  NOTE: SQLite3 behaves strangely unless you pass it variables stored on the stack.
     */

    long          guid              = pub->guid;
    char         *title             = pub->title;
    int           process_id        = pub->process_id;
    int           thread_id         = pub->thread_id;
    int           comm_rank         = pub->comm_rank;
    char         *node_id           = pub->node_id;
    char         *prog_name         = pub->prog_name;
    char         *prog_ver          = pub->prog_ver;
    int           meta_channel      = pub->meta.channel;
    const char   *meta_nature       = SOS_ENUM_STR(pub->meta.nature, SOS_NATURE);
    const char   *meta_layer        = SOS_ENUM_STR(pub->meta.layer, SOS_LAYER);
    const char   *meta_pri_hint     = SOS_ENUM_STR(pub->meta.pri_hint, SOS_PRI);
    const char   *meta_scope_hint   = SOS_ENUM_STR(pub->meta.scope_hint, SOS_SCOPE);
    const char   *meta_retain_hint  = SOS_ENUM_STR(pub->meta.retain_hint, SOS_RETAIN);
    char         *pragma            = pub->pragma_msg;
    int           pragma_len        = pub->pragma_len;
    char          pragma_empty      = '\0';

    dlog(5, "[%s]:   ... binding values into the statement\n", whoami);
    dlog(6, "[%s]:      ... pragma_len = %d\n", whoami, pragma_len);
    dlog(6, "[%s]:      ... pragma     = \"%s\"\n", whoami, pragma);

    CALL_SQLITE (bind_int    (stmt, 1,  guid         ));
    CALL_SQLITE (bind_text   (stmt, 2,  title,            1 + strlen(title), SQLITE_STATIC  ));
    CALL_SQLITE (bind_int    (stmt, 3,  process_id   ));
    CALL_SQLITE (bind_int    (stmt, 4,  thread_id    ));
    CALL_SQLITE (bind_int    (stmt, 5,  comm_rank    ));
    CALL_SQLITE (bind_text   (stmt, 6,  node_id,          1 + strlen(node_id), SQLITE_STATIC  ));
    CALL_SQLITE (bind_text   (stmt, 7,  prog_name,        1 + strlen(prog_name), SQLITE_STATIC  ));
    CALL_SQLITE (bind_text   (stmt, 8,  prog_ver,         1 + strlen(prog_ver), SQLITE_STATIC  ));
    CALL_SQLITE (bind_int    (stmt, 9,  meta_channel ));
    CALL_SQLITE (bind_text   (stmt, 10, meta_nature,      1 + strlen(meta_nature), SQLITE_STATIC  ));
    CALL_SQLITE (bind_text   (stmt, 11, meta_layer,       1 + strlen(meta_layer), SQLITE_STATIC  ));
    CALL_SQLITE (bind_text   (stmt, 12, meta_pri_hint,    1 + strlen(meta_layer), SQLITE_STATIC  ));
    CALL_SQLITE (bind_text   (stmt, 13, meta_scope_hint,  1 + strlen(meta_scope_hint), SQLITE_STATIC  ));
    CALL_SQLITE (bind_text   (stmt, 14, meta_retain_hint, 1 + strlen(meta_retain_hint), SQLITE_STATIC  ));
    if (pragma_len > 0) {
        /* Only bind the pragma is there actually is something to insert... */
        CALL_SQLITE (bind_text   (stmt, 15, pragma,           pub->pragma_len, SQLITE_STATIC  ));
    } else {
        CALL_SQLITE (bind_text   (stmt, 15, &pragma_empty,    1, SQLITE_STATIC  ));
    }

    dlog(5, "[%s]:   ... executing the query\n", whoami);

    CALL_SQLITE_EXPECT (step (stmt), DONE);  /* Execute the query. */

    dlog(5, "[%s]:   ... success!  resetting the statement.\n", whoami);

    CALL_SQLITE (reset (stmt));
    CALL_SQLITE (clear_bindings (stmt));
    CALL_SQLITE (finalize(stmt));

    dlog(5, "[%s]:   ... done.  returning to loop.\n", whoami);
    return;
}




void SOSD_db_insert_data( SOS_pub *pub ) {
    SOS_SET_WHOAMI(whoami, "SOSD_db_insert_data");
    int i;
    sqlite3_stmt *stmt;

    char *sql_data = "INSERT INTO " SOSD_DB_DATA_TABLE_NAME " (" \
        " pub_guid,"                                            \
        " guid,"                                                \
        " sem_hint,"                                            \
        " time_pack,"                                           \
        " time_send,"                                           \
        " time_recv,"                                           \
        " val_type,"                                            \
        " val "                                                 \
        ") VALUES (?,?,?,?,?,?,?,?);";

    dlog(5, "[%s]: Inserting pub(%ld)->data[] into database(%s).\n", whoami, pub->guid, SOSD.db_file);
    dlog(6, "[%s]:   ... preparing sql statement: \n", whoami);

    while (!SOSD.db_ready) { ; }

    CALL_SQLITE (prepare(database, sql_data, strlen(sql_data) + 1, &stmt, NULL));


    for (i = 0; i < pub->elem_count; i++) {
        if (pub->data[i]->state != SOS_VAL_STATE_DIRTY) continue;
        /*
         *  NOTE: SQLite3 behaves strangely unless you pass it variables stored on the stack.
         */
        long          pub_guid          = pub->guid;
        long          guid              = pub->data[i]->guid;
        const char   *sem_hint          = SOS_ENUM_STR( pub->data[i]->sem_hint, SOS_SEM );
        double        time_pack         = pub->data[i]->time.pack;
        double        time_send         = pub->data[i]->time.send;
        double        time_recv         = pub->data[i]->time.recv;
        const char   *val_type          = SOS_ENUM_STR( pub->data[i]->type, SOS_VAL_TYPE );
        char         *val;
        char          val_num_as_str[SOS_DEFAULT_STRING_LEN];
        memset( val_num_as_str, '\0', SOS_DEFAULT_STRING_LEN);

        switch (pub->data[i]->type) {
        case SOS_VAL_TYPE_INT:    val = val_num_as_str; snprintf(val, SOS_DEFAULT_STRING_LEN, "%d",  pub->data[i]->val.i_val); break;
        case SOS_VAL_TYPE_LONG:   val = val_num_as_str; snprintf(val, SOS_DEFAULT_STRING_LEN, "%ld", pub->data[i]->val.l_val); break;
        case SOS_VAL_TYPE_DOUBLE: val = val_num_as_str; snprintf(val, SOS_DEFAULT_STRING_LEN, "%lf", pub->data[i]->val.d_val); break;
        case SOS_VAL_TYPE_STRING: val = pub->data[i]->val.c_val; break;
        }

        dlog(6, "[%s]:   ... binding values into the statement\n", whoami);
        dlog(6, "[%s]:      ... sem_hint = \"%s\"\n", whoami, sem_hint);
        dlog(6, "[%s]:      ... val_type = \"%s\"\n", whoami, val_type);
        dlog(6, "[%s]:      ... val      = \"%s\"\n", whoami, val     );

        
        CALL_SQLITE (bind_int    (stmt, 1,  pub_guid     ));
        CALL_SQLITE (bind_int    (stmt, 2,  guid         ));
        CALL_SQLITE (bind_text   (stmt, 3,  sem_hint,         1 + strlen(sem_hint), SQLITE_STATIC  ));
        CALL_SQLITE (bind_double (stmt, 4,  time_pack    ));
        CALL_SQLITE (bind_double (stmt, 5,  time_send    ));
        CALL_SQLITE (bind_double (stmt, 6,  time_recv    ));
        CALL_SQLITE (bind_text   (stmt, 7,  val_type,         1 + strlen(val_type), SQLITE_STATIC  ));
        CALL_SQLITE (bind_text   (stmt, 8,  val,              1 + strlen(val), SQLITE_STATIC  ));
        
        dlog(5, "[%s]:   ... executing insert query   pub->data[%d].(%s)\n", whoami, i, pub->data[i]->name);

        CALL_SQLITE_EXPECT (step (stmt), DONE);

        dlog(6, "[%s]:   ... success!  resetting the statement.\n", whoami);
        CALL_SQLITE (reset (stmt));
        CALL_SQLITE (clear_bindings (stmt));

        pub->data[i]->state = SOS_VAL_STATE_CLEAN;
    }

    CALL_SQLITE (finalize(stmt));
    
    dlog(5, "[%s]:   ... done.  returning to loop.\n", whoami);
    
    return;
}




void SOSD_db_create_tables(void) {
    SOS_SET_WHOAMI(whoami, "SOSD_db_create_tables");
    int rc;
    char *err = NULL;

    const char *pub_table = ""
        "CREATE TABLE IF NOT EXISTS " SOSD_DB_PUB_TABLE_NAME " ( "
        " row_id            INTEGER PRIMARY KEY,   "
        " guid              INTEGER,     "
        " title             STRING,      "
        " process_id        INTEGER,     "
        " thread_id         INTEGER,     "
        " comm_rank         INTEGER,     "
        " node_id           STRING,      "
        " prog_name         STRING,      "
        " prog_ver          STRING,      "
        " meta_channel      INTEGER,     "
        " meta_nature       STRING,      "
        " meta_layer        STRING,      "
        " meta_pri_hint     STRING,      "
        " meta_scope_hint   STRING,      "
        " meta_retain_hint  STRING,      "
        " pragma            STRING    ); ";

    const char *data_table = ""
        "CREATE TABLE IF NOT EXISTS " SOSD_DB_DATA_TABLE_NAME " ( "
        " row_id            INTEGER PRIMARY KEY,   "
        " pub_guid          INTEGER,     "
        " guid              INTEGER,     "
        " val               STRING,      "
        " val_type          STRING,      "
        " sem_hint          STRING,      "
        " time_pack         DOUBLE,      "
        " time_send         DOUBLE,      "
        " time_recv         DOUBLE    ); ";

    dlog(1, "[%s]: Creating tables in the database...\n", whoami);

    rc = sqlite3_exec(database, pub_table, NULL, NULL, &err);
    if( err != NULL ){
        dlog(0, "[%s]: ERROR!  Can't create pub_table in the database!  (%s)\n", whoami, err);
        sqlite3_close(database); exit(EXIT_FAILURE);
    }

    rc = sqlite3_exec(database, data_table, NULL, NULL, &err);
    if( err != NULL ){
        dlog(0, "[%s]: ERROR!  Can't create pub_table in the database!  (%s)\n", whoami, err);
        sqlite3_close(database); exit(EXIT_FAILURE);
    }

    dlog(1, "[%s]:   ... Done creating tables!\n", whoami);
    return;
}






/*
void SOSD_db_update_values( SOS_pub *pub ) {
    SOS_SET_WHOAMI(whoami, "SOSD_db_update_values");
    int end_index;
    int i;
    char sql[SOS_DEFAULT_STRING_LEN];
    sqlite3_stmt * stmt;

    memset(sql, '\0', SOS_DEFAULT_STRING_LEN);
    snpritnf(sql, SOS_DEFAULT_STRING_LEN, "UPDATE sos_data set numeric_value = ? where pub_id = ? and rank = ?;");
    end_index = pub->elem_count;

    CALL_SQLITE (prepare_v2 (database, sql, strlen (sql) + 1, & stmt, NULL));
    for (i = 0 ; i < end_index ; i ++ ) {
        if (pub->data[i]->dirty) {
            //printf ("New value: %d, %d, %f\n", pub->data[i]->id, pub->origin_rank, pub->data[i]->val.d_val );
            int tmpid = pub->data[i]->id; // for some reason, the binding needs a straight up int
            double tmpval = pub->data[i]->val.d_val;
            CALL_SQLITE (bind_double (stmt, 1, tmpval));
            CALL_SQLITE (bind_int (stmt, 2, tmpid));
            CALL_SQLITE (bind_int (stmt, 3, pub->origin_rank));
            CALL_SQLITE_EXPECT (step (stmt), DONE);
            CALL_SQLITE (reset (stmt));
        }
    }
    CALL_SQLITE (clear_bindings (stmt));
}


void SOSD_db_print_max_value ( void ) {
    SOS_SET_WHOAMI(whoami, "SOSD_db_print_max_value");
    //const char * query = "select pub_id, rank, tool_name, thread, key_type, key_name, numeric_value from sos_data where tool_name like 'TAU' and key_type like 'exclusive_TIME' order by numeric_value desc LIMIT 3;";
    const char * query = "select pub_id, rank, tool_name, thread, key_type, key_name, max(numeric_value) from sos_data where tool_name like 'TAU' and key_type like 'exclusive_TIME';";
    char *err = NULL;
    int rc = sqlite3_exec ( database, query, callback, NULL, &err);
    if( err != NULL ){
        fprintf(stderr, "Can't query database: %s\n", err);
        sqlite3_close(database);
    }
    const char * sql = "select min(numeric_value), avg(numeric_value), max(numeric_value), key_name from sos_data where pub_id like ?;";
    sqlite3_stmt * stmt;
    CALL_SQLITE (prepare_v2 (database, sql, strlen (sql) + 1, & stmt, NULL));
    CALL_SQLITE (bind_int (stmt, 1, max_pub_id));
    CALL_SQLITE_EXPECT (step (stmt), ROW);
    int bytes;
    double min;
    double avg;
    double max;
    const unsigned char * name;
    bytes = sqlite3_column_bytes(stmt, 0);
    min  = sqlite3_column_double (stmt, 0);
    avg  = sqlite3_column_double (stmt, 1);
    max  = sqlite3_column_double (stmt, 2);
    name  = sqlite3_column_text (stmt, 3);
    int row = 0;
  
    dlog(4, "min: %f, avg: %f, max: %f, name: %s\n", min, avg, max, name);
    if( err != NULL ){
        fprintf(stderr, "Can't query database: %s\n", err);
        sqlite3_close(database);
    }
}
*/


