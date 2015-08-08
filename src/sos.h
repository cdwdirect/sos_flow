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
        case SOS_CLIENT    : sprintf(__SOS_var_name, "client(%s).%s",  SOS.client_id, __SOS_str_func ); break; \
        case SOS_DAEMON    : sprintf(__SOS_var_name, "daemon(%s).%s",  SOS.client_id, __SOS_str_func ); break; \
        case SOS_LEADER    : sprintf(__SOS_var_name, "leader(%s).%s",  SOS.client_id, __SOS_str_func ); break; \
        case SOS_CONTROL   : sprintf(__SOS_var_name, "control(%s).%s", SOS.client_id, __SOS_str_func ); break; \
        default            : sprintf(__SOS_var_name, "------(%s).%s",  SOS.client_id, __SOS_str_func ); break; \
        }                                                               \
    }

/* NOTE: The __SOS_tmp_str is not implicitly thread-safe: Use TLS for the ptr. */
#define SOS_STR_COPY_OF(__SOS_src_str, __SOS_tmp_str)                   \
    __SOS_tmp_str = malloc( 1 + ( sizeof(char) * strlen(__SOS_src_str)) ); \
    strcpy( __SOS_tmp_str, __SOS_src_str );                             


#define SOS_DEFAULT_LOCALHOST      "localhost"
#define SOS_DEFAULT_BUFFER_LEN     1024000
#define SOS_DEFAULT_CMD_TIMEOUT    2048
#define SOS_DEFAULT_RING_SIZE      1024
#define SOS_DEFAULT_STRING_LEN     256
#define SOS_DEFAULT_UID_MAX        LONG_MAX
#define SOS_DEFAULT_ELEM_COUNT     64

/* ************************************ */


typedef enum SOS_whoami {
    SOS_WHOAMI_CLIENT,
    SOS_WHOAMI_DAEMON,
    SOS_WHOAMI_LEADER,
    SOS_WHOAMI_CONTROL
} SOS_whoami;

typedef enum SOS_state {
    SOS_STATE_INIT,
    SOS_STATE_RUNNING,
    SOS_STATE_SHUTDOWN
} SOS_state;

typedef enum SOS_msg_type {
    SOS_MSG_TYPE_ANNOUNCE,
    SOS_MSG_TYPE_REANNOUNCE,
    SOS_MSG_TYPE_VALUE,
    SOS_MSG_TYPE_ACKNOWLEDGE,
    SOS_MSG_TYPE_SHUTDOWN
} SOS_msg_type;

typedef enum SOS_pri {
    SOS_PRI_DEFAULT,
    SOS_PRI_LOW,
    SOS_PRI_IMMEDIATE
} SOS_pri;

typedef enum SOS_val_type {
    SOS_VAL_TYPE_LONG,
    SOS_VAL_TYPE_DOUBLE,
    SOS_VAL_TYPE_TEXT,
    SOS_VAL_TYPE_BLOB
} SOS_val_type;

typedef enum SOS_val_state {
    SOS_VAL_STATE_CLEAN,
    SOS_VAL_STATE_DIRTY,
    SOS_VAL_STATE_EMPTY
} SOS_val_state;

typedef enum SOS_val_sem {
    SOS_VAL_SEM_TIME_START,
    SOS_VAL_SEM_TIME_STOP,
    SOS_VAL_SEM_TIME_STAMP,
    SOS_VAL_SEM_TIME_SPAN,
    SOS_VAL_SEM_VAL_CURRENT,
    SOS_VAL_SEM_VAL_COUNTER
} SOS_sem;

typedef enum SOS_scope {
    SOS_SCOPE_DEFAULT,
    SOS_SCOPE_SELF,
    SOS_SCOPE_NODE,
    SOS_SCOPE_ENCLAVE
} SOS_scope;

typedef enum SOS_layer {
    SOS_LAYER_APP,
    SOS_LAYER_OS,
    SOS_LAYER_LIB,
    SOS_LAYER_FLOW,
    SOS_LAYER_CONTROL
} SOS_layer;

typedef enum SOS_nature {
    SOS_NATURE_CREATE_INPUT,
    SOS_NATURE_CREATE_OUTPUT,
    SOS_NATURE_CREATE_VIZ,
    SOS_NATURE_EXEC_WORK,
    SOS_NATURE_BUFFER,
    SOS_NATURE_SUPPORT_EXEC,
    SOS_NATURE_SUPPORT_FLOW,
    SOS_NATURE_CONTROL_FLOW,
    SOS_NATURE_SOS
} SOS_nature;

typedef enum SOS_retain {
    SOS_RETAIN_DEFAULT,
    SOS_RETAIN_SESSION,
    SOS_RETAIN_IMMEDIATE
} SOS_retain;

typedef union {
    int           i_val;        /* default: (null)                */
    long          l_val;        /* default: (null)                */
    double        d_val;        /* default: (null)                */
    char         *c_val;        /* default: (null)                */
} SOS_val;

typedef struct {
    double        pack;         /* default: -1.0                  */
    double        send;         /* default: -1.0                  */
    double        recv;         /* default: -1.0                  */
} SOS_time;

typedef struct {
    char         *channel;      /* default: (null)                */
    int           pragma_len;   /* default: -1                    */
    char         *pragma_msg;   /* default: (null)                */
    SOS_role      role;         /* default: --------- manual      */
    SOS_nature    nature;       /* default: --------- manual      */
    SOS_layer     layer;        /* default: SOS_LAYER_APP         */
    SOS_pri       pri_hint;     /* default: SOS_PRI_DEFAULT       */
    SOS_sem       sem_hint;     /* default: --------- manual      */
    SOS_scope     scope_hint;   /* default: SOS_SCOPE_DEFAULT     */
    SOS_retain    retain_hint;  /* default: SOS_RETAIN_DEFAULT    */
} SOS_meta;

typedef struct {
    long          guid;         /* default: (auto)                */
    char         *name;         /* default: --------- manual      */
    SOS_type      type;         /* default: --------- manual      */
    int           len;          /* default: (auto) [on assign]    */
    SOS_val       val;          /* default: --------- manual      */
    SOS_dirty     dirty;        /* default: SOS_VAL_STATE_EMPTY   */
    SOS_time      time;         /* default: (complex)             */
} SOS_data;

typedef struct {
    int           pub_id;       /* default: (auto)                */
    char         *node_id;      /* default: SOS.config.node_id    */
    int           process_id;   /* default: -1                    */
    int           thread_id;    /* default: -1                    */
    int           comm_rank;    /* default: -1                    */
    SOS_meta      meta;         /* default: (complex)             */
    char         *prog_name;    /* default: argv[0] / manual      */
    char         *prog_ver;     /* default: (null)                */
    int           pragma_len;   /* default: -1                    */
    char         *pragma_msg;   /* default: (null)                */
    char         *title;        /* default: (null)                */
    int           announced;    /* default: 0                     */
    int           elem_max;     /* default: SOS_DEFAULT_ELEM_MAX  */
    int           elem_count;   /* default: 0                     */
    SOS_data    **data;
} SOS_pub;

typedef struct {
    int             suid;
    int             active;
    pthread_t       thread_handle;
    int             refresh_delay;
    SOS_role        source_role;
    int             source_rank;
    SOS_pub *pub;
} SOS_sub;

typedef struct {
    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    int         server_socket_fd;
    int         client_socket_fd;
    char       *cmd_host;
    int         cmd_port;
    int         buffer_len;
    int         cmd_timeout;
} SOS_socket_set;

typedef struct {
    int               argc;
    char            **argv;
    char             *node_id;
} SOS_config;

typedef struct {
    long            next;
    long            last;
    pthread_mutex_t lock;
} SOS_uid;

typedef struct {
    SOS_uid       *pub;
    SOS_uid       *sub;
    SOS_uid       *seq;
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
    pthread_t      *post;    /* POST pending msgs to the daemon */
    pthread_t      *read;    /* READ char* msgs, organize into data structures. */
    pthread_t      *scan;    /* SCAN for dirty data, queue msg for daemon. */
} SOS_task_set;

typedef struct {
    SOS_config       config;
    SOS_role         role;
    SOS_unique_set   uid;
    SOS_ring_set     ring;
    SOS_task_set     task;
    SOS_status       status;
    SOS_socket_set   target;        /* Daemon or DB, etc. */
    pthread_mutex_t  global_lock;
    char            *client_id;
} SOS_runtime;



/* ----------
 *
 *  The root 'global' data structure:
 */

SOS_runtime SOS;

/*
 *
 * ----------
 */

/* Required if included by C++ code. */
#ifdef __cplusplus
extern "C" {
#endif


    
/* =========== [summary] =================== */
/*

    ---- SOS_FLOW - ready functions ----


    void SOS_init( int *argc, char ***argv, SOS_role role );
    void SOS_finalize();

    long SOS_next_id( SOS_uid *uid );
    void SOS_strip_str(char *str);


    
*/  
/* ----------- [util] ---------------------- */



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
     * Function..: SOS_strip_str
     * Purpose...: Convenience function to clear out extended characters from a string.
     *
     */
    void SOS_strip_str(char *str);


    
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
    int SOS_pack( SOS_pub *pub, const char *name, SOS_type pack_type, SOS_val pack_val );
    void SOS_repack( SOS_pub *pub, int index, SOS_val pack_val );

    SOS_pub* SOS_new_pub(char *pub_name);
    SOS_pub* SOS_new_post(char *pub_name);    /* A 'post' is a pub w/1 data element. */


    /*
     *  Functions that have not yet been ported to SOS_FLOW structure...
     *
     */
    
    void SOS_apply_announce( SOS_pub *pub, char *msg, int msg_len );
    void SOS_apply_publish( SOS_pub *pub, char *msg, int msg_len );
    void SOS_expand_data( SOS_pub *pub );
    
    void SOS_get_val( SOS_pub *pub, char *name );

    SOS_sub* SOS_new_sub();
  
    void SOS_free_pub( SOS_pub *pub );
    void SOS_free_sub( SOS_sub *sub );

    void SOS_display_pub(SOS_pub *pub, FILE *output_to);
    
    void SOS_announce( SOS_pub *pub );

    /* TODO:{ CHAD, PUBLISH, POST } --> Add SOS_publish_immediately() and SOS_post_immediately(); */

    void SOS_publish( SOS_pub *pub );
    void SOS_publish_immediately( SOS_pub *pub );

    void SOS_send_to_daemon( char *msg, char *reply );

    void SOS_unannounce( SOS_pub *pub );

    SOS_sub* SOS_subscribe( SOS_role target_role, int target_rank, char *pub_title, int refresh_ms );
    void SOS_unsubscribe( SOS_sub *sub );



#ifdef __cplusplus
}
#endif



#endif //SOS_H
