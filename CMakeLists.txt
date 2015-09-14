cmake_minimum_required (VERSION 2.6)

option(SILENCE_DEBUG "Disable DEBUG messages?" OFF)
option(DAEMON_LOGGING "Should the sosd daemon produce a log file?" ON)

set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set(PROJECT_NAME sos_flow)
set(PEDANTIC TRUE CACHE BOOL "Treat all compiler warnings as errors")

project(${PROJECT_NAME} C)

message("Building on: ${CMAKE_SYSTEM_NAME}")

find_package("Sqlite3" REQUIRED)
find_packagE("MPI" REQUIRED)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${sos_flow_SOURCE_DIR}/bin")

subdirs (src)