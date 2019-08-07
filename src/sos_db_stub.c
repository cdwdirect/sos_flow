
/*
 * sos_db_stub.c           SOS stub database 
 *
 *   NOTE: .CSV output, init() and insert() only.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#include "sos_db.h"
#include "sos_debug.h"

#define NUM_AS_STR_MAX 48
#define INSERT_MAX (SOS_DEFAULT_STRING_LEN * 10)

#ifndef SOS_DB_CSV_FILEHANDLES_EXISTS
#define SOS_DB_CSV_FILEHANDLES_EXISTS

  char SOS_DB_FILE_CSV[SOS_DEFAULT_STRING_LEN];
//char SOS_DB_FILE_ROW[SOS_DEFAULT_STRING_LEN];
//char SOS_DB_FILE_LCK[SOS_DEFAULT_STRING_LEN];

  FILE *SOS_db_csv_fp;
  FILE *SOS_db_row_fp;
  FILE *SOS_db_lck_fp;

  char SOS_db_insert_stmt[ INSERT_MAX ];
  char SOS_db_last_row[sizeof(int) + 1];
  int  SOS_db_new_row_id;

  char *SOS_type_str[4] = { "SOS_INT", "SOS_LONG", "SOS_DOUBLE", "SOS_STRING" };

  int CURRENT_ROW;
  int errsv;
  int write_iter;

#endif


/* Stub functions that create a .CSV file... */
void SOS_db_init_database();
void SOS_db_insert_dirty( SOS_pub_handle * pub );
void SOS_db_shutdown();

/* Not used... */
void SOS_db_create_tables();
void SOS_db_update_values( SOS_pub_handle * pub );
void SOS_db_print_max_value();

#define CALL_SQLITE(__f)          { dlog(1, "[db_stub(%d)]: SQL database operations are disabled for the stub database.\n", SOS_RANK); }
#define CALL_SQLITE_EXPECT(__f,__x) { dlog(1, "[db_stub(%d)]: SQL database operations are disabled for the stub database.\n", SOS_RANK); }


void SOS_db_init_database( void ) {
  int retval;
  int flags;

  CURRENT_ROW = 0;
 
  sprintf(SOS_DB_FILE_CSV, "%s.%d.csv", SOS_DATABASE_FILE, SOS_RANK);
  //sprintf(SOS_DB_FILE_LCK, "%s.%d.lock", SOS_DATABASE_FILE, SOS_RANK);
  //sprintf(SOS_DB_FILE_ROW, "%s.%d.last", SOS_DATABASE_FILE, SOS_RANK);

  dlog(1, "[db_stub(%d)]: Opening (stub) database: %s\n", SOS_RANK, SOS_DB_FILE_CSV);

  errno = 0;
  /* Create a blank database file to make sure we're able to do so. */
  if ((SOS_db_csv_fp = fopen(SOS_DB_FILE_CSV, "w+")) == NULL) {
    errsv = errno;
    dlog(0, "[db_stub(%d).init_database]: ERROR!! Could not open the database file (CREATE): %s (%d, %s)\n", SOS_RANK, SOS_DB_FILE_CSV, errsv, strerror(errsv));
    return;
  }

  retval = fclose(SOS_db_csv_fp);

  dlog(1, "[db_stub(%d)]: SOS_db_init_database() complete.\n", SOS_RANK);
  
  return;
}



void SOS_db_insert_dirty( SOS_pub_handle *pub ) {
  int i;
  int flags;
  int retval;
  
  /* Open the database for append. */
  if ((SOS_db_csv_fp = fopen(SOS_DB_FILE_CSV, "a")) == NULL) {
    errsv = errno;
    if (errsv != 0) {
      dlog(0, "[db_stub(%d).insert_dirty]: ERROR!! Could not open the database file (APPEND): %s (%d, %s)\n", SOS_RANK, SOS_DB_FILE_CSV, errsv, strerror(errsv));
      return;
    }
  }

  if (strncmp(pub->data[0]->name, "TAU::", 5) == 0) {
    
    /* ************************************/
    /* *** TAU : Profiling information... */
    /* ************************************/
    
    dlog(5, "[db_stub(%d).insert_dirty]: TAU data received.\n", SOS_RANK);
    
    for (i = 0; i < pub->elem_count ; i++) {
      if (pub->data[i]->dirty == 0) continue;
      
      char empty[] = "---";

      char *tmp_name;

      tmp_name = (char*) malloc(sizeof(char) * strlen(pub->data[i]->name));
      memset(tmp_name, '\0', strlen(pub->data[i]->name));
      memcpy(tmp_name, pub->data[i]->name, strlen(pub->data[i]->name));

      //dlog(1, "[db_stub(%d).insert_dirty]: pub->data[%d]->name=%s\n", SOS_RANK, i, pub->data[i]->name);

      char* tool_name = strtok(tmp_name, "::");
      char* thread = strtok(NULL, "::");
      char* key_type = strtok(NULL, "::");
      char* key_name = strtok(NULL, "::");
      int tmp_puid = pub->origin_puid;
      char* tau_name = "TAU";
      char* val_str;
      char num_as_str[ NUM_AS_STR_MAX ];

      dlog(1, "[db_stub(%d).insert_dirty]: tool_name:%s thread:%s key_type:%s key_name:%s\n", SOS_RANK, tool_name, thread, key_type, key_name);

      SOS_db_strip_str(key_type);
      SOS_db_strip_str(key_name);

      memset(num_as_str, '\0', NUM_AS_STR_MAX);
      switch (pub->data[i]->type) {
      case SOS_STRING : val_str = pub->data[i]->val.c_val; SOS_db_strip_str(val_str); break;
      case SOS_INT :	snprintf(num_as_str, NUM_AS_STR_MAX, "%d", pub->data[i]->val.i_val); break;
      case SOS_LONG :	snprintf(num_as_str, NUM_AS_STR_MAX, "%ld", pub->data[i]->val.l_val); break;
      case SOS_DOUBLE :	snprintf(num_as_str, NUM_AS_STR_MAX, "%lf", pub->data[i]->val.d_val); break;
      }; if (pub->data[i]->type != SOS_STRING) val_str = num_as_str;

      sprintf(SOS_db_insert_stmt, "%d,%d,%d,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%lf,%lf,%lf,\"%s\"\n",
	      /*AUTO*/ CURRENT_ROW++,
	      /*  1 */ pub->origin_puid,
	      /*  2 */ pub->origin_rank,
	      /*  3 */ pub->origin_prog,
	      /*  4 */ tau_name,
	      /*  5 */ thread,
	      /*  6 */ key_type,
	      /*  7 */ key_name,
	      /*  8 */ pub->data[i]->pack_ts,
	      /*  9 */ pub->data[i]->send_ts,
	      /* 10 */ pub->data[i]->recv_ts,
	      /* 11 */ val_str );

      fwrite(SOS_db_insert_stmt, strlen(SOS_db_insert_stmt), 1, SOS_db_csv_fp);

      pub->data[i]->dirty = 0;
    } //end: foreach dirty record (TAU)

  } else {

      /* **********************************/
      /* ***  SOS PUB : Generic pub data. */
      /* **********************************/
      
      dlog(5, "[db_stub(%d).insert_dirty]: SOS PUB (non-TAU) data received.\n", SOS_RANK);
      
      char empty[] = "---";
      
      /* SQLite cruft, no point in re-coding for the stub's efficiency... */
      
      int origin_puid = pub->origin_puid;
      int origin_rank = pub->origin_rank;
      char *origin_prog = pub->origin_prog;
      char *role_titles[] = { "SOS_APP", "SOS_MONITOR", "SOS_DB", "SOS_POWSCHED", "UNKNOWN" };
      char *role_str;
      
      switch (pub->origin_role) {
      case SOS_APP      : role_str = (char*) role_titles[0]; break;
      case SOS_MONITOR  : role_str = (char*) role_titles[1]; break;
      case SOS_DB       : role_str = (char*) role_titles[2]; break;
      case SOS_POWSCHED : role_str = (char*) role_titles[3]; break;
      default           : role_str = (char*) role_titles[4]; break;
      }
      
      for (i = 0; i < pub->elem_count ; i++) {
	if (pub->data[i]->dirty == 0) continue;
	
	memset(SOS_db_insert_stmt, '\0', INSERT_MAX);
	
	char thread[4] = "---";
	char *type_str = (char*) SOS_type_str[pub->data[i]->type];
	char *name_str = (char*) pub->data[i]->name;
	double pack_ts = pub->data[i]->pack_ts;
	double send_ts = pub->data[i]->send_ts;
	double recv_ts = pub->data[i]->recv_ts;
	char *key_type = SOS_type_str[pub->data[i]->type];
	char *key_name = pub->data[i]->name;
	char *val_str;
	char num_as_str[NUM_AS_STR_MAX];
	
	memset(num_as_str, '\0', NUM_AS_STR_MAX);
	switch (pub->data[i]->type) {
	case SOS_STRING : val_str = pub->data[i]->val.c_val; SOS_db_strip_str(val_str); break;
	case SOS_INT :	snprintf(num_as_str, NUM_AS_STR_MAX, "%d", pub->data[i]->val.i_val); break;
	case SOS_LONG :	snprintf(num_as_str, NUM_AS_STR_MAX, "%ld", pub->data[i]->val.l_val); break;
	case SOS_DOUBLE :	snprintf(num_as_str, NUM_AS_STR_MAX, "%lf", pub->data[i]->val.d_val); break;
	} if (pub->data[i]->type != SOS_STRING) val_str = num_as_str;
	
	sprintf(SOS_db_insert_stmt, "%d,%d,%d,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%lf,%lf,%lf,\"%s\"\n",
		/*AUTO*/ CURRENT_ROW++,
		/*  1 */ origin_puid,
		/*  2 */ origin_rank,
		/*  3 */ origin_prog,
		/*  4 */ role_str,
		/*  5 */ thread,
		/*  6 */ key_type,
		/*  7 */ key_name,
		/*  8 */ pack_ts,
		/*  9 */ send_ts,
		/* 10 */ recv_ts,
		/* 11 */ val_str );

	fwrite(SOS_db_insert_stmt, strlen(SOS_db_insert_stmt), 1, SOS_db_csv_fp);
	
	pub->data[i]->dirty = 0;
	
	
      } //end: foreach dirty record (NON-TAU)
      
  } //end: pub

  fflush(SOS_db_csv_fp);
  retval = fclose(SOS_db_csv_fp);
  
  dlog(7, "[db_stub(%d).insert_dirty]: Returning to the message loop...\n", SOS_RANK);
  
  return;
}
  
  
  /* Not used... */
  
  void SOS_db_insert_announcement( SOS_pub_handle * pub ) { }
  void SOS_db_update_values( SOS_pub_handle * pub ) { }
  void SOS_db_print_max_value ( void ) { }
  
  
