include_directories("${sos_flow_SOURCE_DIR}/src")
include_directories("${MPI_INCLUDE_PATH}")

link_directories("${sos_flow_SOURCE_DIR}/src")


if(SILENCE_DEBUG)
  add_definitions("-DSOS_DEBUG=0")
endif()

if(DAEMON_LOGGING)
  add_definitions("-DSOS_DAEMON_LOGGING=1")
endif()

add_library("sos" SHARED "sos.c")
add_library("sos-static" STATIC "sos.c")


add_executable("sosd" "sosd.c" "sosd_db_sqlite.c" "qhashtbl.c")
target_link_libraries("sosd" "sos" "sqlite3")


add_executable("sos_cmd" "sos_cmd.c")
target_link_libraries("sos_cmd" "sos")


add_executable("demo_app" "demo_app.c")
target_link_libraries("demo_app" "sos")


add_executable("mpi_to_sos" "mpi_to_sos.c")
target_link_libraries("mpi_to_sos" "sos" "${MPI_LIBRARIES}")
if(MPI_COMPILE_FLAGS)
  set_target_properties("mpi_to_sos" PROPERTIES COMPILE_FLAGS "${MPI_COMPILE_FLAGS}")
endif()
if(MPI_LINK_FLAGS)
  set_target_properties("mpi_to_sos" PROPERTIES LINK_FLAGS "${MPI_LINK_FLAGS}")
endif()