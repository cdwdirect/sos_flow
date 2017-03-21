set(CMAKE_POSITION_INDEPENDENT_CODE True)
set(POSITION_INDEPENDENT_CODE True)

if(SOSD_CLOUD_SYNC_WITH_MPI)
    include_directories("${MPI_INCLUDE_PATH}")
endif()
include_directories("${sos_flow_SOURCE_DIR}/src")
#   link_directories("${sos_flow_SOURCE_DIR}/lib")

if(DEFINED SQLITE3_INCLUDE_DIR)
    include_directories("${SQLITE3_INCLUDE_DIR}")
else()
    if(EXISTS "$ENV{SQLITE}")
        include_directories("$ENV{SQLITE}/include")
        link_directories("$ENV{SQLITE}/lib")
    endif()
endif()

if(DEFINED EVPATH_FOUND)
    include_directories(${EVPATH_INCLUDE_DIRS})
    link_directories(${EVPATH_LIBRARY_DIRS})
    link_libraries(${EVPATH_LIBRARIES})
endif()
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

file(GLOB sosa_mod_src_file_list
     RELATIVE "${CMAKE_SOURCE_DIR}/src/sosa_mods"
     "*.c" )
foreach(src_file_name ${sosa_mod_src_file_list})
  string(REGEX REPLACE "\\.[^.]*$" "" bin_file_name ${src_file_name})
  add_executable("sosa_${bin_file_name}" "../sosa.c" "${src_file_name}")
  target_link_libraries("sosa_${bin_file_name}" "sos" "m" "sosa" "${MPI_LIBRARIES}")
  if(MPI_COMPILE_FLAGS)
    set_target_properties("sosa_${bin_file_name}" PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
  endif()
  if(MPI_LINK_FLAGS)
    set_target_properties("sosa_${bin_file_name}" PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
  endif()
endforeach()

# target_link_libraries(sosa_py_runner ${PYTHON_LIBRARIES})
# target_include_directories(sosa_py_runner PUBLIC ${PYTHON_INCLUDE_DIRS})
# target_compile_options(sosa_py_runner PUBLIC ${PYTHON_CFLAGS_OTHER})



