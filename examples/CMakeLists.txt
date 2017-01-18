cmake_minimum_required (VERSION 2.8)

set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/../cmake ${CMAKE_MODULE_PATH})
set(PEDANTIC TRUE CACHE BOOL "Treat all compiler warnings as errors")

project(sos_flow_examples C)
set(CMAKE_C_FLAGS "-std=gnu99 -O3 -g")
#set(CMAKE_C_FLAGS "-std=gnu99 -O3 -g -pedantic -Wall -Werror -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-function -Wno-maybe-uninitialized")

message(STATUS "Building on: ${CMAKE_SYSTEM_NAME}")

# MPI
find_package(MPI REQUIRED)
# PTHREADS
set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
# SOS
find_package(SOS REQUIRED)
include_directories(${SOS_INCLUDE_DIRS})
set(LIBS ${LIBS} ${SOS_LIBRARIES})

message(STATUS ">>> ADIOS:  Searching for the ADIOS install location...")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ADIOS_ROOT}/etc)
find_package(ADIOS REQUIRED)

if(ADIOS_FOUND)
    if(DEFINED ADIOS_WRAPPER_FLAGS)
        file(READ ${ADIOS_WRAPPER_FLAGS} ADIOS_TAU_FLAGS)
    endif(DEFINED ADIOS_WRAPPER_FLAGS)
    message(STATUS ">>> ADIOS:  ADIOS has been found!")
    message(STATUS ">>> ADIOS:  Building the ADIOS-enabled workflow examples with ADIOS ${ADIOS_VERSION}.")
	if(FIX_ADIOS_DEPENDENCIES)
		find_package(PkgConfig)
    	message(STATUS ">>> ADIOS:  Searching for libenet (set your PKG_CONFIG_PATH if not found).")
		pkg_search_module(ENET REQUIRED libenet)
    	message(STATUS ">>> ADIOS:  Searching for cray-rca (set your PKG_CONFIG_PATH if not found).")
		pkg_search_module(RCA REQUIRED cray-rca)
	endif(FIX_ADIOS_DEPENDENCIES)
    subdirs (general synthetic_1)
else()
    message(FATAL ">>> ADIOS  ** ERROR **:  Could not locate an ADIOS installation!  ($PATH, or 'adios_config -d')")
endif()

find_package(TAU REQUIRED)

# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${sos_flow_examples_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${sos_flow_examples_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${sos_flow_examples_BINARY_DIR}/lib")


