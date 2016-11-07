#ifndef SOS_TYPES_H
#define SOS_TYPES_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <limits.h>
#include <inttypes.h>
#include <sys/socket.h>
#include <netdb.h>

#include "sos_qhashtbl.h"
#include "sos_pipe.h"
#include "sos_buffer.h"

#define FOREACH_ROLE(ROLE)                      \
    ROLE(SOS_ROLE_UNASSIGNED)                   \
    ROLE(SOS_ROLE_CLIENT)                       \
    ROLE(SOS_ROLE_DAEMON)                       \
    ROLE(SOS_ROLE_DB)                           \
    ROLE(SOS_ROLE_ANALYTICS)                    \
    ROLE(SOS_ROLE_RUNTIME_UTILITY)              \
    ROLE(SOS_ROLE_OFFLINE_TEST_MODE)            \
    ROLE(SOS_ROLE___MAX)

#define FOREACH_TARGET(TARGET)                  \
    TARGET(SOS_TARGET_LOCAL_SYNC)               \
    TARGET(SOS_TARGET_CLOUD_SYNC)               \
    TARGET(SOS_TARGET___MAX)
    
#define FOREACH_STATUS(STATUS)                  \
    STATUS(SOS_STATUS_INIT)                     \
    STATUS(SOS_STATUS_RUNNING)                  \
    STATUS(SOS_STATUS_HALTING)                  \
    STATUS(SOS_STATUS_SHUTDOWN)                 \
    STATUS(SOS_STATUS___MAX)
    
#define FOREACH_MSG_TYPE(MSG_TYPE)              \
    MSG_TYPE(SOS_MSG_TYPE_NULLMSG)              \
    MSG_TYPE(SOS_MSG_TYPE_REGISTER)             \
    MSG_TYPE(SOS_MSG_TYPE_GUID_BLOCK)           \
    MSG_TYPE(SOS_MSG_TYPE_ANNOUNCE)             \
    MSG_TYPE(SOS_MSG_TYPE_PUBLISH)              \
    MSG_TYPE(SOS_MSG_TYPE_VAL_SNAPS)            \
    MSG_TYPE(SOS_MSG_TYPE_ECHO)                 \
    MSG_TYPE(SOS_MSG_TYPE_SHUTDOWN)             \
    MSG_TYPE(SOS_MSG_TYPE_ACK)                  \
    MSG_TYPE(SOS_MSG_TYPE_CHECK_IN)             \
    MSG_TYPE(SOS_MSG_TYPE_FEEDBACK)             \
    MSG_TYPE(SOS_MSG_TYPE_PROBE)                \
    MSG_TYPE(SOS_MSG_TYPE_QUERY)                \
    MSG_TYPE(SOS_MSG_TYPE___MAX)


#define FOREACH_FEEDBACK(FEEDBACK)              \
    FEEDBACK(SOS_FEEDBACK_CONTINUE)             \
    FEEDBACK(SOS_FEEDBACK_EXEC_FUNCTION)        \
    FEEDBACK(SOS_FEEDBACK_SET_PARAMETER)        \
    FEEDBACK(SOS_FEEDBACK_EFFECT_CHANGE)        \
    FEEDBACK(SOS_FEEDBACK___MAX)
    
#define FOREACH_PRI(PRI)                        \
    PRI(SOS_PRI_DEFAULT)                        \
    PRI(SOS_PRI_LOW)                            \
    PRI(SOS_PRI_IMMEDIATE)                      \
    PRI(SOS_PRI___MAX)

#define FOREACH_VOLUME(VOLUME)                  \
    VOLUME(SOS_VOLUME_HEXAHEDRON)               \
    VOLUME(SOS_VOLUME___MAX)

#define FOREACH_VAL_TYPE(VAL_TYPE)              \
    VAL_TYPE(SOS_VAL_TYPE_INT)                  \
    VAL_TYPE(SOS_VAL_TYPE_LONG)                 \
    VAL_TYPE(SOS_VAL_TYPE_DOUBLE)               \
    VAL_TYPE(SOS_VAL_TYPE_STRING)               \
    VAL_TYPE(SOS_VAL_TYPE_BYTES)                \
    VAL_TYPE(SOS_VAL_TYPE___MAX)

#define FOREACH_VAL_SYNC(VAL_SYNC)              \
    VAL_SYNC(SOS_VAL_SYNC_DEFAULT)              \
    VAL_SYNC(SOS_VAL_SYNC_RENEW)                \
    VAL_SYNC(SOS_VAL_SYNC_LOCAL)                \
    VAL_SYNC(SOS_VAL_SYNC_CLOUD)                \
    VAL_SYNC(SOS_VAL_SYNC___MAX)

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
    LAYER(SOS_LAYER_ENVIRONMENT)                \
    LAYER(SOS_LAYER_SOS_RUNTIME)                \
    LAYER(SOS_LAYER___MAX)

#define FOREACH_NATURE(NATURE)                  \
    NATURE(SOS_NATURE_DEFAULT)                  \
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

#define FOREACH_LOCALE(SOS_LOCALE)              \
    SOS_LOCALE(SOS_LOCALE_INDEPENDENT)          \
    SOS_LOCALE(SOS_LOCALE_DAEMON_DBMS)          \
    SOS_LOCALE(SOS_LOCALE_APPLICATION)          \
    SOS_LOCALE(SOS_LOCALE___MAX)


#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

typedef enum { FOREACH_ROLE(GENERATE_ENUM)         } SOS_role;
typedef enum { FOREACH_TARGET(GENERATE_ENUM)       } SOS_target;
typedef enum { FOREACH_STATUS(GENERATE_ENUM)       } SOS_status;
typedef enum { FOREACH_MSG_TYPE(GENERATE_ENUM)     } SOS_msg_type;
typedef enum { FOREACH_FEEDBACK(GENERATE_ENUM)     } SOS_feedback;
typedef enum { FOREACH_PRI(GENERATE_ENUM)          } SOS_pri;
typedef enum { FOREACH_VOLUME(GENERATE_ENUM)       } SOS_volume;
typedef enum { FOREACH_VAL_TYPE(GENERATE_ENUM)     } SOS_val_type;
typedef enum { FOREACH_VAL_STATE(GENERATE_ENUM)    } SOS_val_state;
typedef enum { FOREACH_VAL_SYNC(GENERATE_ENUM)     } SOS_val_sync;
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
typedef enum { FOREACH_LOCALE(GENERATE_ENUM)       } SOS_locale;

static const char *SOS_ROLE_string[] =         { FOREACH_ROLE(GENERATE_STRING)         };
static const char *SOS_TARGET_string[] =       { FOREACH_TARGET(GENERATE_STRING)       };
static const char *SOS_STATUS_string[] =       { FOREACH_STATUS(GENERATE_STRING)       };
static const char *SOS_MSG_TYPE_string[] =     { FOREACH_MSG_TYPE(GENERATE_STRING)     };
static const char *SOS_FEEDBACK_string[] =     { FOREACH_FEEDBACK(GENERATE_STRING)     };
static const char *SOS_PRI_string[] =          { FOREACH_PRI(GENERATE_STRING)          };
static const char *SOS_VOLUME_string[] =       { FOREACH_VOLUME(GENERATE_STRING)       };
static const char *SOS_VAL_TYPE_string[] =     { FOREACH_VAL_TYPE(GENERATE_STRING)     };
static const char *SOS_VAL_STATE_string[] =    { FOREACH_VAL_STATE(GENERATE_STRING)    };
static const char *SOS_VAL_SYNC_string[] =     { FOREACH_VAL_SYNC(GENERATE_STRING)     };
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
static const char *SOS_LOCALE_string[] =       { FOREACH_LOCALE(GENERATE_STRING)       };

#define SOS_ENUM_IN_RANGE(__SOS_var_name, __SOS_max_name)  (__SOS_var_name >= 0 && __SOS_var_name < __SOS_max_name)
#define SOS_ENUM_STR(__SOS_var_name, __SOS_enum_type)  SOS_ENUM_IN_RANGE(__SOS_var_name, (__SOS_enum_type ## ___MAX)) ? __SOS_enum_type ## _string[__SOS_var_name] : "** " #__SOS_enum_type " is INVALID **"
/*  Examples:   char *pub_layer     = SOS_ENUM_STR( pub->meta.layer,        SOS_LAYER     );
 *              char *data_type     = SOS_ENUM_STR( pub->data[i]->type,     SOS_VAL_TYPE  );
 *              char *data_semantic = SOS_ENUM_STR( pub->data[i]->sem_hint, SOS_SEM       );
 */


typedef     uint64_t SOS_guid;
#define SOS_GUID_FMT PRIu64


typedef union {
    int                 i_val;
    long                l_val;
    double              d_val;
    char               *c_val;
    void               *bytes;   /* Use addr. of SOS_buffer object. */
} SOS_val;

//
//  SOS_position:
//
//        [Z]
//         | [Y]
//         | /
//         |/
//  . . ... ------[X]
//        ,:
//       . .
//      .  .
//
typedef struct {
    double              x;
    double              y;
    double              z;
} SOS_position;

//
//  SOS_volume_hexahedron:
//
//          [p7]---------[p6]
//         /________    / |
//      [p4]--------[p5]  |
//       ||  |       ||   |
//       || [p3]-----||--[p2]
//       ||/________ || /
//      [p0]--------[p1]
//
typedef struct {
    SOS_position        p0;
    SOS_position        p1;
    SOS_position        p2;
    SOS_position        p3;
    SOS_position        p4;
    SOS_position        p5;
    SOS_position        p6;
    SOS_position        p7;
} SOS_volume_hexahedron;


typedef struct {
    double              pack;
    double              send;
    double              recv;
} SOS_time;


typedef struct {
    SOS_val_freq        freq;
    SOS_val_semantic    semantic;
    SOS_val_class       classifier;
    SOS_val_pattern     pattern;
    SOS_val_compare     compare;
    SOS_mood            mood;
} SOS_val_meta;

typedef struct {
    int                 elem;
    SOS_guid            guid;
    SOS_val             val;
    int                 val_len;
    SOS_val_type        type;
    SOS_time            time;
    long                frame;
    SOS_val_semantic    semantic;
    SOS_mood            mood;
} SOS_val_snap;

typedef struct {
    SOS_guid            guid;
    int                 val_len;
    SOS_val             val;
    SOS_val_type        type;
    SOS_val_meta        meta;
    SOS_val_state       state;
    SOS_val_sync        sync;
    SOS_time            time;
    char                name[SOS_DEFAULT_STRING_LEN];
} SOS_data;

typedef struct {
    int                 channel;
    SOS_nature          nature;
    SOS_layer           layer;
    SOS_pri             pri_hint;
    SOS_scope           scope_hint;
    SOS_retain          retain_hint;
} SOS_pub_meta;

typedef struct {
    void               *sos_context;
    pthread_mutex_t    *lock;
    int                 sync_pending;
    SOS_guid            guid;
    char                guid_str[SOS_DEFAULT_STRING_LEN];
    int                 process_id;
    int                 thread_id;
    int                 comm_rank;
    SOS_pub_meta        meta;
    int                 announced;
    long                frame;
    int                 elem_max;
    int                 elem_count;
    int                 pragma_len;
    unsigned char       pragma_msg[SOS_DEFAULT_STRING_LEN];
    char                node_id[SOS_DEFAULT_STRING_LEN];
    char                prog_name[SOS_DEFAULT_STRING_LEN];
    char                prog_ver[SOS_DEFAULT_STRING_LEN];
    char                title[SOS_DEFAULT_STRING_LEN];
    SOS_data          **data;
    qhashtbl_t         *name_table;
    SOS_pipe           *snap_queue;
} SOS_pub;


typedef struct {
    SOS_guid            guid;
    SOS_guid            client_guid;
    char                handle[SOS_DEFAULT_STRING_LEN];
    void               *target;
    SOS_feedback        target_type;
    int                 daemon_trigger_count;
    int                 client_receipt_count;
} SOS_sensitivity;


typedef struct {
    SOS_guid            guid;
    SOS_guid            source_guid;
    char                handle[SOS_DEFAULT_STRING_LEN];
    SOS_feedback        feedback;
    void               *data;
    int                 data_len;
    int                 apply_count; /* -1 == constant */
} SOS_action;


typedef struct {
    char               *sql;
    int                 sql_len;
    SOS_action          action;
} SOS_trigger;


typedef struct {
    char               *server_host;
    char               *server_port;
    struct addrinfo    *server_addr;
    struct addrinfo    *result_list;
    struct addrinfo     server_hint;
    struct addrinfo    *client_addr;
    int                 timeout;
    int                 buffer_len;
    pthread_mutex_t    *send_lock;
    SOS_buffer         *recv_part;
} SOS_socket_set;

typedef struct {
    int                 msg_size;
    SOS_msg_type        msg_type;
    SOS_guid            msg_from;
    SOS_guid            pub_guid;
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
    SOS_layer           layer;
    SOS_locale          locale;
    bool                offline_test_mode;
    bool                runtime_utility;
} SOS_config;

typedef struct {
    void               *sos_context;
    SOS_guid            next;
    SOS_guid            last;
    pthread_mutex_t    *lock;
} SOS_uid;

typedef struct {
    SOS_uid            *local_serial;
    SOS_uid            *my_guid_pool;
} SOS_unique_set;


typedef struct {
    bool                feedback_active;
    pthread_t          *feedback;
    pthread_mutex_t    *feedback_lock;
    pthread_cond_t     *feedback_cond;
    qhashtbl_t         *sense_table;
} SOS_task_set;

typedef struct {
    SOS_config          config;
    SOS_role            role;
    SOS_status          status;
    SOS_unique_set      uid;
    SOS_task_set        task;
    SOS_socket_set      net;
    SOS_guid            my_guid;
} SOS_runtime;


#endif
