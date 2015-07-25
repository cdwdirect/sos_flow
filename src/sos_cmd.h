#ifndef SOS_CLOUD_H
#define SOS_CLOUD_H
/*
 * sos_cloud.h        API suppliment for use by cloud modules.
 */

/*
 * [ THREAD SAFETY ]: Note that these routines do not speculatively add
 * [  INFORMATION  ]  locking mechanisms.  If you are going to share
 *                    the same pub/sub handle between several threads,
 *                    you are strongly encouraged to apply mutexes around
 *                    SOS function calls in your code.
 *
 *                    Ideally your application would only have one thread
 *                    involved in making SOS calls.
 *
 */

#include "sos.h"
#include "sos_monitor.h"
#include "sos_db.h"
#include "sos_powsched.h"

/* The range of application ranks this module is responsible for. */

int SOS_RANK_FROM;
int SOS_RANK_TO;
int SOS_RANK_COUNT;

/* Command-line parameter sets the path to the database file. */

char *SOS_DATABASE_FILE;

/* How many seconds should this run before it self-terminates? (-1 == forever) */

int SOS_RUN_SECONDS;

/* Pull in elements from:  sos.c   */
extern void SOS_announce_to( SOS_pub_handle *pub, MPI_Comm target_comm, int target_rank);
extern void SOS_publish_to( SOS_pub_handle *pub, MPI_Comm target_comm, int target_rank);
extern void SOS_free_pub( SOS_pub_handle *pub );
extern void SOS_free_sub( SOS_sub_handle *sub );
extern void SOS_free_data( SOS_data *dat );
extern void SOS_expand_pub( SOS_pub_handle *pub );
extern MPI_Comm SOS_COMM_WORLD;
extern MPI_Comm SOS_COMM_LOCAL;
extern MPI_Comm SOS_ICOMM_APP;
extern MPI_Comm SOS_ICOMM_MONITOR;
extern MPI_Comm SOS_ICOMM_MODULE;
extern MPI_Comm SOS_ICOMM_POWSCHED;
extern int SOS_WARNING_LEVEL;

/* Container structures. */

/*
 *  NOTE: Commented out until the namespace collision with <string.h> is resolved.
 *

typedef struct {
  SOS_pub_handle *pub;
  struct list_head list;
} SOS_pub_list;

typedef struct {
  int rank;
  SOS_pub_list pubs;
  struct list_head list;
} SOS_rank_list;

typedef struct {
  SOS_role role;
  SOS_rank_list ranks;
  struct list_head list;
} SOS_role_list;

 *
 */







#endif //SOS_CLOUD_H
