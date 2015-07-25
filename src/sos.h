#ifndef SOS_H
#define SOS_H

/*
 * sos.h              API for Applications that use SOS.
 *
 *
 * [ THREAD SAFETY ]: Note that these routines do not speculatively add
 * [  INFORMATION  ]  locking mechanisms.  If you are going to share
 *                    the same pub/sub handle between several threads,
 *                    you are strongly encouraged to apply mutexes around
 *                    SOS function calls in your code.
 *
 *                    Certain functions such as SOS_next_serial(); ARE
 *                    thread safe, as SOS uses them in threaded contexts
 *                    internally.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>


#define SOS_TIME(now)  { struct timeval t; gettimeofday(&t, NULL); now = t.tv_sec + t.tv_usec/1000000.0; }



/*
 *  Whenever SOS has a 'temporary' string for element name packing, etc, this
 *  will be the default buffer space that will be allocated for working on it.
 *
 *  This will typically be the maximum 'safe' length of an element name in SOS.
 */
#define SOS_DEFAULT_STRING_LEN   256



/* ************************************ */


enum SOS_role {
  SOS_APP,
  SOS_MONITOR,
  SOS_DB,
  SOS_POWSCHED,
  SOS_SURPLUS,
  SOS_ANALYTICS
};

enum SOS_msg {
  SOS_MSG_ANNOUNCE,
  SOS_MSG_PUBLISH,
  SOS_MSG_UNANNOUNCE,
  SOS_MSG_LIST_PUBS,
  SOS_MSG_SUBSCRIBE,
  SOS_MSG_REFRESH,
  SOS_MSG_UNSUBSCRIBE,
  SOS_MSG_SHUTDOWN
};

enum SOS_type {
  SOS_INT     = 0,
  SOS_LONG    = 1,
  SOS_DOUBLE  = 2,
  SOS_STRING  = 3
};

enum SOS_aggregate {
  SOS_SUM,
  SOS_MIN,
  SOS_MAX,
  SOS_MEAN,
  SOS_STDDEV
};

enum SOS_scope {
  SOS_SCOPE_GLOBAL
};

typedef enum SOS_role       SOS_role;
typedef enum SOS_msg        SOS_msg;
typedef enum SOS_type       SOS_type;
typedef enum SOS_aggregate  SOS_aggregate;
typedef enum SOS_scope      SOS_scope;

typedef union {
  int           i_val;
  long          l_val;
  double        d_val;
  char         *c_val;
} SOS_val;

typedef struct {
  long        guid;
  SOS_type    type;
  SOS_val     val;
  char       *name;
  char       *channel;
  int         dirty;
  double      pack_ts;
  double      send_ts;
  double      recv_ts;
  int         semantic_hint;
  char       *pragma_msg;
  int         pragma_len;
  int         update_priority;
  int         src_layer;
  int         src_role;
  int         scope_hint;
  int         retention_policy;
} SOS_data;

typedef struct {
  int         origin_pub_uid;
  int         origin_node_id;
  int         origin_comm_rank;
  int         origin_process_id;
  int         origin_thread_id;
  SOS_role    origin_role;
  char       *origin_prog_name;
  char       *origin_prog_ver;
  int         target_list;
  char       *pragma_msg;
  int         pragma_len;
  char       *title;
  int         announced;
  int         elem_max;
  int         elem_count;
  SOS_data  **data;
} SOS_pub_handle;

typedef struct {
  int             suid;
  int             active;
  pthread_t       thread_handle;
  int             refresh_delay;
  SOS_role        source_role;
  int             source_rank;
  SOS_pub_handle *pub;
} SOS_sub_handle;


static const char *SOS_TEMP_STRING = "SOS_TEMP_STRING";

int                SOS_ARGC;
char             **SOS_ARGV;
int                SOS_RANK;
int                SOS_SIZE;

int                SOS_SERIAL_GENERIC_VAL;
int                SOS_SERIAL_PUB_VAL;
int                SOS_SERIAL_SUB_VAL;
pthread_mutex_t    SOS_MUTEX_SERIAL;
pthread_mutex_t    SOS_MUTEX_QUEUES;
pthread_mutex_t    SOS_MUTEX_PUBLISH_TO;
pthread_mutex_t    SOS_MUTEX_ANNOUNCE_TO;

SOS_role           SOS_ROLE;



char               SOS_pub_buffer[1000000];
char               SOS_ann_buffer[1000000];

/* Required if included by C++ code. */

#ifdef __cplusplus
extern "C" {
#endif

// ----------- [util] ----------------------


  /*
   * Function..: SOS_init
   * Purpose...: Configure any preliminary values needed before other
   *             parts of the SOS system can work.  This should be
   *             called immediately after MPI_Init().
   *
   */
  void SOS_init( int *argc, char ***argv, SOS_role role );

  /*
   * Function..: SOS_finalize
   * Purpose...: Free up the MPI communicators created during the split.
   *
   */
  void SOS_finalize();
  


  /*
   * Function..: SOS_next_serial   [THREAD SAFE]
   * Purpose...: Return a unique (per process) ID for use in keeping
   *             track of replies to subscription requests, etc.
   */
  int SOS_next_serial();

  /*
   * Function..: SOS_strip_str
   * Purpose...: Convenience function to clear out extended characters from a string.
   *
   */
  void SOS_strip_str(char *str);


  /*
   * Function..: SOS_new_pub
   * Purpose...: Create a new publication handle to work with.
   * Notes.....: By default, all publications target SOS_DB.
   *
   *             Additional targets can be added with SOS_add_target() calls.
   */
  SOS_pub_handle* SOS_new_pub(char *pub_name);


  /*
   * Function..: SOS_pack
   * Purpose...: Helper function that packs name/val arrays into a handle.
   * Notes.....: It manages all malloc/free internally, and grows pubs to support
   *             arbitrarily large schemas as needed.  This SOS_pack() function
   *             is how you define your schema to SOS.
   *
   *             The recommended behavior is to pack empty values into each
   *             unique 'name' you intend to publish, and then announce, all
   *             during your program initialization.  After that, you can
   *             make calls to SOS_pack() OR SOS_repack() to update values, and
   *             make calls to SOS_publish() to send all changed values out.
   *
   *             -- PERFORMANCE ADVISORY --
   *
   *             This function returns the index of the name in the pub's
   *             data store.  If you capture that value, you can use it when
   *             making calls to SOS_repack().  This can be considerably faster
   *             depending on your schema's size and ordering.  SOS_repack has
   *             O(1) performance with low overhead, vs. O(n) for SOS_pack
   *             and string comparisons across a (potentially) linear scan of
   *             every previously packed data element for a matching name.
   */
  int SOS_pack( SOS_pub_handle *pub, const char *name, SOS_type pack_type, SOS_val pack_val );


  /*
   * Function..: SOS_repack
   * Purpose...: 
   *
   */
  void SOS_repack( SOS_pub_handle *pub, int index, SOS_val pack_val );
  


  /*
   * Function..: SOS_display_pub
   * Purpose...: 
   *
   */
  void SOS_display_pub(SOS_pub_handle *pub, FILE *output_to);
  
  
  
  // ---------- [pub] ----------------------
  
  
  
  
  /*
   * Function..: SOS_announce
   * Purpose...: 
   */
  void SOS_announce( SOS_pub_handle *pub );
  
  
  /*
   * Function..: SOS_publish()
   * Purpose...: 
   */
  void SOS_publish( SOS_pub_handle *pub );


  
  /*
   * Function..: SOS_unannounce
   * Purpose...: 
   */
  void SOS_unannounce( SOS_pub_handle *pub );
  
  
  
  
  // --------- [sub] --------------------


  /*
   *  TODO:{ SUBSCRIPTION, CHAD }
   *
   *  This section is a huge work-in-progress...
   */
  


  /* 
   * Function..: SOS_subscribe
   * Purpose...: 
   */
  SOS_sub_handle* SOS_subscribe( SOS_role target_role, int target_rank, char *pub_title, int refresh_ms );



  /*
   * Function..: SOS_unsubscribe
   * Purpose...: 
   */
  void SOS_unsubscribe( SOS_sub_handle *sub );



#ifdef __cplusplus
}
#endif



#endif //SOS_H
