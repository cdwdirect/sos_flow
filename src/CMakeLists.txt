set(CMAKE_POSITION_INDEPENDENT_CODE True)
set(POSITION_INDEPENDENT_CODE True)

include_directories("${MPI_INCLUDE_PATH}")
include_directories("${sos_flow_SOURCE_DIR}/src")
   link_directories("${sos_flow_SOURCE_DIR}/lib")

if(EXISTS "$ENV{SQLITE}")
include_directories("$ENV{SQLITE}/include")
   link_directories("$ENV{SQLITE}/lib")
endif()

if(SOS_SILENCE_DEBUG)
  add_definitions("-DSOS_DEBUG=0")
endif()

if(SOSD_DAEMON_LOGGING)
  add_definitions("-DSOSD_DAEMON_LOGGING=1")
endif()

if(SOSD_DAEMON_DETACH)
  message(STATUS ">>> DAEMON_DETACH:")
  message(STATUS ">>> DAEMON_DETACH:  Daemon is set to DETACH and run in the background...")
  message(STATUS ">>> DAEMON_DETACH:")
  add_definitions("-DSOSD_DAEMON_MODE=1")
else()
  message(STATUS ">>> DAEMON_DETACH:")
  message(STATUS ">>> DAEMON_DETACH:  Daemon is set to run INTERACTIVELY...")
  message(STATUS ">>> DAEMON_DETACH:")
  add_definitions("-DSOSD_DAEMON_MODE=0")
endif()


#TARGET ---> sos
#            (library)
add_library("sos" SHARED "sos.c" "pack_buffer.c" "qhashtbl.c")
add_library("sos_static" STATIC "sos.c" "pack_buffer.c" "qhashtbl.c")
if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(TARGET "sos" PROPERTY COMPILE_OPTIONS "-pthread")
  target_compile_options(TARGET "sos" PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread") 
  target_compile_options(TARGET "sos_static" PROPERTY COMPILE_OPTIONS "-pthread")
  target_compile_options(TARGET "sos_static" PROPERTY INTERFACE_COMPILE_OPTIONS "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries("sos" "${CMAKE_THREAD_LIBS_INIT}")
  target_link_libraries("sos_static" "${CMAKE_THREAD_LIBS_INIT}")
endif()


#TARGET ---> sosd
#            (daemon)
if(SOSD_CLOUD_SYNC)
  add_definitions("-DSOSD_CLOUD_SYNC")
  if(SOSD_CLOUD_SYNC_WITH_MPI)
    message(STATUS ">>> CLOUD_SYNC:")
    message(STATUS ">>> CLOUD_SYNC:  Building the daemon using SOSD_CLOUD_SYNC_WITH_MPI enabled...")
    message(STATUS ">>> CLOUD_SYNC:")
    add_executable("sosd"
      "sosd.c"
      "sosd_cloud_mpi.c"
      "sosd_db_sqlite.c"
      "sos_error.c"
      "qhashtbl.c"
      "pack_buffer.c"  )
    set_target_properties("sosd" PROPERTIES COMPILE_FLAGS "-DSOSD_DAEMON_SRC")
    set_target_properties("sosd" PROPERTIES COMPILE_FLAGS "-DSOSD_CLOUD_SYNC")
    set_target_properties("sosd" PROPERTIES COMPILE_FLAGS "-DSOSD_CLOUD_SYNC_WITH_MPI")
    target_link_libraries("sosd" "sos" "${MPI_LIBRARIES}" "${SQLITE3_LIBRARY}")
    if(APPLE)
        # Apple does no support the '-lrt' compiler option.
    else()
        target_link_libraries("sosd" "rt")
    endif()

if(MPI_COMPILE_FLAGS)
      set_target_properties("sosd" PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
    endif()
    if(MPI_LINK_FLAGS)
      set_target_properties("sosd" PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
    endif()
  elseif(SOSD_CLOUD_SYNC_WITH_EVPATH)
    message(AUTHOR_WARNING ">>> CLOUD_SYNC:")
    message(AUTHOR_WARNING ">>> CLOUD_SYNC:  SOSD_CLOUD_SYNC_WITH_EVPATH was requested... this is NOT SUPPORTED at present!")
    message(AUTHOR_WARNING ">>> CLOUD_SYNC:     ... data will not migrate off-node.")  
    message(AUTHOR_WARNING ">>> CLOUD_SYNC:")
    add_executable("sosd"
      "sosd.c"
      "sosd_cloud_evpath.c"
      "sosd_db_sqlite.c"
      "sos_error.c"
      "qhashtbl.c"
      "pack_buffer.c"  )
    set_target_properties("sosd" PROPERTIES COMPILE_FLAGS "-DSOSD_DAEMON_SRC")
    set_target_properties("sosd" PROPERTIES COMPILE_FLAGS "-DSOSD_CLOUD_SYNC")
    set_target_properties("sosd" PROPERTIES COMPILE_FLAGS "-DSOSD_CLOUD_SYNC_WITH_EVPATH")
    target_link_libraries("sosd" "sos" "rt" "${SQLITE3_LIBRARY}")
  else()
    message(FATAL_ERROR "You have specified that cloud_sync is activated, but not specified a supported method.")  
  endif()
else()
  message(STATUS ">>> CLOUD_SYNC:")
  message(STATUS ">>> CLOUD_SYNC:  Building the daemon with NO SUPPORT for cloud_sync operations.")
  message(STATUS ">>> CLOUD_SYNC:  Nothing will be sent off-node.  Functions are stubbed out, but not called.")
  message(STATUS ">>> CLOUD_SYNC:")
  add_executable("sosd"
    "sosd.c"
    "sosd_cloud_stubs.c"
    "sosd_db_sqlite.c"
    "sos_error.c"
    "qhashtbl.c"
    "pack_buffer.c")
  set_target_properties("sosd" PROPERTIES COMPILE_FLAGS "-DSOSD_DAEMON_SRC")
  set_target_properties("sosd" PROPERTIES COMPILE_FLAGS "-DSOSD_CLOUD_SYNC_WITH_STUBS")
  target_link_libraries("sosd" "sos" "rt" "${SQLITE3_LIBRARY}")
endif()

if(THREADS_HAVE_PTHREAD_ARG)
  target_compile_options(TARGET "sosd" PROPERTY COMPILE_OPTIONS "-pthread")
  target_compile_options(TARGET "sosd" PROPERTY COMPILE_OPTIONS "-pthread")
endif()
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries("sosd" "${CMAKE_THREAD_LIBS_INIT}")
endif()

#TARGET ---> sosd_stop
#            (command-line interface)
add_executable("sosd_stop" "sosd_stop.c")
target_link_libraries("sosd_stop" "sos")


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

INSTALL(FILES showdb DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  PERMISSIONS OWNER_EXECUTE OWNER_WRITE 
  OWNER_READ GROUP_EXECUTE GROUP_READ
  WORLD_EXECUTE WORLD_READ)
