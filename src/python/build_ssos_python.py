# file "ssos_build.py"

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
    libraries=["ssos", "sos", "sosa"],
    library_dirs=["../../build-linux/lib"],
    include_dirs=[".."],
    extra_compile_args=["-Wno-unused-variable"])

ffibuilder.cdef("""    

    typedef struct {
        void        *sos_context;
        uint32_t     col_max;
        uint32_t     col_count;
        char       **col_names;
        uint32_t     row_max;
        uint32_t     row_count;
        char      ***data;
    } SSOS_query_results;
    
    void SSOS_init(void);
    void SSOS_is_online(int *addr_of_int32_flag);
    void SSOS_pack(char *name, int pack_type, void *addr_of_value);
    void SSOS_announce(void);
    void SSOS_publish(void);
    void SSOS_finalize(void);

    void SSOS_exec_query(char *sql, SSOS_query_results *results);

    #define SSOS_TYPE_INT     1
    #define SSOS_TYPE_LONG    2
    #define SSOS_TYPE_DOUBLE  3
    #define SSOS_TYPE_STRING  4
    
""")

#
# ----------
#
if __name__ == "__main__":
    ffibuilder.compile(verbose=True)
