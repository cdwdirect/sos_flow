#ifndef SOS_DB_SQLITE_H
#define SOS_DB_SQLITE_H

/*
 * sos_db_sqlite.h           SOS database module SQLite support.
 */

#include <sqlite3.h>

#include "sos.h"
#include "sos_debug.h"
#include "sosd.h"


/* Required if included by C++ code. */
#ifdef __cplusplus
extern "C" {
#endif


void SOS_db_init_database(void);
void SOS_db_close_database();
void SOS_db_create_tables(void);
void SOS_db_insert_announcement( SOS_pub *pub );
void SOS_db_update_values( SOS_pub *pub );
void SOS_db_print_max_value(void);



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
