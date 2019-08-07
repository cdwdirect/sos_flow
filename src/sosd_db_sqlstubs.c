
#include <sqlite3.h>
#include "sos.h"

sqlite3 *database;


static int max_pub_id = 0;
static int callback(void *NotUsed, int argc, char **argv, char **azColName){
    return 0;
}

void SOS_db_init_database() { return; }
void SOS_db_insert_pub( SOS_pub *pub ) { return; }
void SOS_db_insert_data( SOS_pub *pub ) { return; }
void SOS_db_create_tables(void) { return; }
