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
#include <limits.h>

#define SOS_TIME(__SOS_now)  { struct timeval t; gettimeofday(&t, NULL); __SOS_now = t.tv_sec + t.tv_usec/1000000.0; }
#define SOS_SET_WHOAMI(__SOS_var_name, __SOS_str_func)                  \
    char __SOS_var_name[SOS_DEFAULT_STRING_LEN];                        \
    {                                                                   \
        memset(whoami, '\0', SOS_DEFAULT_STRING_LEN);                   \
        switch (SOS.role) {                                             \
        case SOS_CLIENT    : sprintf(__SOS_var_name, "client(%s).%s", SOS.client_id, __SOS_str_func ); break; \
        case SOS_SERVER    : sprintf(__SOS_var_name, "server(%s).%s", SOS.client_id, __SOS_str_func ); break; \
        case SOS_LEADER    : sprintf(__SOS_var_name, "leader(%s).%s", SOS.client_id, __SOS_str_func ); break; \
        case SOS_DB        : sprintf(__SOS_var_name, "db(%s).%s", SOS.client_id, __SOS_str_func     ); break; \
        default            : sprintf(__SOS_var_name, "??????(%s).%s", SOS.client_id, __SOS_str_func ); break; \
        }                                                               \
    }


#define SOS_DEFAULT_LOCALHOST    "localhost"
#define SOS_DEFAULT_BUFFER_LEN   1024000
#define SOS_DEFAULT_CMD_TIMEOUT  2048
#define SOS_DEFAULT_RING_SIZE    1024
#define SOS_DEFAULT_STRING_LEN   256
#define SOS_DEFAULT_UID_MAX      LONG_MAX


/* ************************************ */


/*
 *  NOTE: Data sources / [sos_cmd] are SOS_CLIENT's.
 *        All [sosd] instances are SOS_DAEMON, and all
 *        daemons have an upstream target.  The
 *        notion of an 'enclave' is purely logical.
 *        There is only a single SOS_LEADER in the
 *        entire workflow.  Everything doesn't
 *        necessarily propagate to it, but it is the
 *        'authority' for the SOS services to sync on
 *        if there is some reason to.
 *
 *        SOS_DB is the role allocated for the helper
 *        process running alongside the backplane
 *        SQL server.  This may not be necessary, but
 *        we'll see... :)   -CW
 */

enum SOS_role {
    SOS_CLIENT,
    SOS_DAEMON,
    SOS_LEADER,
    SOS_DB
};

enum SOS_status {
    SOS_STATUS_INIT,
    SOS_STATUS_RUNNING,
    SOS_STATUS_SHUTDOWN
};

enum SOS_msg {
    SOS_MSG_ANNOUNCE,
    SOS_MSG_REANNOUNCE,
    SOS_MSG_PUBLISH,
    SOS_MSG_POST,
    SOS_MSG_UNANNOUNCE,
    SOS_MSG_SUBSCRIBE,
    SOS_MSG_UNSUBSCRIBE,
    SOS_MSG_REFRESH,
    SOS_MSG_SHUTDOWN
};

enum SOS_type {
    SOS_INT,
    SOS_LONG,
    SOS_DOUBLE,
    SOS_STRING
};

enum SOS_aggregate {
    SOS_SUM,
    SOS_MIN,
    SOS_MAX,
    SOS_MEAN,
    SOS_STDDEV
};

enum SOS_scope {
    SOS_SCOPE_SELF,
    SOS_SCOPE_NODE,
    SOS_SCOPE_ENCLAVE,
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
    double        pack;
    double        send;
    double        recv;
} SOS_time;

typedef struct {
    char         *channel;
    int           semantic_hint;
    int           pragma_len;     /* allows for blob's */
    char         *pragma_msg;
    int           update_priority;
    int           src_layer;
    int           src_role;
    int           scope_hint;
    int           retention_policy;
} SOS_meta;

typedef struct {
    long          guid;   /* global unique id */
    int           dirty;
    SOS_type      type;
    char         *name;
    SOS_time      time;
    SOS_meta      meta;
    int           len;     /* allows for blob val's */
    SOS_val       val;
} SOS_data;

typedef struct {
    int           origin_pub_id;
    int           origin_node_id;
    int           origin_comm_rank;
    int           origin_process_id;
    int           origin_thread_id;
    SOS_role      origin_role;
    char         *origin_prog_name;
    char         *origin_prog_ver;
    int           target_list;
    char         *pragma_msg;
    int           pragma_len;
    char         *title;
    int           announced;
    int           elem_max;
    int           elem_count;
    SOS_data    **data;
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

typedef struct {
    char           *cmd_host;
    int             cmd_port;
    int             buffer_len;
    int             cmd_timeout;
    int             argc;
    char          **argv;
} SOS_config;

typedef struct {
    long            next;
    long            last;
    pthread_mutex_t lock;
} SOS_uid;

typedef struct {
    SOS_uid       pub;
    SOS_uid       sub;
    SOS_uid       seq;
} SOS_unique_set;

typedef struct {
    int             read_pos;
    int             write_pos;
    int             size;
    long            bytes;
    void          **heap;
    pthread_mutex_t lock;
} SOS_ring_queue;

typedef struct {
    SOS_ring_queue  send;
    SOS_ring_queue  recv;
} SOS_ring_set;

typedef struct {
    SOS_config       config;
    SOS_role         role;
    SOS_unique_set   uid;
    SOS_ring_set     ring;
    SOS_status       status;
    int              thread_count;
    pthread_mutex_t  global_lock;
    char            *client_id;
} SOS_runtime;



SOS_runtime SOS;





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
    long SOS_next_id( SOS_uid *uid );


    /*
     * Function..: SOS_expand_data
     * Purpose...: 
     *
     */
    void SOS_expand_data( SOS_pub_handle *pub );


    /*
     * Function..: SOS_strip_str
     * Purpose...: Convenience function to clear out extended characters from a string.
     *
     */
    void SOS_strip_str(char *str);



    /*
     * Function..: SOS_apply_announce / SOS_apply_publish
     * Purpose...:
     *
     */
    void SOS_apply_announce( SOS_pub_handle *pub, char *msg, int msg_len );
    void SOS_apply_publish( SOS_pub_handle *pub, char *msg, int msg_len );



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
     * Function..: SOS_get_val
     * Purpose...: 
     *
     */
    void SOS_get_val( SOS_pub_handle *pub, char *name );


    

    /*
     * Function..: SOS_new_pub
     * Purpose...: Create a new publication handle to work with.
     *
     */
    SOS_pub_handle* SOS_new_pub(char *pub_name);


    /*
     * Function..: SOS_new_sub
     * Purpose...: 
     *
     */
    SOS_sub_handle* SOS_new_sub();
  

    /*
     * Function..: SOS_free_pub / SOS_free_sub
     * Purpose...:
     *
     */
    void SOS_free_pub( SOS_pub_handle *pub );
    void SOS_free_sub( SOS_sub_handle *sub );


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
