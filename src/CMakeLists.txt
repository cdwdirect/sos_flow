set(CMAKE_POSITION_INDEPENDENT_CODE True)
set(POSITION_INDEPENDENT_CODE True)

include_directories("${MPI_INCLUDE_PATH}")
include_directories("${sos_flow_SOURCE_DIR}/src")
#   link_directories("${sos_flow_SOURCE_DIR}/lib")


subdirs (sosa_mods)


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


#TARGET ---> sos_pipe
#            (library)
add_library("sos_pipe" "sos_pipe.c")
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries("sos_pipe" "${CMAKE_THREAD_LIBS_INIT}")
endif()

#TARGET ---> sos_buffer
#            (library)
add_library("sos_buffer" "sos_buffer.c")
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries("sos_buffer" "${CMAKE_THREAD_LIBS_INIT}")
endif()

#TARGET ---> sos_qhashtbl
#            (library)
add_library("sos_qhashtbl" "sos_qhashtbl.c")




#TARGET ---> sos
#            (library)
#add_library("sos" SHARED "sos.c" "sos_buffer.c" "sos_qhashtbl.c" "sos_pipe.c")
add_library("sos" "sos.c" "sos_buffer.c" "sos_qhashtbl.c" "sos_pipe.c")
# add_library("sos_static" STATIC "sos.c" "sos_pack_buffer.c" "sos_qhashtbl.c" "sos_pipe.c")
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries("sos"
    "sos_buffer"
    "sos_qhashtbl"
    "sos_pipe"
    "${CMAKE_THREAD_LIBS_INIT}")
  # target_link_libraries("sos_static" "${CMAKE_THREAD_LIBS_INIT}")
endif()


#TARGET ---> sosa
#            (library)
add_library("sosa" "sosa.c" "sos_buffer.c" "sos_qhashtbl.c" "sos_pipe.c" )
if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries("sosa"
    "sos_buffer"
    "sos_qhashtbl"
    "sos_pipe"
    "${CMAKE_THREAD_LIBS_INIT}"
    "${MPI_LIBRARIES}")
endif()
if(MPI_COMPILE_FLAGS)
  set_target_properties("sosa" PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
endif()
if(MPI_LINK_FLAGS)
  set_target_properties("sosa" PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
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
      "sos_qhashtbl.c"
      "sos_buffer.c"
      "sos_pipe.c"
      "sosa.c")
    set_target_properties("sosd" PROPERTIES COMPILE_FLAGS "-DSOSD_DAEMON_SRC")
    set_target_properties("sosd" PROPERTIES COMPILE_FLAGS "-DSOSD_CLOUD_SYNC")
    set_target_properties("sosd" PROPERTIES COMPILE_FLAGS "-DSOSD_CLOUD_SYNC_WITH_MPI")
    target_link_libraries("sosd"
      "sos"
      "sos_qhashtbl"
      "sos_buffer"
      "sos_pipe"
      "sosa"
      "${MPI_LIBRARIES}"
      "${SQLITE3_LIBRARY}")
    if(APPLE)
        # Apple does not support the '-lrt' compiler option.
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
    message(STATUS ">>> CLOUD_SYNC:")
    message(STATUS ">>> CLOUD_SYNC:  SOSD_CLOUD_SYNC_WITH_EVPATH was requested...")
    message(STATUS ">>> CLOUD_SYNC:")
    add_executable("sosd"
      "sosd.c"
      "sosd_cloud_evpath.c"
      "sosd_db_sqlite.c"
      "sos_error.c"
      "sos_qhashtbl.c"
      "sos_buffer.c"
      "sos_pipe.c"
      "sosa.c")
    set_target_properties("sosd" PROPERTIES COMPILE_FLAGS "-DSOSD_DAEMON_SRC")
    set_target_properties("sosd" PROPERTIES COMPILE_FLAGS "-DSOSD_CLOUD_SYNC")
    set_target_properties("sosd" PROPERTIES COMPILE_FLAGS "-DSOSD_CLOUD_SYNC_WITH_EVPATH")
    target_link_libraries("sosd"
      "sos"
      "sos_qhashtbl"
      "sos_buffer"
      "sos_pipe"
      "sosa"
      "${SQLITE3_LIBRARY}")
    if(APPLE)
        # Apple does no support the '-lrt' compiler option.
    else()
        target_link_libraries("sosd" "rt")
    endif()

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
    "sos_qhashtbl.c"
    "sos_buffer.c"
    "sos_pipe.c"
    "sosa.c")
  set_target_properties("sosd" PROPERTIES COMPILE_FLAGS "-DSOSD_DAEMON_SRC")
  set_target_properties("sosd" PROPERTIES COMPILE_FLAGS "-DSOSD_CLOUD_SYNC_WITH_STUBS")
  target_link_libraries("sosd"
    "sos"
    "sos_qhashtbl"
    "sos_buffer"
    "sos_pipe"
    "sosa"
    "${SQLITE3_LIBRARY}")
  if(APPLE)
    # Apple does no support the '-lrt' compiler option.
  else()
    target_link_libraries("sosd" "rt")
  endif()

endif()

if(CMAKE_THREAD_LIBS_INIT)
  target_link_libraries("sosd" "${CMAKE_THREAD_LIBS_INIT}")
endif()




#TARGET ---> sosd_stop
#            (shut down the daemon)
add_executable("sosd_stop" "sosd_stop.c")
target_link_libraries("sosd_stop" "sos" "${MPI_LIBRARIES}")
if(MPI_COMPILE_FLAGS)
  set_target_properties("sosd_stop" PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
endif()
if(MPI_LINK_FLAGS)
  set_target_properties("sosd_stop" PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
endif()


#TARGET ---> sosd_probe
#            (get daemon satus and display it)
add_executable("sosd_probe" "sosd_probe.c")
target_link_libraries("sosd_probe" "sos" "${MPI_LIBRARIES}")
if(MPI_COMPILE_FLAGS)
  set_target_properties("sosd_probe" PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
endif()
if(MPI_LINK_FLAGS)
  set_target_properties("sosd_probe" PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
endif()




#TARGET ---> demo_app
#            (demonstration / example)
add_executable("demo_app" "demo_app.c")
target_link_libraries("demo_app" "sos" "${MPI_LIBRARIES}")
if(MPI_COMPILE_FLAGS)
  set_target_properties("demo_app" PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
endif()
if(MPI_LINK_FLAGS)
  set_target_properties("demo_app" PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
endif()




#TARGET ---> demo_sweep
#            (demonstration / example)
add_executable("demo_sweep" "demo_sweep.c")
target_link_libraries("demo_sweep" "sos" "${MPI_LIBRARIES}")
if(MPI_COMPILE_FLAGS)
  set_target_properties("demo_sweep" PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
endif()
if(MPI_LINK_FLAGS)
  set_target_properties("demo_sweep" PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
endif()





#TARGET ---> proc_app
#            (demonstration / example)
add_executable("proc_app" "proc_app.c")
target_link_libraries("proc_app" "sos" "${MPI_LIBRARIES}")
if(MPI_COMPILE_FLAGS)
  set_target_properties("proc_app" PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
endif()
if(MPI_LINK_FLAGS)
  set_target_properties("proc_app" PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
endif()



INSTALL(FILES ../scripts/showdb DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
  PERMISSIONS OWNER_EXECUTE OWNER_WRITE 
  OWNER_READ GROUP_EXECUTE GROUP_READ
  WORLD_EXECUTE WORLD_READ)

INSTALL(FILES sos.h sos_types.h sos_qhashtbl.h sos_pipe.h sos_buffer.h DESTINATION include)
#INSTALL(TARGETS sos_pipe sos_buffer sos sosa sosd_stop sosd_probe demo_app demo_sweep proc_app RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE DESTINATION lib) 
