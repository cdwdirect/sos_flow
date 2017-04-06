# file "example_build.py"

# Note: we instantiate the same 'cffi.FFI' class as in the previous
# example, but call the result 'ffibuilder' now instead of 'ffi';
# this is to avoid confusion with the other 'ffi' object you get below

from cffi import FFI
ffibuilder = FFI()

ffibuilder.cdef("""    

    typedef long unsigned int size_t;
    typedef unsigned char __u_char;
    typedef unsigned short int __u_short;
    typedef unsigned int __u_int;
    typedef unsigned long int __u_long;
    typedef signed char __int8_t;
    typedef unsigned char __uint8_t;
    typedef signed short int __int16_t;
    typedef unsigned short int __uint16_t;
    typedef signed int __int32_t;
    typedef unsigned int __uint32_t;
    typedef signed long int __int64_t;
    typedef unsigned long int __uint64_t;
    typedef long int __quad_t;
    typedef unsigned long int __u_quad_t;
    typedef unsigned long int __dev_t;
    typedef unsigned int __uid_t;
    typedef unsigned int __gid_t;
    typedef unsigned long int __ino_t;
    typedef unsigned long int __ino64_t;
    typedef unsigned int __mode_t;
    typedef unsigned long int __nlink_t;
    typedef long int __off_t;
    typedef long int __off64_t;
    typedef int __pid_t;
    typedef struct { int __val[2]; } __fsid_t;
    typedef long int __clock_t;
    typedef unsigned long int __rlim_t;
    typedef unsigned long int __rlim64_t;
    typedef unsigned int __id_t;
    typedef long int __time_t;
    typedef unsigned int __useconds_t;
    typedef long int __suseconds_t;
    typedef int __daddr_t;
    typedef int __key_t;
    typedef int __clockid_t;
    typedef void * __timer_t;
    typedef long int __blksize_t;
    typedef long int __blkcnt_t;
    typedef long int __blkcnt64_t;
    typedef unsigned long int __fsblkcnt_t;
    typedef unsigned long int __fsblkcnt64_t;
    typedef unsigned long int __fsfilcnt_t;
    typedef unsigned long int __fsfilcnt64_t;
    typedef long int __fsword_t;
    typedef long int __ssize_t;
    typedef long int __syscall_slong_t;
    typedef unsigned long int __syscall_ulong_t;
    typedef __off64_t __loff_t;
    typedef __quad_t *__qaddr_t;
    typedef char *__caddr_t;
    typedef long int __intptr_t;
    typedef unsigned int __socklen_t;

    struct _IO_FILE;
    typedef struct _IO_FILE FILE;
    typedef struct _IO_FILE __FILE;
    typedef struct
{
    int __count;
    union
    {
        unsigned int __wch;
        char __wchb[4];
    } __value;
} __mbstate_t;
typedef struct
{
    __off_t __pos;
    __mbstate_t __state;
} _G_fpos_t;
typedef struct
{
    __off64_t __pos;
    __mbstate_t __state;
} _G_fpos64_t;

typedef void* __builtin_va_list;
typedef void* __gnuc_va_list;


struct _IO_jump_t; struct _IO_FILE;
typedef void _IO_lock_t;
struct _IO_marker {
    struct _IO_marker *_next;
    struct _IO_FILE *_sbuf;
    int _pos;
};
enum __codecvt_result
{
    __codecvt_ok,
    __codecvt_partial,
    __codecvt_error,
    __codecvt_noconv
};
struct _IO_FILE {
    int _flags;
    char* _IO_read_ptr;
    char* _IO_read_end;
    char* _IO_read_base;
    char* _IO_write_base;
    char* _IO_write_ptr;
    char* _IO_write_end;
    char* _IO_buf_base;
    char* _IO_buf_end;
    char *_IO_save_base;
    char *_IO_backup_base;
    char *_IO_save_end;
    struct _IO_marker *_markers;
    struct _IO_FILE *_chain;
    int _fileno;
    int _flags2;
    __off_t _old_offset;
    unsigned short _cur_column;
    signed char _vtable_offset;
    char _shortbuf[1];
    _IO_lock_t *_lock;
    __off64_t _offset;
    void *__pad1;
    void *__pad2;
    void *__pad3;
    void *__pad4;
    size_t __pad5;
    int _mode;
    char _unused2[1024];
};
typedef struct _IO_FILE _IO_FILE;
struct _IO_FILE_plus;
extern struct _IO_FILE_plus _IO_2_1_stdin_;
extern struct _IO_FILE_plus _IO_2_1_stdout_;
extern struct _IO_FILE_plus _IO_2_1_stderr_;
typedef __gnuc_va_list va_list;
typedef __off_t off_t;
typedef __ssize_t ssize_t;

typedef _G_fpos_t fpos_t;

extern struct _IO_FILE *stdin;
extern struct _IO_FILE *stdout;
extern struct _IO_FILE *stderr;

typedef signed char int8_t;
typedef short int int16_t;
typedef int int32_t;
typedef long int int64_t;
typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long int uint64_t;
typedef signed char int_least8_t;
typedef short int int_least16_t;
typedef int int_least32_t;
typedef long int int_least64_t;
typedef unsigned char uint_least8_t;
typedef unsigned short int uint_least16_t;
typedef unsigned int uint_least32_t;
typedef unsigned long int uint_least64_t;
typedef signed char int_fast8_t;
typedef long int int_fast16_t;
typedef long int int_fast32_t;
typedef long int int_fast64_t;
typedef unsigned char uint_fast8_t;
typedef unsigned long int uint_fast16_t;
typedef unsigned long int uint_fast32_t;
typedef unsigned long int uint_fast64_t;
typedef long int intptr_t;
typedef unsigned long int uintptr_t;
typedef long int intmax_t;
typedef unsigned long int uintmax_t;
typedef __time_t time_t;


struct timespec
{
    __time_t tv_sec;
    __syscall_slong_t tv_nsec;
};
typedef __pid_t pid_t;
struct sched_param
{
    int __sched_priority;
};


struct __sched_param
{
    int __sched_priority;
};
typedef unsigned long int __cpu_mask;
typedef struct
{
    __cpu_mask __bits[1024];
} cpu_set_t;



typedef __clock_t clock_t;


typedef __clockid_t clockid_t;
typedef __timer_t timer_t;

struct tm
{
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
    long int tm_gmtoff;
    const char *tm_zone;
};


struct itimerspec
{
    struct timespec it_interval;
    struct timespec it_value;
};
struct sigevent;

typedef struct __locale_struct
{
    struct __locale_data *__locales[13];
    const unsigned short int *__ctype_b;
    const int *__ctype_tolower;
    const int *__ctype_toupper;
    const char *__names[13];
} *__locale_t;
typedef __locale_t locale_t;
extern char *__tzname[2];
extern int __daylight;
extern long int __timezone;
extern char *tzname[2];
extern int daylight;
extern long int timezone;

typedef unsigned long int pthread_t;
union pthread_attr_t
{
    char __size[56];
    long int __align;
};
typedef union pthread_attr_t pthread_attr_t;
typedef struct __pthread_internal_list
{
    struct __pthread_internal_list *__prev;
    struct __pthread_internal_list *__next;
} __pthread_list_t;
typedef union
{
    struct __pthread_mutex_s
    {
        int __lock;
        unsigned int __count;
        int __owner;
        unsigned int __nusers;
        int __kind;
        short __spins;
        short __elision;
        __pthread_list_t __list;
    } __data;
    char __size[40];
    long int __align;
} pthread_mutex_t;
typedef union
{
    char __size[4];
    int __align;
} pthread_mutexattr_t;
typedef union
{
    struct
    {
        int __lock;
        unsigned int __futex;
        unsigned long long int __total_seq;
        unsigned long long int __wakeup_seq;
        unsigned long long int __woken_seq;
        void *__mutex;
        unsigned int __nwaiters;
        unsigned int __broadcast_seq;
    } __data;
    char __size[48];
    long long int __align;
} pthread_cond_t;
typedef union
{
    char __size[4];
    int __align;
} pthread_condattr_t;
typedef unsigned int pthread_key_t;
typedef int pthread_once_t;
typedef union
{
    struct
    {
        int __lock;
        unsigned int __nr_readers;
        unsigned int __readers_wakeup;
        unsigned int __writer_wakeup;
        unsigned int __nr_readers_queued;
        unsigned int __nr_writers_queued;
        int __writer;
        int __shared;
        unsigned long int __pad1;
        unsigned long int __pad2;
        unsigned int __flags;
    } __data;
    char __size[56];
    long int __align;
} pthread_rwlock_t;
typedef union
{
    char __size[8];
    long int __align;
} pthread_rwlockattr_t;
typedef volatile int pthread_spinlock_t;
typedef union
{
    char __size[32];
    long int __align;
} pthread_barrier_t;
typedef union
{
    char __size[4];
    int __align;
} pthread_barrierattr_t;
typedef long int __jmp_buf[8];
enum
{
    PTHREAD_CREATE_JOINABLE,
    PTHREAD_CREATE_DETACHED
};
enum
{
    PTHREAD_MUTEX_TIMED_NP,
    PTHREAD_MUTEX_RECURSIVE_NP,
    PTHREAD_MUTEX_ERRORCHECK_NP,
    PTHREAD_MUTEX_ADAPTIVE_NP
        ,
    PTHREAD_MUTEX_NORMAL = PTHREAD_MUTEX_TIMED_NP,
    PTHREAD_MUTEX_RECURSIVE = PTHREAD_MUTEX_RECURSIVE_NP,
    PTHREAD_MUTEX_ERRORCHECK = PTHREAD_MUTEX_ERRORCHECK_NP,
    PTHREAD_MUTEX_DEFAULT = PTHREAD_MUTEX_NORMAL
};
enum
{
    PTHREAD_MUTEX_STALLED,
    PTHREAD_MUTEX_STALLED_NP = PTHREAD_MUTEX_STALLED,
    PTHREAD_MUTEX_ROBUST,
    PTHREAD_MUTEX_ROBUST_NP = PTHREAD_MUTEX_ROBUST
};
enum
{
    PTHREAD_PRIO_NONE,
    PTHREAD_PRIO_INHERIT,
    PTHREAD_PRIO_PROTECT
};
enum
{
    PTHREAD_RWLOCK_PREFER_READER_NP,
    PTHREAD_RWLOCK_PREFER_WRITER_NP,
    PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP,
    PTHREAD_RWLOCK_DEFAULT_NP = PTHREAD_RWLOCK_PREFER_READER_NP
};
enum
{
    PTHREAD_INHERIT_SCHED,
    PTHREAD_EXPLICIT_SCHED
};
enum
{
    PTHREAD_SCOPE_SYSTEM,
    PTHREAD_SCOPE_PROCESS
};
enum
{
    PTHREAD_PROCESS_PRIVATE,
    PTHREAD_PROCESS_SHARED
};
struct _pthread_cleanup_buffer
{
    void (*__routine) (void *);
    void *__arg;
    int __canceltype;
    struct _pthread_cleanup_buffer *__prev;
};
enum
{
    PTHREAD_CANCEL_ENABLE,
    PTHREAD_CANCEL_DISABLE
};
enum
{
    PTHREAD_CANCEL_DEFERRED,
    PTHREAD_CANCEL_ASYNCHRONOUS
};

typedef struct
{
    struct
    {
        __jmp_buf __cancel_jmp_buf;
        int __mask_was_saved;
    } __cancel_jmp_buf[1];
    void *__pad[4];
} __pthread_unwind_buf_t ; //
struct __pthread_cleanup_frame
{
    void (*__cancel_routine) (void *);
    void *__cancel_arg;
    int __do_it;
    int __cancel_type;
};
    struct __jmp_buf_tag;
struct timeval
{
    __time_t tv_sec;
    __suseconds_t tv_usec;
};
typedef int __sig_atomic_t;
typedef struct
{
    unsigned long int __val[1024];
} __sigset_t;
typedef __sigset_t sigset_t;
typedef __suseconds_t suseconds_t;
typedef long int __fd_mask;
typedef struct
{
    __fd_mask __fds_bits[1024];
} fd_set;
typedef __fd_mask fd_mask;


struct timezone
{
    int tz_minuteswest;
    int tz_dsttime;
};
typedef struct timezone *     __timezone_ptr_t;
enum __itimer_which
{
    ITIMER_REAL = 0,
    ITIMER_VIRTUAL = 1,
    ITIMER_PROF = 2
};
struct itimerval
{
    struct timeval it_interval;
    struct timeval it_value;
};
typedef int __itimer_which_t;
    typedef int __gwchar_t;

    typedef struct
{
    long int quot;
    long int rem;
} imaxdiv_t;

    typedef __u_char u_char;
    typedef __u_short u_short;
    typedef __u_int u_int;
    typedef __u_long u_long;
    typedef __quad_t quad_t;
    typedef __u_quad_t u_quad_t;
    typedef __fsid_t fsid_t;
    typedef __loff_t loff_t;
    typedef __ino_t ino_t;
    typedef __dev_t dev_t;
    typedef __gid_t gid_t;
    typedef __mode_t mode_t;
    typedef __nlink_t nlink_t;
    typedef __uid_t uid_t;
    typedef __id_t id_t;
    typedef __daddr_t daddr_t;
    typedef __caddr_t caddr_t;
    typedef __key_t key_t;
    typedef unsigned long int ulong;
    typedef unsigned short int ushort;
    typedef unsigned int uint;
    typedef unsigned int u_int8_t ; //
    typedef unsigned int u_int16_t ; //
    typedef unsigned int u_int32_t ; //
    typedef unsigned int u_int64_t ; //
    typedef int register_t ; //


    typedef __blksize_t blksize_t;
    typedef __blkcnt_t blkcnt_t;
    typedef __fsblkcnt_t fsblkcnt_t;
    typedef __fsfilcnt_t fsfilcnt_t;


    struct iovec
{
    void *iov_base;
    size_t iov_len;
};
typedef __socklen_t socklen_t;
enum __socket_type
{
    SOCK_STREAM = 1,
    SOCK_DGRAM = 2,
    SOCK_RAW = 3,
    SOCK_RDM = 4,
    SOCK_SEQPACKET = 5,
    SOCK_DCCP = 6,
    SOCK_PACKET = 10,
    SOCK_CLOEXEC = 02000000,
    SOCK_NONBLOCK = 00004000
};
typedef unsigned short int sa_family_t;
struct sockaddr
{
    sa_family_t sa_family;
    char sa_data[14];
};
struct sockaddr_storage
{
    sa_family_t ss_family;
    unsigned long int __ss_align;
    char __ss_padding[128];
};
enum
{
    MSG_OOB = 0x01,
    MSG_PEEK = 0x02,
    MSG_DONTROUTE = 0x04,
    MSG_CTRUNC = 0x08,
    MSG_PROXY = 0x10,
    MSG_TRUNC = 0x20,
    MSG_DONTWAIT = 0x40,
    MSG_EOR = 0x80,
    MSG_WAITALL = 0x100,
    MSG_FIN = 0x200,
    MSG_SYN = 0x400,
    MSG_CONFIRM = 0x800,
    MSG_RST = 0x1000,
    MSG_ERRQUEUE = 0x2000,
    MSG_NOSIGNAL = 0x4000,
    MSG_MORE = 0x8000,
    MSG_WAITFORONE = 0x10000,
    MSG_FASTOPEN = 0x20000000,
    MSG_CMSG_CLOEXEC = 0x40000000
};
struct msghdr
{
    void *msg_name;
    socklen_t msg_namelen;
    struct iovec *msg_iov;
    size_t msg_iovlen;
    void *msg_control;
    size_t msg_controllen;
    int msg_flags;
};
struct cmsghdr
{
    size_t cmsg_len;
    int cmsg_level;
    int cmsg_type;
    unsigned char __cmsg_data [];
};
enum
{
    SCM_RIGHTS = 0x01
};
struct linger
{
    int l_onoff;
    int l_linger;
};
struct osockaddr
{
    unsigned short int sa_family;
    unsigned char sa_data[14];
};
enum
{
    SHUT_RD = 0,
    SHUT_WR,
    SHUT_RDWR
};

typedef uint32_t in_addr_t;
struct in_addr
{
    in_addr_t s_addr;
};
struct ip_opts
{
    struct in_addr ip_dst;
    char ip_opts[40];
};
struct ip_mreqn
{
    struct in_addr imr_multiaddr;
    struct in_addr imr_address;
    int imr_ifindex;
};
struct in_pktinfo
{
    int ipi_ifindex;
    struct in_addr ipi_spec_dst;
    struct in_addr ipi_addr;
};
enum
{
    IPPROTO_IP = 0,
    IPPROTO_ICMP = 1,
    IPPROTO_IGMP = 2,
    IPPROTO_IPIP = 4,
    IPPROTO_TCP = 6,
    IPPROTO_EGP = 8,
    IPPROTO_PUP = 12,
    IPPROTO_UDP = 17,
    IPPROTO_IDP = 22,
    IPPROTO_TP = 29,
    IPPROTO_DCCP = 33,
    IPPROTO_IPV6 = 41,
    IPPROTO_RSVP = 46,
    IPPROTO_GRE = 47,
    IPPROTO_ESP = 50,
    IPPROTO_AH = 51,
    IPPROTO_MTP = 92,
    IPPROTO_BEETPH = 94,
    IPPROTO_ENCAP = 98,
    IPPROTO_PIM = 103,
    IPPROTO_COMP = 108,
    IPPROTO_SCTP = 132,
    IPPROTO_UDPLITE = 136,
    IPPROTO_RAW = 255,
    IPPROTO_MAX
};
enum
{
    IPPROTO_HOPOPTS = 0,
    IPPROTO_ROUTING = 43,
    IPPROTO_FRAGMENT = 44,
    IPPROTO_ICMPV6 = 58,
    IPPROTO_NONE = 59,
    IPPROTO_DSTOPTS = 60,
    IPPROTO_MH = 135
};
typedef uint16_t in_port_t;
enum
{
    IPPORT_ECHO = 7,
    IPPORT_DISCARD = 9,
    IPPORT_SYSTAT = 11,
    IPPORT_DAYTIME = 13,
    IPPORT_NETSTAT = 15,
    IPPORT_FTP = 21,
    IPPORT_TELNET = 23,
    IPPORT_SMTP = 25,
    IPPORT_TIMESERVER = 37,
    IPPORT_NAMESERVER = 42,
    IPPORT_WHOIS = 43,
    IPPORT_MTP = 57,
    IPPORT_TFTP = 69,
    IPPORT_RJE = 77,
    IPPORT_FINGER = 79,
    IPPORT_TTYLINK = 87,
    IPPORT_SUPDUP = 95,
    IPPORT_EXECSERVER = 512,
    IPPORT_LOGINSERVER = 513,
    IPPORT_CMDSERVER = 514,
    IPPORT_EFSSERVER = 520,
    IPPORT_BIFFUDP = 512,
    IPPORT_WHOSERVER = 513,
    IPPORT_ROUTESERVER = 520,
    IPPORT_RESERVED = 1024,
    IPPORT_USERRESERVED = 5000
};
struct in6_addr
{
    union
    {
        uint8_t __u6_addr8[16];
        uint16_t __u6_addr16[8];
        uint32_t __u6_addr32[4];
    } __in6_u;
};
extern const struct in6_addr in6addr_any;
extern const struct in6_addr in6addr_loopback;
struct sockaddr_in
{
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
    unsigned char sin_zero[2048];
};
struct sockaddr_in6
{
    sa_family_t sin6_family;
    in_port_t sin6_port;
    uint32_t sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t sin6_scope_id;
};
struct ip_mreq
{
    struct in_addr imr_multiaddr;
    struct in_addr imr_interface;
};
struct ip_mreq_source
{
    struct in_addr imr_multiaddr;
    struct in_addr imr_interface;
    struct in_addr imr_sourceaddr;
};
struct ipv6_mreq
{
    struct in6_addr ipv6mr_multiaddr;
    unsigned int ipv6mr_interface;
};
struct group_req
{
    uint32_t gr_interface;
    struct sockaddr_storage gr_group;
};
struct group_source_req
{
    uint32_t gsr_interface;
    struct sockaddr_storage gsr_group;
    struct sockaddr_storage gsr_source;
};
struct ip_msfilter
{
    struct in_addr imsf_multiaddr;
    struct in_addr imsf_interface;
    uint32_t imsf_fmode;
    uint32_t imsf_numsrc;
    struct in_addr imsf_slist[1];
};
struct group_filter
{
    uint32_t gf_interface;
    struct sockaddr_storage gf_group;
    uint32_t gf_fmode;
    uint32_t gf_numsrc;
    struct sockaddr_storage gf_slist[1];
};
    struct rpcent
{
    char *r_name;
    char **r_aliases;
    int r_number;
};
struct netent
{
    char *n_name;
    char **n_aliases;
    int n_addrtype;
    uint32_t n_net;
};

struct hostent
{
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};
struct servent
{
    char *s_name;
    char **s_aliases;
    int s_port;
    char *s_proto;
};
struct addrinfo
{
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};
typedef struct qhnobj_s qhnobj_t;
typedef struct qhslot_s qhslot_t;
typedef struct qhashtbl_s qhashtbl_t;
struct qhnobj_s {
    uint32_t hash;
    char *key;
    void *value;
    qhnobj_t *next;
};
struct qhslot_s {
    qhnobj_t *head;
    qhnobj_t *tail;
};

struct qhashtbl_s {
    _Bool (*put) (qhashtbl_t *tbl, const char *fullpath, const void *data);
    _Bool (*put2) (qhashtbl_t *tbl, const char *path, const char *name, const void *data);
    void *(*get) (qhashtbl_t *tbl, const char *fullpath);
    void *(*get2) (qhashtbl_t *tbl, const char *path, const char *name);
    _Bool (*remove) (qhashtbl_t *tbl, const char *fullpath);
    int (*size) (qhashtbl_t *tbl);
    void (*clear) (qhashtbl_t *tbl);
    void (*debug) (qhashtbl_t *tbl, FILE *out, _Bool detailed);
    void (*free) (qhashtbl_t *tbl);
    int num;
    int range;
    qhslot_t *slots;
    int ncalls_get;
    int nwalks_get;
    int ncalls_put;
    int nwalks_put;
};
typedef long int ptrdiff_t;
typedef int wchar_t;
typedef struct pipe_t pipe_t;
typedef struct pipe_producer_t pipe_producer_t;
typedef struct pipe_consumer_t pipe_consumer_t;
typedef struct pipe_generic_t pipe_generic_t;
typedef struct {
    void *sos_context;
    pipe_producer_t *intake;
    pipe_consumer_t *outlet;
    size_t elem_size;
    int elem_count;
    pthread_mutex_t *sync_lock;
    pthread_cond_t *sync_cond;
    int sync_pending;
} SOS_pipe;


void SOS_pipe_init(void *sos_context, SOS_pipe **pipe_obj, size_t elem_size);
extern const unsigned short int **__ctype_b_loc (void);
extern const __int32_t **__ctype_tolower_loc (void);
extern const __int32_t **__ctype_toupper_loc (void);


    typedef enum
{
    P_ALL,
    P_PID,
    P_PGID
} idtype_t;
union wait
{
    int w_status;
    struct
    {
        unsigned int __w_termsig:7;
        unsigned int __w_coredump:1;
        unsigned int __w_retcode:8;
        unsigned int:16;
    } __wait_terminated;
    struct
    {
        unsigned int __w_stopval:8;
        unsigned int __w_stopsig:8;
        unsigned int:16;
    } __wait_stopped;
};
typedef union
{
    union wait *__uptr;
    int *__iptr;
} __WAIT_STATUS ; //

typedef struct
{
    int quot;
    int rem;
} div_t;
typedef struct
{
    long int quot;
    long int rem;
} ldiv_t;


typedef struct
{
    long long int quot;
    long long int rem;
} lldiv_t;

struct random_data
{
    int32_t *fptr;
    int32_t *rptr;
    int32_t *state;
    int rand_type;
    int rand_deg;
    int rand_sep;
    int32_t *end_ptr;
};
   struct drand48_data
{
    unsigned short int __x[3];
    unsigned short int __old_x[3];
    unsigned short int __c;
    unsigned short int __init;
    unsigned long long int __a;
};
//
//
// typedef void (*SOS_feedback_handler_f)(SOS_feedback feedback, SOS_buffer *msg);
//
//
//

typedef void* SOS_feedback_handler_f;

typedef enum { SOS_ROLE_UNASSIGNED, SOS_ROLE_CLIENT, SOS_ROLE_LISTENER, SOS_ROLE_AGGREGATOR, SOS_ROLE_ANALYTICS, SOS_ROLE_RUNTIME_UTILITY, SOS_ROLE_OFFLINE_TEST_MODE, SOS_ROLE___MAX, } SOS_role;
typedef enum { SOS_TARGET_LOCAL_SYNC, SOS_TARGET_CLOUD_SYNC, SOS_TARGET___MAX, } SOS_target;
typedef enum { SOS_STATUS_INIT, SOS_STATUS_RUNNING, SOS_STATUS_HALTING, SOS_STATUS_SHUTDOWN, SOS_STATUS___MAX, } SOS_status;
typedef enum { SOS_MSG_TYPE_NULLMSG, SOS_MSG_TYPE_REGISTER, SOS_MSG_TYPE_GUID_BLOCK, SOS_MSG_TYPE_ANNOUNCE, SOS_MSG_TYPE_PUBLISH, SOS_MSG_TYPE_VAL_SNAPS, SOS_MSG_TYPE_ECHO, SOS_MSG_TYPE_SHUTDOWN, SOS_MSG_TYPE_ACK, SOS_MSG_TYPE_CHECK_IN, SOS_MSG_TYPE_FEEDBACK, SOS_MSG_TYPE_SENSITIVITY, SOS_MSG_TYPE_TRIGGERPULL, SOS_MSG_TYPE_PROBE, SOS_MSG_TYPE_QUERY, SOS_MSG_TYPE_KMEAN_DATA, SOS_MSG_TYPE___MAX, } SOS_msg_type;
typedef enum { SOS_RECEIVES_DIRECT_MESSAGES, SOS_RECEIVES_TIMED_CHECKIN, SOS_RECEIVES_MANUAL_CHECKIN, SOS_RECEIVES_NO_FEEDBACK, SOS_RECEIVES_DAEMON_MODE, SOS_RECEIVES___MAX, } SOS_receives;
typedef enum { SOS_FEEDBACK_CONTINUE, SOS_FEEDBACK_CUSTOM, SOS_FEEDBACK___MAX, } SOS_feedback;
typedef enum { SOS_PRI_DEFAULT, SOS_PRI_LOW, SOS_PRI_IMMEDIATE, SOS_PRI___MAX, } SOS_pri;
typedef enum { SOS_GEOMETRY_POINT, SOS_GEOMETRY_HEXAHEDRON, SOS_GEOMETRY___MAX, } SOS_geometry;
typedef enum { SOS_VAL_TYPE_INT, SOS_VAL_TYPE_LONG, SOS_VAL_TYPE_DOUBLE, SOS_VAL_TYPE_STRING, SOS_VAL_TYPE_BYTES, SOS_VAL_TYPE___MAX, } SOS_val_type;
typedef enum { SOS_VAL_STATE_CLEAN, SOS_VAL_STATE_DIRTY, SOS_VAL_STATE_EMPTY, SOS_VAL_STATE___MAX, } SOS_val_state;
typedef enum { SOS_VAL_SYNC_DEFAULT, SOS_VAL_SYNC_RENEW, SOS_VAL_SYNC_LOCAL, SOS_VAL_SYNC_CLOUD, SOS_VAL_SYNC___MAX, } SOS_val_sync;
typedef enum { SOS_VAL_SEMANTIC_DEFAULT, SOS_VAL_SEMANTIC_TIME_START, SOS_VAL_SEMANTIC_TIME_STOP, SOS_VAL_SEMANTIC_TIME_STAMP, SOS_VAL_SEMANTIC_TIME_SPAN, SOS_VAL_SEMANTIC_SAMPLE, SOS_VAL_SEMANTIC_COUNTER, SOS_VAL_SEMANTIC_LOG, SOS_VAL_SEMANTIC___MAX, } SOS_val_semantic;
typedef enum { SOS_VAL_FREQ_DEFAULT, SOS_VAL_FREQ_RARE, SOS_VAL_FREQ_COMMON, SOS_VAL_FREQ_CONTINUOUS, SOS_VAL_FREQ_IRREGULAR, SOS_VAL_FREQ___MAX, } SOS_val_freq;
typedef enum { SOS_VAL_PATTERN_DEFAULT, SOS_VAL_PATTERN_STATIC, SOS_VAL_PATTERN_RISING, SOS_VAL_PATTERN_PLATEAU, SOS_VAL_PATTERN_OSCILLATING, SOS_VAL_PATTERN_ARC, SOS_VAL_PATTERN___MAX, } SOS_val_pattern;
typedef enum { SOS_VAL_COMPARE_SELF, SOS_VAL_COMPARE_RELATIONS, SOS_VAL_COMPARE___MAX, } SOS_val_compare;
typedef enum { SOS_VAL_CLASS_DATA, SOS_VAL_CLASS_EVENT, SOS_VAL_CLASS___MAX, } SOS_val_class;
typedef enum { SOS_MOOD_DEFAULT, SOS_MOOD_GOOD, SOS_MOOD_BAD, SOS_MOOD_UGLY, SOS_MOOD___MAX, } SOS_mood;
typedef enum { SOS_SCOPE_DEFAULT, SOS_SCOPE_SELF, SOS_SCOPE_NODE, SOS_SCOPE_ENCLAVE, SOS_SCOPE___MAX, } SOS_scope;
typedef enum { SOS_LAYER_DEFAULT, SOS_LAYER_APP, SOS_LAYER_OS, SOS_LAYER_LIB, SOS_LAYER_ENVIRONMENT, SOS_LAYER_SOS_RUNTIME, SOS_LAYER___MAX, } SOS_layer;
typedef enum { SOS_NATURE_DEFAULT, SOS_NATURE_CREATE_INPUT, SOS_NATURE_CREATE_OUTPUT, SOS_NATURE_CREATE_VIZ, SOS_NATURE_EXEC_WORK, SOS_NATURE_BUFFER, SOS_NATURE_SUPPORT_EXEC, SOS_NATURE_SUPPORT_FLOW, SOS_NATURE_CONTROL_FLOW, SOS_NATURE_KMEAN_2D, SOS_NATURE_SOS, SOS_NATURE___MAX, } SOS_nature;
typedef enum { SOS_RETAIN_DEFAULT, SOS_RETAIN_SESSION, SOS_RETAIN_IMMEDIATE, SOS_RETAIN___MAX, } SOS_retain;
typedef enum { SOS_LOCALE_INDEPENDENT, SOS_LOCALE_DAEMON_DBMS, SOS_LOCALE_APPLICATION, SOS_LOCALE___MAX, } SOS_locale;
static const char *SOS_ROLE_string[] = { "SOS_ROLE_UNASSIGNED", "SOS_ROLE_CLIENT", "SOS_ROLE_LISTENER", "SOS_ROLE_AGGREGATOR", "SOS_ROLE_ANALYTICS", "SOS_ROLE_RUNTIME_UTILITY", "SOS_ROLE_OFFLINE_TEST_MODE", "SOS_ROLE___MAX", };
static const char *SOS_TARGET_string[] = { "SOS_TARGET_LOCAL_SYNC", "SOS_TARGET_CLOUD_SYNC", "SOS_TARGET___MAX", };
static const char *SOS_STATUS_string[] = { "SOS_STATUS_INIT", "SOS_STATUS_RUNNING", "SOS_STATUS_HALTING", "SOS_STATUS_SHUTDOWN", "SOS_STATUS___MAX", };
static const char *SOS_MSG_TYPE_string[] = { "SOS_MSG_TYPE_NULLMSG", "SOS_MSG_TYPE_REGISTER", "SOS_MSG_TYPE_GUID_BLOCK", "SOS_MSG_TYPE_ANNOUNCE", "SOS_MSG_TYPE_PUBLISH", "SOS_MSG_TYPE_VAL_SNAPS", "SOS_MSG_TYPE_ECHO", "SOS_MSG_TYPE_SHUTDOWN", "SOS_MSG_TYPE_ACK", "SOS_MSG_TYPE_CHECK_IN", "SOS_MSG_TYPE_FEEDBACK", "SOS_MSG_TYPE_SENSITIVITY", "SOS_MSG_TYPE_TRIGGERPULL", "SOS_MSG_TYPE_PROBE", "SOS_MSG_TYPE_QUERY", "SOS_MSG_TYPE_KMEAN_DATA", "SOS_MSG_TYPE___MAX", };
static const char *SOS_RECEIVES_string[] = { "SOS_RECEIVES_DIRECT_MESSAGES", "SOS_RECEIVES_TIMED_CHECKIN", "SOS_RECEIVES_MANUAL_CHECKIN", "SOS_RECEIVES_NO_FEEDBACK", "SOS_RECEIVES_DAEMON_MODE", "SOS_RECEIVES___MAX", };
static const char *SOS_FEEDBACK_string[] = { "SOS_FEEDBACK_CONTINUE", "SOS_FEEDBACK_CUSTOM", "SOS_FEEDBACK___MAX", };
static const char *SOS_PRI_string[] = { "SOS_PRI_DEFAULT", "SOS_PRI_LOW", "SOS_PRI_IMMEDIATE", "SOS_PRI___MAX", };
static const char *SOS_GEOMETRY_string[] = { "SOS_GEOMETRY_POINT", "SOS_GEOMETRY_HEXAHEDRON", "SOS_GEOMETRY___MAX", };
static const char *SOS_VAL_TYPE_string[] = { "SOS_VAL_TYPE_INT", "SOS_VAL_TYPE_LONG", "SOS_VAL_TYPE_DOUBLE", "SOS_VAL_TYPE_STRING", "SOS_VAL_TYPE_BYTES", "SOS_VAL_TYPE___MAX", };
static const char *SOS_VAL_STATE_string[] = { "SOS_VAL_STATE_CLEAN", "SOS_VAL_STATE_DIRTY", "SOS_VAL_STATE_EMPTY", "SOS_VAL_STATE___MAX", };
static const char *SOS_VAL_SYNC_string[] = { "SOS_VAL_SYNC_DEFAULT", "SOS_VAL_SYNC_RENEW", "SOS_VAL_SYNC_LOCAL", "SOS_VAL_SYNC_CLOUD", "SOS_VAL_SYNC___MAX", };
static const char *SOS_VAL_FREQ_string[] = { "SOS_VAL_FREQ_DEFAULT", "SOS_VAL_FREQ_RARE", "SOS_VAL_FREQ_COMMON", "SOS_VAL_FREQ_CONTINUOUS", "SOS_VAL_FREQ_IRREGULAR", "SOS_VAL_FREQ___MAX", };
static const char *SOS_VAL_SEMANTIC_string[] = { "SOS_VAL_SEMANTIC_DEFAULT", "SOS_VAL_SEMANTIC_TIME_START", "SOS_VAL_SEMANTIC_TIME_STOP", "SOS_VAL_SEMANTIC_TIME_STAMP", "SOS_VAL_SEMANTIC_TIME_SPAN", "SOS_VAL_SEMANTIC_SAMPLE", "SOS_VAL_SEMANTIC_COUNTER", "SOS_VAL_SEMANTIC_LOG", "SOS_VAL_SEMANTIC___MAX", };
static const char *SOS_VAL_PATTERN_string[] = { "SOS_VAL_PATTERN_DEFAULT", "SOS_VAL_PATTERN_STATIC", "SOS_VAL_PATTERN_RISING", "SOS_VAL_PATTERN_PLATEAU", "SOS_VAL_PATTERN_OSCILLATING", "SOS_VAL_PATTERN_ARC", "SOS_VAL_PATTERN___MAX", };
static const char *SOS_VAL_COMPARE_string[] = { "SOS_VAL_COMPARE_SELF", "SOS_VAL_COMPARE_RELATIONS", "SOS_VAL_COMPARE___MAX", };
static const char *SOS_VAL_CLASS_string[] = { "SOS_VAL_CLASS_DATA", "SOS_VAL_CLASS_EVENT", "SOS_VAL_CLASS___MAX", };
static const char *SOS_MOOD_string[] = { "SOS_MOOD_DEFAULT", "SOS_MOOD_GOOD", "SOS_MOOD_BAD", "SOS_MOOD_UGLY", "SOS_MOOD___MAX", };
static const char *SOS_SCOPE_string[] = { "SOS_SCOPE_DEFAULT", "SOS_SCOPE_SELF", "SOS_SCOPE_NODE", "SOS_SCOPE_ENCLAVE", "SOS_SCOPE___MAX", };
static const char *SOS_LAYER_string[] = { "SOS_LAYER_DEFAULT", "SOS_LAYER_APP", "SOS_LAYER_OS", "SOS_LAYER_LIB", "SOS_LAYER_ENVIRONMENT", "SOS_LAYER_SOS_RUNTIME", "SOS_LAYER___MAX", };
static const char *SOS_NATURE_string[] = { "SOS_NATURE_DEFAULT", "SOS_NATURE_CREATE_INPUT", "SOS_NATURE_CREATE_OUTPUT", "SOS_NATURE_CREATE_VIZ", "SOS_NATURE_EXEC_WORK", "SOS_NATURE_BUFFER", "SOS_NATURE_SUPPORT_EXEC", "SOS_NATURE_SUPPORT_FLOW", "SOS_NATURE_CONTROL_FLOW", "SOS_NATURE_KMEAN_2D", "SOS_NATURE_SOS", "SOS_NATURE___MAX", };
static const char *SOS_RETAIN_string[] = { "SOS_RETAIN_DEFAULT", "SOS_RETAIN_SESSION", "SOS_RETAIN_IMMEDIATE", "SOS_RETAIN___MAX", };
static const char *SOS_LOCALE_string[] = { "SOS_LOCALE_INDEPENDENT", "SOS_LOCALE_DAEMON_DBMS", "SOS_LOCALE_APPLICATION", "SOS_LOCALE___MAX", };

typedef struct {
    void *sos_context;
    _Bool is_locking;
    pthread_mutex_t *lock;
    unsigned char *data;
    int len;
    int max;
} SOS_buffer;

typedef uint64_t SOS_guid;
typedef union {
    int i_val;
    long l_val;
    double d_val;
    char *c_val;
    void *bytes;
} SOS_val;
typedef struct {
    double x;
    double y;
    double z;
} SOS_position;
typedef struct {
    SOS_position p[8];
} SOS_volume_hexahedron;
typedef struct {
    double pack;
    double send;
    double recv;
} SOS_time;
typedef struct {
    SOS_val_freq freq;
    SOS_val_semantic semantic;
    SOS_val_class classifier;
    SOS_val_pattern pattern;
    SOS_val_compare compare;
    SOS_mood mood;
} SOS_val_meta;
typedef struct {
    int elem;
    SOS_guid guid;
    SOS_val val;
    int val_len;
    SOS_val_type type;
    SOS_time time;
    long frame;
    SOS_val_semantic semantic;
    SOS_mood mood;
} SOS_val_snap;
typedef struct {
    SOS_guid guid;
    int val_len;
    SOS_val val;
    SOS_val_type type;
    SOS_val_meta meta;
    SOS_val_state state;
    SOS_val_sync sync;
    SOS_time time;
    char name[256];
} SOS_data;
typedef struct {
    int channel;
    SOS_nature nature;
    SOS_layer layer;
    SOS_pri pri_hint;
    SOS_scope scope_hint;
    SOS_retain retain_hint;
} SOS_pub_meta;
typedef struct {
    void *sos_context;
    pthread_mutex_t *lock;
    int sync_pending;
    SOS_guid guid;
    char guid_str[256];
    int process_id;
    int thread_id;
    int comm_rank;
    SOS_pub_meta meta;
    int announced;
    long frame;
    int elem_max;
    int elem_count;
    int pragma_len;
    unsigned char pragma_msg[256];
    char node_id[256];
    char prog_name[256];
    char prog_ver[256];
    char title[256];
    SOS_data **data;
    qhashtbl_t *name_table;
    SOS_pipe *snap_queue;
} SOS_pub;
typedef struct {
    SOS_guid guid;
    SOS_guid client_guid;
    char handle[256];
    void *target;
    SOS_feedback target_type;
    int daemon_trigger_count;
    int client_receipt_count;
} SOS_sensitivity;
typedef struct {
    SOS_guid guid;
    SOS_guid source_guid;
    char handle[256];
    SOS_feedback feedback;
    void *data;
    int data_len;
    int apply_count;
} SOS_action;
typedef struct {
    char *sql;
    int sql_len;
    SOS_action action;
} SOS_trigger;
typedef struct {
    char *server_host;
    char *server_port;
    struct addrinfo *server_addr;
    struct addrinfo *result_list;
    struct addrinfo server_hint;
    struct addrinfo *client_addr;
    int timeout;
    int buffer_len;
    pthread_mutex_t *send_lock;
    SOS_buffer *recv_part;
} SOS_socket_out;
typedef struct {
    int server_socket_fd;
    int client_socket_fd;
    int port_number;
    char *server_port;
    int listen_backlog;
    int client_len;
    struct addrinfo server_hint;
    struct addrinfo *server_addr;
    char *client_host;
    char *client_port;
    struct addrinfo *result;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
} SOS_socket_in;
typedef struct {
    int msg_size;
    SOS_msg_type msg_type;
    SOS_guid msg_from;
    SOS_guid pub_guid;
} SOS_msg_header;
typedef struct {
    int argc;
    char **argv;
    char *node_id;
    int comm_rank;
    int comm_size;
    int comm_support;
    int process_id;
    int thread_id;
    SOS_layer layer;
    SOS_locale locale;
    SOS_receives receives;
    int receives_port;
    _Bool offline_test_mode;
    _Bool runtime_utility;
} SOS_config;
typedef struct {
    void *sos_context;
    SOS_guid next;
    SOS_guid last;
    pthread_mutex_t *lock;
} SOS_uid;
typedef struct {
    SOS_uid *local_serial;
    SOS_uid *my_guid_pool;
} SOS_unique_set;
typedef struct {
    _Bool feedback_active;
    pthread_t *feedback;
    pthread_mutex_t *feedback_lock;
    pthread_cond_t *feedback_cond;
    qhashtbl_t *sense_table;
} SOS_task_set;
typedef struct {
    SOS_config config;
    SOS_role role;
    SOS_status status;
    SOS_unique_set uid;
    SOS_task_set task;
    SOS_socket_out net;
    SOS_guid my_guid;
} SOS_runtime;


void SOS_init(int *argc, char ***argv,
            SOS_runtime **runtime, SOS_role role, SOS_receives receives,
            SOS_feedback_handler_f handler);

void SOS_init_with_runtime(int *argc, char ***argv,
            SOS_runtime **extant_sos_runtime, SOS_role role, SOS_receives receives,
            SOS_feedback_handler_f handler);

SOS_pub* SOS_pub_create(SOS_runtime *sos_context, char *pub_title, SOS_nature nature);
int SOS_pack(SOS_pub *pub, const char *name, SOS_val_type pack_type, void *pack_val_var);
int SOS_event(SOS_pub *pub, const char *name, SOS_val_semantic semantic);
void SOS_announce(SOS_pub *pub);
void SOS_publish(SOS_pub *pub);
int SOS_sense_register(SOS_runtime *sos_context, char *handle, void (*your_callback)(void *your_data));
void SOS_sense_activate(SOS_runtime *sos_context, char *handle, SOS_layer layer, void *data, int data_length);
void SOS_finalize(SOS_runtime *sos_context);

int SOS_file_exists(char *path);
SOS_pub* SOS_pub_create_sized(SOS_runtime *sos_context, char *pub_title, SOS_nature nature, int new_size);
int SOS_pub_search(SOS_pub *pub, const char *name);
void SOS_pub_destroy(SOS_pub *pub);
void SOS_announce_to_buffer(SOS_pub *pub, SOS_buffer *buffer);
void SOS_announce_from_buffer(SOS_buffer *buffer, SOS_pub *pub);
void SOS_publish_to_buffer(SOS_pub *pub, SOS_buffer *buffer);
void SOS_publish_from_buffer(SOS_buffer *buffer, SOS_pub *pub, SOS_pipe *optional_snap_queue);
void SOS_uid_init(SOS_runtime *sos_context, SOS_uid **uid, SOS_guid from, SOS_guid to);
SOS_guid SOS_uid_next(SOS_uid *uid);
void SOS_uid_destroy(SOS_uid *uid);
void SOS_val_snap_queue_to_buffer(SOS_pub *pub, SOS_buffer *buffer, _Bool destroy_snaps);
void SOS_val_snap_queue_from_buffer(SOS_buffer *buffer, SOS_pipe *snap_queue, SOS_pub *pub);
void SOS_strip_str(char *str);
char* SOS_uint64_to_str(uint64_t val, char *result, int result_len);
void SOS_send_to_daemon(SOS_buffer *buffer, SOS_buffer *reply);
void SOS_buffer_init(void *sos_context, SOS_buffer **buffer);
void SOS_buffer_init_sized(void *sos_context, SOS_buffer **buffer, int max_size);
void SOS_buffer_init_sized_locking(void *sos_context, SOS_buffer **buffer, int max_size, _Bool locking);
void SOS_buffer_clone(SOS_buffer **dest, SOS_buffer *src);
void SOS_buffer_lock(SOS_buffer *buffer);
void SOS_buffer_unlock(SOS_buffer *buffer);
void SOS_buffer_destroy(SOS_buffer *buffer);
void SOS_buffer_wipe(SOS_buffer *buffer);
void SOS_buffer_grow(SOS_buffer *buffer, size_t grow_amount, char *from_func);
void SOS_buffer_trim(SOS_buffer *buffer, size_t to_new_max);
int SOS_buffer_pack(SOS_buffer *buffer, int *offset, char *format, ...);
int SOS_buffer_unpack(SOS_buffer *buffer, int *offset, char *format, ...);
void SOS_buffer_unpack_safestr(SOS_buffer *buffer, int *offset, char **dest);
uint64_t SOS_buffer_pack754(long double f, unsigned bits, unsigned expbits);
double SOS_buffer_unpack754(uint64_t i, unsigned bits, unsigned expbits);
void SOS_buffer_packi32(unsigned char *buf, int32_t i);
void SOS_buffer_packi64(unsigned char *buf, int64_t i);
int32_t SOS_buffer_unpacki32(unsigned char *buf);
int64_t SOS_buffer_unpacki64(unsigned char *buf);
uint64_t SOS_buffer_unpacku64(unsigned char *buf);

""")



ffibuilder.set_source(
    "_sos",
    """ 
        #include <stdio.h>
        #include <stdlib.h>
        #include <limits.h>
        #include <inttypes.h>
        #include <pthread.h>
        #include "sos.h"
        #include "sos_types.h"
        #include "sos_pipe.h"
        #include "sos_buffer.h"
        #include "sos_qhashtbl.h"
    """,
    sources=[
        "sos.c",
        "sos_buffer.c",
        "sos_pipe.c",
        "sos_qhastbl.c"
    ],
    libraries=[],
    include_dirs=['..'])

#
# ----------
#
if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
