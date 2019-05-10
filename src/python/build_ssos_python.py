#!/usr/bin/env python
# file "build_ssos_python.py"

import os
import sys
from cffi import FFI
ffibuilder = FFI()


#
# ----------
#
if __name__ == "__main__":
    sos_include_dir=""
    sos_lib_dir=""
    if len(sys.argv) > 1:
        sos_include_dir = sys.argv[1]
        sos_lib_dir = sys.argv[2]
    else:
        sos_build_dir = str(os.environ.get("SOS_BUILD_DIR"))
        sos_include_dir = str(sos_build_dir) + "/include"
        sos_lib_dir = str(sos_build_dir) + "/lib"

    ffibuilder.set_source(
    "ssos_python", """

    #include "ssos.h"
    #include "sosa.h"

    """,
    sources=[
       "../ssos.c"
    ],
    libraries=["sos"],
    library_dirs=[sos_lib_dir],
    include_dirs=[sos_include_dir])
    #extra_compile_args=["-Wno-unused-variable"])
    #extra_compile_args=["-Wno-unused-variable", "-DUSE_MUNGE=1"])

    ffibuilder.cdef("""

    typedef struct {
        void        *sos_context;
        char        *query_sql;
        uint64_t     query_guid;
        double       exec_duration;
        uint32_t     topology;
        uint64_t     group_guid;
        uint32_t     group_size;
        uint32_t     group_rank;
        uint32_t     col_max;
        uint32_t     col_count;
        char       **col_names;
        uint32_t     row_max;
        uint32_t     row_count;
        char      ***data;
    } SSOS_query_results;


    // --------------------

    // These are the types supported by SSOS for use as the
    // second parameter of SSOS_pack(name, type, value):

    #define SSOS_TYPE_INT     1
    #define SSOS_TYPE_LONG    2
    #define SSOS_TYPE_DOUBLE  3
    #define SSOS_TYPE_STRING  4


    // These option keys can be used to set values inside of
    // the various objects SOS uses to track application and
    // publication metadata.  They are used as the first parameter
    // of SSOS_set_option(key, value):

    #define SSOS_OPT_PROG_VERSION   1
    #define SSOS_OPT_COMM_RANK      2


    // The following SSOS API functions will be available for
    // use within Python scripts. They are neatly wrapped up for
    // ease of use by the ssos.py script, but can be called
    // directly if desired:

    void SSOS_init(const char *prog_name);
    void SSOS_is_online(int *addr_of_YN_int_flag);
    void SSOS_set_option(int option_key, const char *option_value);
    void SSOS_get_guid(void *addr_of_uint64);

    void SSOS_pack(const char *name, int pack_type, void *addr_of_value);
    void SSOS_announce(void);
    void SSOS_publish(void);
    void SSOS_finalize(void);

    void SSOS_query_exec(char *sql, char *target_host, int target_port);
    //
    void SSOS_request_pub_manifest(
        SSOS_query_results **manifest_var,
        int  *max_frame_overall_var,
        const char *pub_title_filter,
        const char *target_host,
        int   target_port);
    void SSOS_refresh_pub_manifest(
        SSOS_query_results *manifest_var,
        int  *max_frame_overall_var,
        const char *pub_title_filter,
        const char *target_host,
        int   target_port);
    //
    void SSOS_cache_grab(
        const char *pub_filter,
        const char *val_filter,
        int         frame_head,
        int         frame_depth_limit,
        const char *target_host,
        int         target_port);
    //
    void SSOS_result_pool_size(int *addr_of_counter_int);
    void SSOS_result_claim(SSOS_query_results *results);
    void SSOS_result_claim_to_ptraddr(SSOS_query_results **results_ptraddr);
    void SSOS_result_claim_initialized(SSOS_query_results *results,
            int YN_initialize_result_object);
    void SSOS_result_destroy(SSOS_query_results *results);

    void SSOS_sense_trigger(const char *sense_handle,
            int payload_size, void *payload_data);

    void SSOS_get_runtime(void *addr_of_runtime_ptr_var);

    // --------------------
""")

    ffibuilder.compile(verbose=True)
    #ffibuilder.compile(verbose=False)


