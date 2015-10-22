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

#include "qhashtbl.h"

/* SOS Configuration Switches... */

/*
 *   TODO: { VMPI } Merge in the capability of the former SOS project...
 *
 */

#define SOS_CONFIG_USE_THREAD_POOL     1
#define SOS_CONFIG_USE_MUTEXES         1

#define SOS_TIME(__SOS_now)  { struct timeval t; gettimeofday(&t, NULL); __SOS_now = t.tv_sec + t.tv_usec/1000000.0; }
#define SOS_SET_WHOAMI(__SOS_var_name, __SOS_str_func)                  \
    char __SOS_var_name[SOS_DEFAULT_STRING_LEN];                        \
    {                                                                   \
        memset(whoami, '\0', SOS_DEFAULT_STRING_LEN);                   \
        switch (SOS.role) {                                             \
        case SOS_ROLE_CLIENT    : sprintf(__SOS_var_name, "client(%ld).%s",  SOS.my_guid, __SOS_str_func ); break; \
        case SOS_ROLE_DAEMON    : sprintf(__SOS_var_name, "daemon(%d).%s",   SOS.config.comm_rank, __SOS_str_func ); break; \
        case SOS_ROLE_DB        : sprintf(__SOS_var_name, "db(%ld).%s",      SOS.my_guid, __SOS_str_func ); break; \
        case SOS_ROLE_CONTROL   : sprintf(__SOS_var_name, "control(%ld).%s", SOS.my_guid, __SOS_str_func ); break; \
        default            : sprintf(__SOS_var_name, "------(%ld).%s",  SOS.my_guid, __SOS_str_func ); break; \
        }                                                               \
    }

#define SOS_DEFAULT_SERVER_HOST    "localhost"
#define SOS_DEFAULT_SERVER_PORT    22505
#define SOS_DEFAULT_MSG_TIMEOUT    2048
#define SOS_DEFAULT_BUFFER_LEN     1048576
#define SOS_DEFAULT_ACK_LEN        128
#define SOS_DEFAULT_RING_SIZE      1024
#define SOS_DEFAULT_TABLE_SIZE     128
#define SOS_DEFAULT_STRING_LEN     256
#define SOS_DEFAULT_UID_MAX        LONG_MAX
#define SOS_DEFAULT_GUID_BLOCK     512
#define SOS_DEFAULT_ELEM_MAX       64

/* ************************************ */


#define FOREACH_ROLE(ROLE)                      \
    ROLE(SOS_ROLE_CLIENT)                       \
    ROLE(SOS_ROLE_DAEMON)                       \
    ROLE(SOS_ROLE_DB)                           \
    ROLE(SOS_ROLE_CONTROL)                      \
    ROLE(SOS_ROLE___MAX)

#define FOREACH_TARGET(TARGET)                  \
    TARGET(SOS_TARGET_LOCAL_SYNC)               \
    TARGET(SOS_TARGET_NODE_SYNC)                \
    TARGET(SOS_TARGET___MAX)
    
#define FOREACH_STATUS(STATUS)                  \
    STATUS(SOS_STATUS_INIT)                     \
    STATUS(SOS_STATUS_RUNNING)                  \
    STATUS(SOS_STATUS_SHUTDOWN)                 \
    STATUS(SOS_STATUS___MAX)
    
#define FOREACH_MSG_TYPE(MSG_TYPE)              \
    MSG_TYPE(SOS_MSG_TYPE_REGISTER)             \
    MSG_TYPE(SOS_MSG_TYPE_GUID_BLOCK)           \
    MSG_TYPE(SOS_MSG_TYPE_ANNOUNCE)             \
    MSG_TYPE(SOS_MSG_TYPE_PUBLISH)              \
    MSG_TYPE(SOS_MSG_TYPE_ECHO)                 \
    MSG_TYPE(SOS_MSG_TYPE_SHUTDOWN)             \
    MSG_TYPE(SOS_MSG_TYPE___MAX)
    
#define FOREACH_PRI(PRI)                        \
    PRI(SOS_PRI_DEFAULT)                        \
    PRI(SOS_PRI_LOW)                            \
    PRI(SOS_PRI_IMMEDIATE)                      \
    PRI(SOS_PRI___MAX)

#define FOREACH_VAL_TYPE(VAL_TYPE)              \
    VAL_TYPE(SOS_VAL_TYPE_INT)                  \
    VAL_TYPE(SOS_VAL_TYPE_LONG)                 \
    VAL_TYPE(SOS_VAL_TYPE_DOUBLE)               \
    VAL_TYPE(SOS_VAL_TYPE_STRING)               \
    VAL_TYPE(SOS_VAL_TYPE___MAX)

#define FOREACH_VAL_STATE(VAL_STATE)            \
    VAL_STATE(SOS_VAL_STATE_CLEAN)              \
    VAL_STATE(SOS_VAL_STATE_DIRTY)              \
    VAL_STATE(SOS_VAL_STATE_EMPTY)              \
    VAL_STATE(SOS_VAL_STATE___MAX)

#define FOREACH_VAL_CLASS(VAL_CLASS)            \
    VAL_CLASS(SOS_VAL_CLASS_DATA)               \
    VAL_CLASS(SOS_VAL_CLASS_EVENT)              \
    VAL_CLASS(SOS_VAL_CLASS___MAX)

#define FOREACH_VAL_SEMANTIC(VAL_SEM)           \
    VAL_SEM(SOS_VAL_SEMANTIC_DEFAULT)           \
    VAL_SEM(SOS_VAL_SEMANTIC_TIME_START)        \
    VAL_SEM(SOS_VAL_SEMANTIC_TIME_STOP)         \
    VAL_SEM(SOS_VAL_SEMANTIC_TIME_STAMP)        \
    VAL_SEM(SOS_VAL_SEMANTIC_TIME_SPAN)         \
    VAL_SEM(SOS_VAL_SEMANTIC_SAMPLE)            \
    VAL_SEM(SOS_VAL_SEMANTIC_COUNTER)           \
    VAL_SEM(SOS_VAL_SEMANTIC_LOG)               \
    VAL_SEM(SOS_VAL_SEMANTIC___MAX)

#define FOREACH_VAL_FREQ(VAL_FREQ)              \
    VAL_FREQ(SOS_VAL_FREQ_DEFAULT)              \
    VAL_FREQ(SOS_VAL_FREQ_RARE)                 \
    VAL_FREQ(SOS_VAL_FREQ_COMMON)               \
    VAL_FREQ(SOS_VAL_FREQ_CONTINUOUS)           \
    VAL_FREQ(SOS_VAL_FREQ_IRREGULAR)            \
    VAL_FREQ(SOS_VAL_FREQ___MAX)

#define FOREACH_VAL_PATTERN(PATTERN)            \
    PATTERN(SOS_VAL_PATTERN_DEFAULT)            \
    PATTERN(SOS_VAL_PATTERN_STATIC)             \
    PATTERN(SOS_VAL_PATTERN_RISING)             \
    PATTERN(SOS_VAL_PATTERN_PLATEAU)            \
    PATTERN(SOS_VAL_PATTERN_OSCILLATING)        \
    PATTERN(SOS_VAL_PATTERN_ARC)                \
    PATTERN(SOS_VAL_PATTERN___MAX)

#define FOREACH_VAL_COMPARE(COMPARE)            \
    COMPARE(SOS_VAL_COMPARE_SELF)               \
    COMPARE(SOS_VAL_COMPARE_RELATIONS)          \
    COMPARE(SOS_VAL_COMPARE___MAX)

#define FOREACH_MOOD(MOOD)                      \
    MOOD(SOS_MOOD_DEFAULT)                      \
    MOOD(SOS_MOOD_GOOD)                         \
    MOOD(SOS_MOOD_BAD)                          \
    MOOD(SOS_MOOD_UGLY)                         \
    MOOD(SOS_MOOD___MAX)

#define FOREACH_SCOPE(SCOPE)                    \
    SCOPE(SOS_SCOPE_DEFAULT)                    \
    SCOPE(SOS_SCOPE_SELF)                       \
    SCOPE(SOS_SCOPE_NODE)                       \
    SCOPE(SOS_SCOPE_ENCLAVE)                    \
    SCOPE(SOS_SCOPE___MAX)

#define FOREACH_LAYER(LAYER)                    \
    LAYER(SOS_LAYER_APP)                        \
    LAYER(SOS_LAYER_OS)                         \
    LAYER(SOS_LAYER_LIB)                        \
    LAYER(SOS_LAYER_FLOW)                       \
    LAYER(SOS_LAYER_CONTROL)                    \
    LAYER(SOS_LAYER___MAX)

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
    NATURE(SOS_NATURE___MAX)
        
#define FOREACH_RETAIN(RETAIN)                  \
    RETAIN(SOS_RETAIN_DEFAULT)                  \
    RETAIN(SOS_RETAIN_SESSION)                  \
    RETAIN(SOS_RETAIN_IMMEDIATE)                \
    RETAIN(SOS_RETAIN___MAX)

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

typedef enum { FOREACH_ROLE(GENERATE_ENUM)         } SOS_role;
typedef enum { FOREACH_TARGET(GENERATE_ENUM)       } SOS_target;
typedef enum { FOREACH_STATUS(GENERATE_ENUM)       } SOS_status;
typedef enum { FOREACH_MSG_TYPE(GENERATE_ENUM)     } SOS_msg_type;
typedef enum { FOREACH_PRI(GENERATE_ENUM)          } SOS_pri;
typedef enum { FOREACH_VAL_TYPE(GENERATE_ENUM)     } SOS_val_type;
typedef enum { FOREACH_VAL_STATE(GENERATE_ENUM)    } SOS_val_state;
typedef enum { FOREACH_VAL_SEMANTIC(GENERATE_ENUM) } SOS_val_semantic;
typedef enum { FOREACH_VAL_FREQ(GENERATE_ENUM)     } SOS_val_freq;
typedef enum { FOREACH_VAL_PATTERN(GENERATE_ENUM)  } SOS_val_pattern;
typedef enum { FOREACH_VAL_COMPARE(GENERATE_ENUM)  } SOS_val_compare;
typedef enum { FOREACH_VAL_CLASS(GENERATE_ENUM)    } SOS_val_class;
typedef enum { FOREACH_MOOD(GENERATE_ENUM)         } SOS_mood;
typedef enum { FOREACH_SCOPE(GENERATE_ENUM)        } SOS_scope;
typedef enum { FOREACH_LAYER(GENERATE_ENUM)        } SOS_layer;
typedef enum { FOREACH_NATURE(GENERATE_ENUM)       } SOS_nature;
typedef enum { FOREACH_RETAIN(GENERATE_ENUM)       } SOS_retain;

static const char *SOS_ROLE_string[] =         { FOREACH_ROLE(GENERATE_STRING)         };
static const char *SOS_TARGET_string[] =       { FOREACH_TARGET(GENERATE_STRING)       };
static const char *SOS_STATUS_string[] =       { FOREACH_STATUS(GENERATE_STRING)       };
static const char *SOS_MSG_TYPE_string[] =     { FOREACH_MSG_TYPE(GENERATE_STRING)     };
static const char *SOS_PRI_string[] =          { FOREACH_PRI(GENERATE_STRING)          };
static const char *SOS_VAL_TYPE_string[] =     { FOREACH_VAL_TYPE(GENERATE_STRING)     };
static const char *SOS_VAL_STATE_string[] =    { FOREACH_VAL_STATE(GENERATE_STRING)    };
static const char *SOS_VAL_FREQ_string[] =     { FOREACH_VAL_FREQ(GENERATE_STRING)     };
static const char *SOS_VAL_SEMANTIC_string[] = { FOREACH_VAL_SEMANTIC(GENERATE_STRING) };
static const char *SOS_VAL_PATTERN_string[] =  { FOREACH_VAL_PATTERN(GENERATE_STRING)  };
static const char *SOS_VAL_COMPARE_string[] =  { FOREACH_VAL_COMPARE(GENERATE_STRING)  };
static const char *SOS_VAL_CLASS_string[] =    { FOREACH_VAL_CLASS(GENERATE_STRING)    };
static const char *SOS_MOOD_string[] =         { FOREACH_MOOD(GENERATE_STRING)         };
static const char *SOS_SCOPE_string[] =        { FOREACH_SCOPE(GENERATE_STRING)        };
static const char *SOS_LAYER_string[] =        { FOREACH_LAYER(GENERATE_STRING)        };
static const char *SOS_NATURE_string[] =       { FOREACH_NATURE(GENERATE_STRING)       };
static const char *SOS_RETAIN_string[] =       { FOREACH_RETAIN(GENERATE_STRING)       };

#define SOS_ENUM_IN_RANGE(__SOS_var_name, __SOS_max_name)  (__SOS_var_name >= 0 && __SOS_var_name < __SOS_max_name)
#define SOS_ENUM_STR(__SOS_var_name, __SOS_enum_type)  SOS_ENUM_IN_RANGE(__SOS_var_name, (__SOS_enum_type ## ___MAX)) ? __SOS_enum_type ## _string[__SOS_var_name] : "** " #__SOS_enum_type " is INVALID **";
/*  Examples:   char *pub_layer     = SOS_ENUM_STR( pub->meta.layer,        SOS_LAYER     );
 *              char *data_type     = SOS_ENUM_STR( pub->data[i]->type,     SOS_VAL_TYPE  );
 *              char *data_semantic = SOS_ENUM_STR( pub->data[i]->sem_hint, SOS_SEM       );
 */


typedef union {
    int                 i_val;        /* default: (null)                */
    long                l_val;        /* default: (null)                */
    double              d_val;        /* default: (null)                */
    char               *c_val;        /* default: (null)                */
} SOS_val;

typedef struct {
    char                data[SOS_DEFAULT_BUFFER_LEN];
    int                 char_count;
} SOS_buffer;

typedef struct {
    double              pack;         /* default: 0.0                   */
    double              send;         /* default: 0.0                   */
    double              recv;         /* default: 0.0                   */
} SOS_time;

typedef struct {
    SOS_val_freq        freq;
    SOS_val_semantic    semantic;
    SOS_val_class       class;
    SOS_val_pattern     pattern;
    SOS_val_compare     compare;
    SOS_mood            mood;
} SOS_val_meta;

typedef struct {
    int                 elem;
    long                guid;
    SOS_val             val;
    SOS_time            time;
    long                frame;
    void               *next;
} SOS_val_snap;

typedef struct {
    qhashtbl_t         *from;
    #if (SOS_CONFIG_USE_MUTEXES > 0)
    pthread_mutex_t    *lock;
    #endif
} SOS_val_snap_queue;

typedef struct {
    long                guid;         /* default: (auto)                */
    int                 val_len;      /* default: (auto) [on assign]    */
    SOS_val             val;          /* default: --------- manual      */
    SOS_val_type        type;         /* default: --------- manual      */
    SOS_val_meta        meta;
    SOS_val_state       state;        /* default: SOS_VAL_STATE_EMPTY   */
    SOS_time            time;         /* default: (complex)             */
    char               *name;         /* default: --------- manual      */
} SOS_data;

typedef struct {
    int                 channel;      /* default: 0                     */
    SOS_nature          nature;       /* default: --------- manual      */
    SOS_layer           layer;        /* default: SOS_LAYER_APP         */
    SOS_pri             pri_hint;     /* default: SOS_PRI_DEFAULT       */
    SOS_scope           scope_hint;   /* default: SOS_SCOPE_DEFAULT     */
    SOS_retain          retain_hint;  /* default: SOS_RETAIN_DEFAULT    */
} SOS_pub_meta;

typedef struct {
    long                guid;         /* default: (auto, on announce)   */
    int                 process_id;   /* default: -1                    */
    int                 thread_id;    /* default: -1                    */
    int                 comm_rank;    /* default: -1                    */
    SOS_pub_meta        meta;         /* default: (complex)             */
    int                 announced;    /* default: 0                     */
    long                frame;        /* default: 0                     */
    int                 elem_max;     /* default: SOS_DEFAULT_ELEM_MAX  */
    int                 elem_count;   /* default: 0                     */
    int                 pragma_len;   /* default: -1                    */
    char               *pragma_msg;   /* default: (null)                */
    char               *node_id;      /* default: SOS.config.node_id    */
    char               *prog_name;    /* default: argv[0] / manual      */
    char               *prog_ver;     /* default: (null)                */
    char               *title;        /* default: (null)                */
    SOS_data          **data;
} SOS_pub;

typedef struct {
    int                 suid;
    int                 active;
    int                 refresh_delay;
    SOS_role            source_role;
    int                 source_rank;
    SOS_pub            *pub;
    pthread_t           thread_handle;
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

typedef struct {                              /* no pointers, headers might get used raw */
    int                 msg_size;
    SOS_msg_type        msg_type;
    long                msg_from;
    long                pub_guid;
} SOS_msg_header;

typedef struct {
    int                 argc;
    char              **argv;
    char               *node_id;
    int                 comm_rank;
    int                 comm_size;
    int                 comm_support;
    int                 process_id;
    int                 thread_id;
} SOS_config;

typedef struct {
    long                next;
    long                last;
    pthread_mutex_t    *lock;
} SOS_uid;

typedef struct {
    SOS_uid            *local_serial;
    SOS_uid            *my_guid_pool;
} SOS_unique_set;

typedef struct {
    int                 read_elem;
    int                 write_elem;
    int                 elem_count;
    int                 elem_max;
    int                 elem_size;
    long               *heap;
    pthread_mutex_t    *lock;
} SOS_ring_queue;

typedef struct {
    SOS_ring_queue     *send;
    SOS_ring_queue     *recv;
} SOS_ring_set;

typedef struct {
    pthread_t          *post;
    pthread_t          *read;
    pthread_t          *scan;
    SOS_val_snap_queue *val_intake;
    SOS_val_snap_queue *val_outlet;
} SOS_task_set;

typedef struct {
    SOS_config          config;
    SOS_role            role;
    SOS_status          status;
    SOS_unique_set      uid;
    SOS_ring_set        ring;
    SOS_task_set        task;
    SOS_socket_set      net;
    long                my_guid;
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

    void      SOS_init( int *argc, char ***argv, SOS_role role );
    void      SOS_finalize();

    SOS_pub*  SOS_pub_create(char *pub_title);
    SOS_pub*  SOS_pub_create_sized(char *pub_title, int new_size);

    int       SOS_define_val( SOS_pub *pub, const char *name, SOS_val_type type, SOS_val_meta meta);
    int       SOS_pack( SOS_pub *pub, const char *name, SOS_val_type pack_type, SOS_val pack_val );
    void      SOS_repack( SOS_pub *pub, int index, SOS_val pack_val );

    void      SOS_announce( SOS_pub *pub );
    void      SOS_announce_to_buffer( SOS_pub *pub, char **buffer, int *buffer_len );
    void      SOS_announce_from_buffer( SOS_pub *pub, char *buffer );

    void      SOS_publish( SOS_pub *pub );
    void      SOS_publish_to_buffer( SOS_pub *pub, char **buffer, int *buffer_len );
    void      SOS_publish_from_buffer( SOS_pub *pub, char *buffer, SOS_val_snap_queue *opt_queue );

    void      SOS_val_snap_queue_init(SOS_val_snap_queue **queue);
    void      SOS_val_snap_enqueue(SOS_val_snap_queue *queue, SOS_pub *pub, int elem);
    void      SOS_val_snap_queue_to_buffer(SOS_val_snap_queue *queue, char **buffer, int *buffer_len);
    void      SOS_val_snap_queue_from_buffer(SOS_val_snap_queue *queue, char *buffer);
    void      SOS_val_snap_queue_destroy(SOS_val_snap_queue *queue);

    void      SOS_strip_str(char *str);

    void      SOS_ring_init(SOS_ring_queue **ring);
    void      SOS_ring_destroy(SOS_ring_queue *ring);
    int       SOS_ring_put(SOS_ring_queue *ring, long item);
    long      SOS_ring_get(SOS_ring_queue *ring);
    long*     SOS_ring_get_all(SOS_ring_queue *ring, int *elem_returning);


    /* NOTE: See [sos.c] and [sosd.c] for additional "private" functions. */


    /* ..... [ empty stubs ] ..... */
    void      SOS_display_pub(SOS_pub *pub, FILE *output_to);
    SOS_val   SOS_get_val( SOS_pub *pub, char *name );
    void      SOS_free_pub( SOS_pub *pub );
    void      SOS_free_sub( SOS_sub *sub );
    void      SOS_unannounce( SOS_pub *pub );
    SOS_sub*  SOS_new_sub();
    SOS_sub*  SOS_subscribe( SOS_role target_role, int target_rank, char *pub_title, int refresh_ms );
    void      SOS_unsubscribe( SOS_sub *sub );

#ifdef __cplusplus
}
#endif



#endif //SOS_H
