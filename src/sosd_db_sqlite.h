#ifndef SOS_DB_SQLITE_H
#define SOS_DB_SQLITE_H

/*
 * sos_db_sqlite.h           SOS database module SQLite support.
 */

#include <sqlite3.h>

#include "sos.h"
#include "sos_debug.h"
#include "sosd.h"
#include "sosa.h"


/* Required if included by C++ code. */
#ifdef __cplusplus
extern "C" {
#endif

void SOSD_db_init_database(void);
void SOSD_db_close_database(void);
void SOSD_db_create_tables(void);
void SOSD_db_insert_pub(SOS_pub *pub);
void SOSD_db_insert_data(SOS_pub *pub);
void SOSD_db_insert_vals(SOS_pipe *from_queue, SOS_pipe *optional_re_queue);
void SOSD_db_transaction_begin(void);
void SOSD_db_transaction_commit(void);
void SOSD_db_handle_sosa_query(SOS_buffer *msg, SOS_buffer *response);

#define CALL_SQLITE(f) {						\
    int i;								\
    int retry;                                                          \
    int retry_count = 0;						\
    do {								\
        i = sqlite3_ ## f;                                              \
        retry = 0;                                                      \
        if (i == SQLITE_BUSY) {                                         \
            retry = 1;                                                  \
            usleep(20);                                                 \
        } else if (i != SQLITE_OK) {                                    \
            fprintf(stderr, "%s failed with status %d: %s\n",           \
                    #f, i, sqlite3_errmsg(database));                   \
        }                                                               \
    } while (retry);                                                    \
 }                                                                      \


#define CALL_SQLITE_EXPECT(f,x) {	                                \
    int i;                                                              \
    i = sqlite3_ ## f;                                                  \
        if (i != SQLITE_ ## x) {                                        \
            fprintf (stderr, "%s failed with status %d: %s\n",          \
                    #f, i, sqlite3_errmsg (database));                  \
            exit (1);                                                   \
        }								\
 }                                                                      \
    






#ifdef __cplusplus
}
#endif
  
#endif //SOS_DB_SQLITE_H
