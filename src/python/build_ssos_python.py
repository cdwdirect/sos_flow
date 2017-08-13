#!/usr/bin/env python
# file "build_ssos_python.py"

import os
from cffi import FFI
ffibuilder = FFI()

ffibuilder.set_source(
    "ssos_python", """ 

    #include "ssos.h"
    #include "sosa.h"

    """,
    sources=[
       "../ssos.c"
    ],
    libraries=["ssos", "sos", "sosa", "munge"],
    library_dirs=[os.environ.get("SOS_BUILD_DIR") + "/lib"],
    include_dirs=[os.environ.get("SOS_BUILD_DIR") + "/include", ".."],
    extra_compile_args=["-Wno-unused-variable", "-DUSE_MUNGE=1"])

ffibuilder.cdef("""    

    typedef struct {
        void        *sos_context;
        char        *query_sql;
        uint64_t     query_guid;
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
    
    void SSOS_init(char *prog_name);
    void SSOS_is_online(int *addr_of_YN_int_flag);
    void SSOS_set_option(int option_key, char *option_value);

    void SSOS_pack(char *name, int pack_type, void *addr_of_value);
    void SSOS_announce(void);
    void SSOS_publish(void);
    void SSOS_finalize(void);

    void SSOS_query_exec_blocking(char *sql, SSOS_query_results *results,
            char *target_host, int target_port);
    void SSOS_query_exec(char *sql, SSOS_query_results *results,
            char *target_host, int target_port);
    void SSOS_is_query_done(int *addr_of_YN_int_flag);
    void SSOS_results_destroy(SSOS_query_results *results);

    void SSOS_sense_trigger(char *sense_handle,
            int payload_size, void *payload_data); 

    
    // --------------------
""")

#
# ----------
#
if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
