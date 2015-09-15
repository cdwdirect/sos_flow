include_directories("${sos_flow_SOURCE_DIR}/src")
include_directories("${MPI_INCLUDE_PATH}")

link_directories("${sos_flow_SOURCE_DIR}/src")


if(SILENCE_DEBUG)
  add_definitions("-DSOS_DEBUG=0")
endif()

if(DAEMON_LOGGING)
  add_definitions("-DSOS_DAEMON_LOGGING=1")
endif()


#TARGET ---> sos
#            (library)
add_library("sos" SHARED "sos.c")
add_library("sos-static" STATIC "sos.c")
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(TARGET "sos" PROPERTY COMPILE_OPTIONS "-pthread")
  target_compile_options(TARGET "sos" PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
  target_compile_options(TARGET "sos-static" PROPERTY COMPILE_OPTIONS "-pthread")
  target_compile_options(TARGET "sos-static" PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries("sos" "${CMAKE_THREAD_LIBS_INIT}")
  target_link_libraries("sos-static" "${CMAKE_THREAD_LIBS_INIT}")
endif()


#TARGET ---> sosd
#            (daemon)
add_executable("sosd" "sosd.c" "sosd_db_sqlite.c" "qhashtbl.c")
target_link_libraries("sosd" "sos" "sqlite3" "rt")
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(TARGET "sosd" PROPERTY COMPILE_OPTIONS "-pthread")
  target_compile_options(TARGET "sosd" PROPERTY COMPILE_OPTIONS "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries("sosd" "${CMAKE_THREAD_LIBS_INIT}")
endif()


#TARGET ---> sos_cmd
#            (command-line interface)
add_executable("sos_cmd" "sos_cmd.c")
target_link_libraries("sos_cmd" "sos")


#TARGET ---> demo_app
#            (demonstration / example)
add_executable("demo_app" "demo_app.c")
target_link_libraries("demo_app" "sos")


#TARGET ---> mpi_to_sos
#            (synthetic benchmark)
add_executable("mpi_to_sos" "mpi_to_sos.c")
target_link_libraries("mpi_to_sos" "sos" "${MPI_LIBRARIES}")
if(MPI_COMPILE_FLAGS)
  set_target_properties("mpi_to_sos" PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
endif()
if(MPI_LINK_FLAGS)
  set_target_properties("mpi_to_sos" PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
endif()
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(TARGET "mpi_to_sos" PROPERTY COMPILE_OPTIONS "-pthread")
  target_compile_options(TARGET "mpi_to_sos" PROPERTY COMPILE_OPTIONS "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries("mpi_to_sos" "${CMAKE_THREAD_LIBS_INIT}")
endif()
