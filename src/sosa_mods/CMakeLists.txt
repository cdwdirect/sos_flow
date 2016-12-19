set(CMAKE_POSITION_INDEPENDENT_CODE True)
set(POSITION_INDEPENDENT_CODE True)

include_directories("${MPI_INCLUDE_PATH}")
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



#TARGET ---> template
#            (sosa analytics shell, to be cloned for new analytics modules)
#add_executable("template" "../sosa.c" "template.c")
#target_link_libraries("template" "sos" "sosa" "${MPI_LIBRARIES}")
#if(MPI_COMPILE_FLAGS)
#  set_target_properties("template" PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
#endif()
#if(MPI_LINK_FLAGS)
#  set_target_properties("template" PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
#endif()



#TARGET ---> latency
#            (data migration latency statistics)
#add_executable("latency" "../sosa.c" "latency.c")
#target_link_libraries("latency" "sos" "sosa" "${MPI_LIBRARIES}")
#if(MPI_COMPILE_FLAGS)
#  set_target_properties("latency" PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
#endif()
#if(MPI_LINK_FLAGS)
#  set_target_properties("latency" PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
#endif()



#TARGET ---> extract_tau
#            (pull out tau data and move it to independent analyzer)
#add_executable("extract_tau" "../sosa.c" "extract_tau.c")
#target_link_libraries("extract_tau" "sos" "sosa" "${MPI_LIBRARIES}")
#if(MPI_COMPILE_FLAGS)
#  set_target_properties("extract_tau" PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
#endif()
#if(MPI_LINK_FLAGS)
#  set_target_properties("extract_tau" PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
#endif()


