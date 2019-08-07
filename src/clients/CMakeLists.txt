set(CMAKE_POSITION_INDEPENDENT_CODE True)
set(POSITION_INDEPENDENT_CODE True)

include_directories(${sos_flow_SOURCE_DIR}/src)

# If using SQLite3, add include files - it's required
if(SQLite3_FOUND)
    include_directories(${SQLite3_INCLUDE_DIR})
    link_libraries(${SQLite3_LIBRARY})
endif(SQLite3_FOUND)

# If using MPI, add the include files
if(MPI_FOUND)
    include_directories(${MPI_INCLUDE_PATH})
    link_libraries(${MPI_C_LIBRARIES})
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${MPI_C_COMPILE_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${MPI_C_LINK_FLAGS}")
endif(MPI_FOUND)

if(EVPath_FOUND)
    include_directories(${EVPath_INCLUDE_DIR})
    link_libraries(${EVPath_LIBRARIES})
endif(EVPath_FOUND)

if(ADIOS_FOUND)
    include_directories(${ADIOS_INCLUDE_DIR})
    link_libraries(${ADIOS_LIBRARIES})
endif(ADIOS_FOUND)

if(Munge_FOUND)
    include_directories(${Munge_INCLUDE_DIR})
    link_libraries(${Munge_LIBRARIES})
endif(Munge_FOUND)

if(DEFINED PYTHON_INCLUDE_DIR)
    include_directories("${PYTHON_INCLUDE_DIR}")
    link_directories("${PYTHON_INCLUDE_DIR}/../../lib")
endif()

if(SOS_SILENCE_DEBUG)
  add_definitions("-DSOS_DEBUG=0")
endif()

#
#   NOTE: This CMakeLists.txt is special, it automatically
#         compiles all <filename>.c files in this directory
#         according to the standard SOSA rules.
#
#         If you want to do your own thing, comment out this
#         section and check out the templates below.
#

if(ADIOS_FOUND)
    if(MPI_FOUND)
        add_executable("sosa_extract_to_adios" "extract_to_adios.cpp")
        target_link_libraries("sosa_extract_to_adios" "sos" stdc++ ${MPI_CXX_LIBRARIES})
        set_target_properties("sosa_extract_to_adios" PROPERTIES COMPILE_FLAGS "${MPI_CXX_COMPILE_FLAGS}")
        set_target_properties("sosa_extract_to_adios" PROPERTIES LINK_FLAGS "${MPI_CXX_LINK_FLAGS}")

        add_executable("sosa_extract_from_cache_to_adios" "extract_from_cache_to_adios.cpp")
        target_link_libraries("sosa_extract_from_cache_to_adios" "sos" stdc++ ${MPI_CXX_LIBRARIES})
        set_target_properties("sosa_extract_from_cache_to_adios" PROPERTIES COMPILE_FLAGS "${MPI_CXX_COMPILE_FLAGS}")
        set_target_properties("sosa_extract_from_cache_to_adios" PROPERTIES LINK_FLAGS "${MPI_CXX_LINK_FLAGS}")
    endif(MPI_FOUND)
endif(ADIOS_FOUND)

# target_link_libraries(sosa_py_runner ${PYTHON_LIBRARIES})
# target_include_directories(sosa_py_runner PUBLIC ${PYTHON_INCLUDE_DIRS})
# target_compile_options(sosa_py_runner PUBLIC ${PYTHON_CFLAGS_OTHER})



