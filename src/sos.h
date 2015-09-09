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
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <limits.h>

#include <sys/socket.h>
#include <netdb.h>


/* SOS Configuration Switches... */

#define SOS_CONFIG_DUMP_TO_FILE        1
#define SOS_CONFIG_USE_THREAD_POOL     0



#define SOS_TIME(__SOS_now)  { struct timeval t; gettimeofday(&t, NULL); __SOS_now = t.tv_sec + t.tv_usec/1000000.0; }
#define SOS_SET_WHOAMI(__SOS_var_name, __SOS_str_func)                  \
    char __SOS_var_name[SOS_DEFAULT_STRING_LEN];                        \
    {                                                                   \
        memset(whoami, '\0', SOS_DEFAULT_STRING_LEN);                   \
        switch (SOS.role) {                                             \
        case SOS_ROLE_CLIENT    : sprintf(__SOS_var_name, "client(%ld).%s",  SOS.my_guid, __SOS_str_func ); break; \
        case SOS_ROLE_DAEMON    : sprintf(__SOS_var_name, "daemon(%ld).%s",  SOS.my_guid, __SOS_str_func ); break; \
        case SOS_ROLE_LEADER    : sprintf(__SOS_var_name, "leader(%ld).%s",  SOS.my_guid, __SOS_str_func ); break; \
        case SOS_ROLE_CONTROL   : sprintf(__SOS_var_name, "control(%ld).%s", SOS.my_guid, __SOS_str_func ); break; \
        default            : sprintf(__SOS_var_name, "------(%ld).%s",  SOS.my_guid, __SOS_str_func ); break; \
        }                                                               \
    }


#define SOS_DEFAULT_SERVER_HOST    "localhost"
#define SOS_DEFAULT_SERVER_PORT    22505
#define SOS_DEFAULT_MSG_TIMEOUT    2048
#define SOS_DEFAULT_BUFFER_LEN     1048576
#define SOS_DEFAULT_RING_SIZE      1024
#define SOS_DEFAULT_TABLE_SIZE     128
#define SOS_DEFAULT_STRING_LEN     256
#define SOS_DEFAULT_UID_MAX        LONG_MAX
#define SOS_DEFAULT_ELEM_COUNT     64

/* ************************************ */


#define FOREACH_ROLE(ROLE)                      \
    ROLE(SOS_ROLE_CLIENT)                       \
    ROLE(SOS_ROLE_DAEMON)                       \
    ROLE(SOS_ROLE_LEADER)                       \
    ROLE(SOS_ROLE_CONTROL)                      \
    
#define FOREACH_STATUS(STATUS)                  \
    STATUS(SOS_STATUS_INIT)                     \
    STATUS(SOS_STATUS_RUNNING)                  \
    STATUS(SOS_STATUS_SHUTDOWN)                 \
    
#define FOREACH_MSG_TYPE(MSG_TYPE)              \
    MSG_TYPE(SOS_MSG_TYPE_REGISTER)             \
    MSG_TYPE(SOS_MSG_TYPE_ANNOUNCE)             \
    MSG_TYPE(SOS_MSG_TYPE_PUBLISH)              \
    MSG_TYPE(SOS_MSG_TYPE_ECHO)                 \
    MSG_TYPE(SOS_MSG_TYPE_SHUTDOWN)             \
    
#define FOREACH_PRI(PRI)                        \
    PRI(SOS_PRI_DEFAULT)                        \
    PRI(SOS_PRI_LOW)                            \
    PRI(SOS_PRI_IMMEDIATE)                      \

#define FOREACH_VAL_TYPE(VAL_TYPE)              \
    VAL_TYPE(SOS_VAL_TYPE_INT)                  \
    VAL_TYPE(SOS_VAL_TYPE_LONG)                 \
    VAL_TYPE(SOS_VAL_TYPE_DOUBLE)               \
    VAL_TYPE(SOS_VAL_TYPE_STRING)               \

#define FOREACH_VAL_STATE(VAL_STATE)            \
    VAL_STATE(SOS_VAL_STATE_CLEAN)              \
    VAL_STATE(SOS_VAL_STATE_DIRTY)              \
    VAL_STATE(SOS_VAL_STATE_EMPTY)              \

#define FOREACH_SEM(SEM)                        \
    SEM(SOS_SEM_TIME_START)                     \
    SEM(SOS_SEM_TIME_STOP)                      \
    SEM(SOS_SEM_TIME_STAMP)                     \
    SEM(SOS_SEM_TIME_SPAN)                      \
    SEM(SOS_SEM_VAL_CURRENT)                    \
    SEM(SOS_SEM_VAL_COUNTER)                    \
    SEM(SOS_SEM_VAL_LOG)                        \

#define FOREACH_SCOPE(SCOPE)                    \
    SCOPE(SOS_SCOPE_DEFAULT)                    \
    SCOPE(SOS_SCOPE_SELF)                       \
    SCOPE(SOS_SCOPE_NODE)                       \
    SCOPE(SOS_SCOPE_ENCLAVE)                    \

#define FOREACH_LAYER(LAYER)                    \
    LAYER(SOS_LAYER_APP)                        \
    LAYER(SOS_LAYER_OS)                         \
    LAYER(SOS_LAYER_LIB)                        \
    LAYER(SOS_LAYER_FLOW)                       \
    LAYER(SOS_LAYER_CONTROL)                    \

#define FOREACH_NATURE(NATURE)                  \
    NATURE(SOS_NATURE_CREATE_INPUT)             \
    NATURE(SOS_NATURE_CREATE_OUTPUT)            \
    NATURE(SOS_NATURE_CREATE_VIZ)               \
    NATURE(SOS_NATURE_EXEC_WORK)                \
    NATURE(SOS_NATURE_BUFFER)                   \
    NATURE(SOS_NATURE_SUPPORT_EXEC)             \
    NATURE(SOS_NATURE_SUPPORT_FLOW)             \
    NATURE(SOS_NATURE_CONTROL_FLOW)             \
    NATURE(SOS_NATURE_SOS)                      \
        
#define FOREACH_RETAIN(RETAIN)                  \
    RETAIN(SOS_RETAIN_DEFAULT)                  \
    RETAIN(SOS_RETAIN_SESSION)                  \
    RETAIN(SOS_RETAIN_IMMEDIATE)                \
    

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

typedef enum { FOREACH_ROLE(GENERATE_ENUM)      } SOS_role;
typedef enum { FOREACH_STATUS(GENERATE_ENUM)    } SOS_status;
typedef enum { FOREACH_MSG_TYPE(GENERATE_ENUM)  } SOS_msg_type;
typedef enum { FOREACH_PRI(GENERATE_ENUM)       } SOS_pri;
typedef enum { FOREACH_VAL_TYPE(GENERATE_ENUM)  } SOS_val_type;
typedef enum { FOREACH_VAL_STATE(GENERATE_ENUM) } SOS_val_state;
typedef enum { FOREACH_SEM(GENERATE_ENUM)       } SOS_sem;
typedef enum { FOREACH_SCOPE(GENERATE_ENUM)     } SOS_scope;
typedef enum { FOREACH_LAYER(GENERATE_ENUM)     } SOS_layer;
typedef enum { FOREACH_NATURE(GENERATE_ENUM)    } SOS_nature;
typedef enum { FOREACH_RETAIN(GENERATE_ENUM)    } SOS_retain;

static const char *SOS_role_string[] =      { FOREACH_ROLE(GENERATE_STRING)      };
static const char *SOS_status_string[] =    { FOREACH_STATUS(GENERATE_STRING)    };
static const char *SOS_msg_type_string[] =  { FOREACH_MSG_TYPE(GENERATE_STRING)  };
static const char *SOS_pri_string[] =       { FOREACH_PRI(GENERATE_STRING)       };
static const char *SOS_val_type_string[] =  { FOREACH_VAL_TYPE(GENERATE_STRING)  };
static const char *SOS_val_state_string[] = { FOREACH_VAL_STATE(GENERATE_STRING) };
static const char *SOS_sem_string[] =       { FOREACH_SEM(GENERATE_STRING)       };
static const char *SOS_scope_string[] =     { FOREACH_SCOPE(GENERATE_STRING)     };
static const char *SOS_layer_string[] =     { FOREACH_LAYER(GENERATE_STRING)     };
static const char *SOS_nature_string[] =    { FOREACH_NATURE(GENERATE_STRING)    };
static const char *SOS_retain_string[] =    { FOREACH_RETAIN(GENERATE_STRING)    };

typedef union {
    int           i_val;        /* default: (null)                */
    long          l_val;        /* default: (null)                */
    double        d_val;        /* default: (null)                */
    char         *c_val;        /* default: (null)                */
} SOS_val;

typedef struct {
    double        pack;         /* default: 0.0                   */
    double        send;         /* default: 0.0                   */
    double        recv;         /* default: 0.0                   */
} SOS_time;

typedef struct {
    int           channel;      /* default: 0                     */
    SOS_nature    nature;       /* default: --------- manual      */
    SOS_layer     layer;        /* default: SOS_LAYER_APP         */
    SOS_pri       pri_hint;     /* default: SOS_PRI_DEFAULT       */
    SOS_scope     scope_hint;   /* default: SOS_SCOPE_DEFAULT     */
    SOS_retain    retain_hint;  /* default: SOS_RETAIN_DEFAULT    */
} SOS_meta;

typedef struct {
    long          guid;         /* default: (auto)                */
    SOS_val_type  type;         /* default: --------- manual      */
    SOS_sem       sem_hint;     /* default: --------- manual      */
    int           val_len;      /* default: (auto) [on assign]    */
    SOS_val       val;          /* default: --------- manual      */
    SOS_val_state state;        /* default: SOS_VAL_STATE_EMPTY   */
    SOS_time      time;         /* default: (complex)             */
    char          __PTR_BEGIN__;/* .........(only pointers follow)*/
    char         *name;         /* default: --------- manual      */
} SOS_data;

typedef struct {
    long          guid;         /* default: (auto, on announce)   */
    int           process_id;   /* default: -1                    */
    int           thread_id;    /* default: -1                    */
    int           comm_rank;    /* default: -1                    */
    SOS_meta      meta;         /* default: (complex)             */
    int           announced;    /* default: 0                     */
    int           elem_max;     /* default: SOS_DEFAULT_ELEM_MAX  */
    int           elem_count;   /* default: 0                     */
    int           pragma_len;   /* default: -1                    */
    char          __PTR_BEGIN__;/* .........(only pointers follow)*/
    char         *pragma_msg;   /* default: (null)                */
    char         *node_id;      /* default: SOS.config.node_id    */
    char         *prog_name;    /* default: argv[0] / manual      */
    char         *prog_ver;     /* default: (null)                */
    char         *title;        /* default: (null)                */
    SOS_data    **data;
} SOS_pub;

typedef struct {
    int           suid;
    int           active;
    pthread_t     thread_handle;
    int           refresh_delay;
    SOS_role      source_role;
    int           source_rank;
    char          __PTR_BEGIN__;/* .........(only pointers follow)*/
    SOS_pub      *pub;
} SOS_sub;

typedef struct {
    char               *server_host;
    char               *server_port;
    struct addrinfo    *server_addr;
    struct addrinfo    *result_list;
    struct addrinfo     server_hint;
    struct addrinfo    *client_addr;          /* used by [sosd] */
    int                 timeout;
    int                 buffer_len;
} SOS_socket_set;

typedef struct {                              /* no pointers, headers get used raw */
    SOS_msg_type   msg_type;
    long           msg_from;
    long           pub_guid;
} SOS_msg_header;

typedef struct {
    int               argc;
    char            **argv;
    char             *node_id;
    int               process_id;
    int               thread_id;
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
    pthread_t    *post;    /* POST pending msgs to the daemon */
    pthread_t    *read;    /* READ char* msgs, organize into data structures. */
    pthread_t    *scan;    /* SCAN for dirty data, queue msg for daemon. */
} SOS_task_set;

typedef struct {
    SOS_config       config;
    SOS_role         role;
    SOS_status       status;
    SOS_unique_set   uid;
    SOS_ring_set     ring;
    SOS_task_set     task;
    SOS_socket_set   net;
    pthread_mutex_t  global_lock;
    long             my_guid;
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
     *             parts of the SOS system can work.
     *
     */
    void SOS_init( int *argc, char ***argv, SOS_role role );

    

    /*
     * Function..: SOS_finalize
     * Purpose...: Unregister with SOS and close down any threads.
     *
     */
    void SOS_finalize();
  


    /*
     * Function..: SOS_next_serial   [THREAD SAFE]
     * Purpose...: Return a unique ID from some managed uid context.
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
    int SOS_pack( SOS_pub *pub, const char *name, SOS_val_type pack_type, SOS_val pack_val );
    void SOS_repack( SOS_pub *pub, int index, SOS_val pack_val );

    SOS_pub* SOS_new_pub(char *pub_name);
    SOS_pub* SOS_new_post(char *pub_name);
    SOS_pub* SOS_new_pub_sized(char *title, int new_size);

    void SOS_announce( SOS_pub *pub );
    void SOS_send_to_daemon( char *buffer, int buffer_len, char *reply, int reply_len );


    /*
     *  Functions that have not yet been ported to SOS_FLOW structure...
     */
    
    void SOS_apply_announce( SOS_pub *pub, char *msg, int msg_len );
    void SOS_apply_publish( SOS_pub *pub, char *msg, int msg_len );
    void SOS_expand_data( SOS_pub *pub );
    
    SOS_val SOS_get_val( SOS_pub *pub, char *name );

    SOS_sub* SOS_new_sub();
  
    void SOS_free_pub( SOS_pub *pub );
    void SOS_free_sub( SOS_sub *sub );

    void SOS_display_pub(SOS_pub *pub, FILE *output_to);
    

    void SOS_publish( SOS_pub *pub );
    void SOS_publish_immediately( SOS_pub *pub );    /* Do we want this? */


    void SOS_unannounce( SOS_pub *pub );

    SOS_sub* SOS_subscribe( SOS_role target_role, int target_rank, char *pub_title, int refresh_ms );
    void SOS_unsubscribe( SOS_sub *sub );



#ifdef __cplusplus
}
#endif



#endif //SOS_H
