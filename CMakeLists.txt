cmake_minimum_required (VERSION 2.8)


option(ENABLE_ADIOS_EXAMPLES        "Build the examples that require the presence of ADIOS?" ON)
option(ENABLE_TAU_WRAPPER           "Compile the codes with tau_cc.sh?" OFF)
option(SOS_SILENCE_DEBUG            "Disable DEBUG messages?" OFF)
option(SOSD_DAEMON_LOGGING          "Should the sosd daemon produce a log file?" ON)
option(SOSD_DAEMON_DETACH           "Have the daemon fork, detach, and get its own session?  (Consider disabling for MPI)" OFF)

option(SOSD_CLOUD_SYNC              "Activate cloud_sync operations for the daemon, if possible.  (Pick one and only one method.)" ON)
option(SOSD_CLOUD_SYNC_WITH_MPI     "Use MPI discover and synchronize data between daemons?" ON)
option(SOSD_CLOUD_SYNC_WITH_EVPATH  "Use EVPATH synchronize data between daemons?" OFF)

if(SOSD_CLOUD_SYNC_WITH_MPI AND SOSD_CLOUD_SYNC_WITH_EVPATH)
  message(FATAL_ERROR "You cannot use both MPI and EVPATH for daemon cloud_sync operations!  Please select only one.")
endif()

if((SOSD_CLOUD_SYNC_WITH_MPI OR SOSD_CLOUD_SYNC_WITH_EVPATH) AND NOT SOSD_CLOUD_SYNC)
  message(FATAL_ERROR "You have specified a method for cloud_sync, but disabled cloud_sync.  Verify your build options.")
endif()

set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set(PROJECT_NAME sos_flow)
set(PEDANTIC TRUE CACHE BOOL "Treat all compiler warnings as errors")

if(APPLE)
    #set(CMAKE_C_COMPILER "/usr/bin/c99")
else()
endif()

project(${PROJECT_NAME} C)

if(ENABLE_TAU_WRAPPER)
  message(STATUS ">>> TAU:")
  message(STATUS ">>> TAU:  Building SOS components with TAU instrumentation (...of SOS itself).")
  message(STATUS ">>> TAU:")
  set (CMAKE_C_COMPILER /usr/local/tau-2.24.1/x86_64/bin/tau_cc.sh)
  add_definitions(-optMemDbg -optTrackPthread -optMpi -optTrackIO -optPDTInst)
  include_directories(/usr/local/tau-2.24.1/include)
  link_directories(/usr/local/tau-2.24.1/x86_64/lib)
endif()

message(STATUS "Building on: ${CMAKE_SYSTEM_NAME}")

find_package("Sqlite3" REQUIRED)
find_package("MPI" REQUIRED)
find_package("Threads" REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${sos_flow_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${sos_flow_SOURCE_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${sos_flow_SOURCE_DIR}/lib")

if(ENABLE_ADIOS_EXAMPLES)
  message(STATUS ">>> ADIOS:")
  message(STATUS ">>> ADIOS:  Searching for the ADIOS install location...")
  message(STATUS ">>> ADIOS:")
  find_package("ADIOS")
  if(ADIOS_FOUND)
    message(STATUS ">>> ADIOS:")
    message(STATUS ">>> ADIOS:  ADIOS has been found!")
    message(STATUS ">>> ADIOS:  Building the ADIOS-enabled workflow examples with ADIOS ${ADIOS_VERSION}.")
    message(STATUS ">>> ADIOS:")
    subdirs (src tests examples/synthetic_1)
  else()
    message(STATUS ">>> ADIOS  ** ERROR **:")
    message(STATUS ">>> ADIOS  ** ERROR **:  Could not locate an ADIOS installation!  ($PATH, or 'adios_config -d')")
    message(STATUS ">>> ADIOS  ** ERROR **:")
    subdirs (src tests)
  endif()
else()
  subdirs (src tests)
endif()

