
cmake_minimum_required (VERSION 2.8)

########### ########### ########### ########### ########### ########### ###########
# User-configurable options.
########### ########### ########### ########### ########### ########### ###########

# --- Paths to dependencies ---
#
# NOTE: Use 'ccmake' within build folder to set them, or set at the
#       command like with: $ cmake -DEVPath_DIR=... rather than modifying
#       this file.

set(SQLite3_DIR    "" CACHE STRING
    "Path to SQLite3 installation (if not system install)")
set(Munge_DIR      "" CACHE STRING
    "Path to Munge installation (lightweight edge authentication for HPC)")
set(EVPath_DIR     "" CACHE STRING
    "Path to EVPath (ADIOS chaos libraries) installation")

# --- Inter-daemon communication method ---
#
# NOTE: Only ONE may be set to ON at a time.

option(SOS_CLOUD_SYNC_WITH_EVPATH "Sync daemons with EVPath? (RECOMMENDED)"  ON)
option(SOS_CLOUD_SYNC_WITH_MPI    "Sync daemons with MPI?    (EXPERIMENTAL)" OFF)
option(SOS_CLOUD_SYNC_WITH_SOCKET "Sync daemons with TCP/IP? (EXPERIMENTAL)" OFF)
option(SOS_CLOUD_SYNC_WITH_ZEROMQ "Sync daemons with ZeroMQ? (EXPERIMENTAL)" OFF)

# --- Other SOS options ---

option(SOS_SECURE_WITH_MUNGE
    "Use the Munge library to verify SOSflow messages?"
    OFF)

option(SOS_DAEMON_DETACH
    "Have the daemon fork, detach, and get its own session?  (EXPERIMENTAL)"
    OFF)

option(SOS_ENABLE_PYTHON
    "Build python analysis interface for client querying of aggregators/listeners"
    OFF)

option(BUILD_SHARED_LIBS
    "Build Shared libraries? (set to OFF/FALSE/NO for static build)"
    ON)

option(SOS_CLOUD_SYNC
    "Activate cloud_sync operations for the daemon? (DEPRECATED, ALWAYS 'ON')"
    ON)


# --- Advanced options ---

option(SOS_FORCE_RPATH
    "Add the rpath to the libraries when linking examples?"
    ON)

option(SOS_SANITIZE_ADDRESS
    "Enable address sanitizer compiler flags"
    OFF)

option(SOS_SANITIZE_THREAD
    "Enable thread sanitizer compiler flags"
    OFF)

########### ########### ########### ########### ########### ########### ###########

###########
# Set some general project CMake settings
###########

set(PROJECT_NAME sos_flow)
project(${PROJECT_NAME} C CXX)

#set(RULE_MESSAGES OFF)
set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

set(PEDANTIC TRUE CACHE BOOL "Treat all compiler warnings as errors")

# Pull in the GIT repo information
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)
add_definitions("-DGIT_SHA1=${GIT_SHA1}")

# Set some host information

execute_process(COMMAND uname -n OUTPUT_VARIABLE SOS_HOST_NODE_NAME)
string(STRIP ${SOS_HOST_NODE_NAME} SOS_HOST_NODE_NAME)
if(NOT APPLE)
    execute_process(COMMAND uname -o OUTPUT_VARIABLE SOS_HOST_DETAILED_O)
    string(STRIP ${SOS_HOST_DETAILED_O} SOS_HOST_DETAILED_O)
else()
    execute_process(COMMAND uname -s OUTPUT_VARIABLE SOS_HOST_DETAILED_O)
    string(STRIP ${SOS_HOST_DETAILED_O} SOS_HOST_DETAILED_O)
endif()
execute_process(COMMAND uname -r OUTPUT_VARIABLE SOS_HOST_DETAILED_R)
string(STRIP ${SOS_HOST_DETAILED_R} SOS_HOST_DETAILED_R)
execute_process(COMMAND uname -m OUTPUT_VARIABLE SOS_HOST_DETAILED_M)
string(STRIP ${SOS_HOST_DETAILED_M} SOS_HOST_DETAILED_M)

SET(SOS_HOST_KNOWN_AS ${SOS_HOST_NODE_NAME})
SET(SOS_HOST_DETAILED "\"${SOS_HOST_DETAILED_O} ${SOS_HOST_DETAILED_R} ${SOS_HOST_DETAILED_M}\"")
message(STATUS "Building on: ${CMAKE_SYSTEM_NAME}")

# Set general Compiler flags

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
endif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSOS_BUILDER=$ENV{USER} -DSOS_BUILT_FOR=${CMAKE_SYSTEM_NAME} -DSOS_HOST_NODE_NAME=${SOS_HOST_NODE_NAME} -DSOS_HOST_DETAILED=${SOS_HOST_DETAILED} -DSOS_HOST_KNOWN_AS=${SOS_HOST_KNOWN_AS} -DGIT_SHA1=${GIT_SHA1}")

if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")
    if(CMAKE_C_COMPILER_VERSION VERSION_LESS 5.0)
        if(SOS_SANITIZE_ADDRESS OR SOS_SANITIZE_THREAD)
            message("Sorry, Cannot enable sanitizer options for gcc compiler version < 5.0")
        endif(SOS_SANITIZE_ADDRESS OR SOS_SANITIZE_THREAD)
    else(CMAKE_C_COMPILER_VERSION VERSION_LESS 5.0)
        if(SOS_SANITIZE_ADDRESS)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
        endif(SOS_SANITIZE_ADDRESS)

        if(SOS_SANITIZE_THREAD)
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
        endif(SOS_SANITIZE_THREAD)
    endif(CMAKE_C_COMPILER_VERSION VERSION_LESS 5.0)
endif ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU")

if(MAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.7)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(POSITION_INDEPENDENT_CODE TRUE)

###########
# SOS_FORCE_RPATH OPTIONS
###########

if(SOS_FORCE_RPATH)
    # use, i.e. don't skip the full RPATH for the build tree
    SET(CMAKE_SKIP_BUILD_RPATH  FALSE)

    # when building, don't use the install RPATH already
    # (but later on when installing)
    SET(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)

    SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

    # add the automatically determined parts of the RPATH
    # which point to directories outside the build tree to the install RPATH
    SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

    # the RPATH to be used when installing, but only if it's not a system directory
    LIST(FIND CMAKE_PLATFORM_IMPLICIT_LINK_DIRECTORIES "${CMAKE_INSTALL_PREFIX}/lib" isSystemDir)
    IF("${isSystemDir}" STREQUAL "-1")
        SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
    ENDIF("${isSystemDir}" STREQUAL "-1")
endif(SOS_FORCE_RPATH)

###########
# Required packages
###########

find_package("SQLite3" REQUIRED QUIET)
find_package("PkgConfig" REQUIRED QUIET)

###########
# SOS_CLOUD_SYNC OPTIONS - validate the selected configuration
###########

# Verify that only one sync method has been enabled:

set(SOS_A SOS_CLOUD_SYNC_WITH_MPI)
set(SOS_B SOS_CLOUD_SYNC_WITH_EVPATH)
set(SOS_C SOS_CLOUD_SYNC_WITH_SOCKET)
set(SOS_D SOS_CLOUD_SYNC_WITH_ZEROMQ)
set(SOS_OPT_USING_CLOUD (SOS_A OR SOS_B OR SOS_C OR SOS_D))
if(    (SOS_A AND SOS_B)
    OR (SOS_A AND SOS_C)
    OR (SOS_A AND SOS_D)
    OR (SOS_B AND SOS_C)
    OR (SOS_B AND SOS_D)
    OR (SOS_C AND SOS_D))
    message(STATUS
        "You have specified more than one inter-daemon synchronization method:")
    message(STATUS "    SOS_CLOUD_SYNC_WITH_EVPATH = ${SOS_CLOUD_SYNC_WITH_EVPATH}")
    message(STATUS "    SOS_CLOUD_SYNC_WITH_MPI    = ${SOS_CLOUD_SYNC_WITH_MPI}")
    message(STATUS "    SOS_CLOUD_SYNC_WITH_SOCKET = ${SOS_CLOUD_SYNC_WITH_SOCKET}")
    message(STATUS "    SOS_CLOUD_SYNC_WITH_ZEROMQ = ${SOS_CLOUD_SYNC_WITH_ZEROMQ}")
    message(FATAL_ERROR
        "Verify your build options.")
endif()

# Verify that the correct defines will propagate through the code:

if(SOS_OPT_USING_CLOUD AND (NOT SOS_CLOUD_SYNC))
    message(STATUS
        "You have specified a method for cloud_sync, but disabled cloud_sync.")
    message(STATUS
        "It is recommended that SOS_CLOUD_SYNC always be set to 'ON'.")
    message(FATAL_ERROR
        "Verify your build options.")
endif()





###########
# SOS_CLOUD_SYNC_WITH_EVPATH OPTIONS
###########

if(SOS_CLOUD_SYNC_WITH_EVPATH)
    message(STATUS "EVPATH:  Sync with EVPath is ENABLED")
    message(STATUS "EVPATH:  Locating package...")
    find_package(EVPath REQUIRED QUIET)
    add_definitions(-DSOSD_CLOUD_SYNC_WITH_EVPATH)
    if (NOT EVPath_FOUND)
        message(STATIS "EVPATH:  WARNING!  Could NOT find EVPath!")
    else()
        message(STATUS "EVPATH:  --EVPath_DIR         = ${EVPath_DIR}")
        message(STATUS "EVPATH:  --EVPath_INCLUDE_DIR = ${EVPath_INCLUDE_DIR}")
        message(STATUS "EVPATH:  --EVPath_LIBRARIES   = ${EVPath_LIBRARIES}")
    endif()
endif()

###########
# SOS_CLOUD_SYNC_WITH_MPI OPTIONS
###########
if(SOS_CLOUD_SYNC_WITH_MPI)
    message(STATUS "MPI:  Sync with MPI is ENABLED")
    message(STATUS "MPI:  Locating package...")
    if(NOT MPI_C_NO_INTERROGATE)
        find_package("MPI" QUIET REQUIRED)
    else()
        set(MPI_FOUND ON)
    endif(NOT MPI_C_NO_INTERROGATE)

    if(MPI_FOUND)
        add_definitions("-DUSE_MPI=1")
        message(STATUS "MPI:  Found!")
    endif(MPI_FOUND)
    add_definitions(-DSOSD_CLOUD_SYNC_WITH_MPI)
endif(SOS_CLOUD_SYNC_WITH_MPI)

###########
# SOS_CLOUD_SYNC_WITH_SOCKET OPTIONS
###########
if(SOS_CLOUD_SYNC_WITH_SOCKET)
    message(STATUS "SOCKET:  Sync with SOCKET (TCP/IP) is ENABLED")
    add_definitions(-DSOSD_CLOUD_SYNC_WITH_SOCKET)
endif()


###########
# SOS_CLOUD_SYNC_WITH_ZEROMQ OPTIONS
###########
if(SOS_CLOUD_SYNC_WITH_ZEROMQ)
    message(STATUS "ZEROMQ:  Sync with ZEROMQ is ENABLED")
    message(STATUS "ZEROMQ:  Including external submodule...")
    subdirs(ext/libzmq)
    subdirs(ext/czmq)
endif()



###########
# SOS_SECURE_WITH_MUNGE OPTIONS
###########

if(SOS_SECURE_WITH_MUNGE)
    find_package(PkgConfig REQUIRED)
    find_package(Munge QUIET REQUIRED)
    add_definitions("-DUSE_MUNGE=1")
    message(STATUS "MUNGE:  Munge is ENABLED!")
endif()

###########
# SOS_ENABLE_PYTHON OPTIONS
###########

if(SOS_ENABLE_PYTHON)
    #pkg_search_module(PYTHON REQUIRED python QUIET)
    message(STATUS "PYTHON:  Locating package...")
    if(PYTHON_FOUND)
    message(STATUS "PYTHON:  Found!")
    endif(PYTHON_FOUND)
endif(SOS_ENABLE_PYTHON)

###########
# Enable threading
###########

# set everything up to use -pthread
set(CMAKE_THREAD_PREFER_PTHREAD ON)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
if(Threads_FOUND)
    set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
    link_libraries(${CMAKE_THREAD_LIBS_INIT})
else()
    set(LIBS ${LIBS} -lpthread)
    link_libraries(pthread)
endif()

###########
# Special cases handled here
###########

# check for Cray static linking
#if (CMAKE_C_COMPILER EQUAL "cc" AND CMAKE_CXX_COMPILER EQUAL "CC")
find_library(DYNAMICLIB dl)
set(LIBS "${LIBS} ${DYNAMICLIB}")
#endif (CMAKE_C_COMPILER EQUAL "cc" AND CMAKE_CXX_COMPILER EQUAL "CC")

###########
# Other boilerplate CMake stuff
###########

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${sos_flow_BINARY_DIR}/tmpbin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${sos_flow_BINARY_DIR}/tmplib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${sos_flow_BINARY_DIR}/tmplib")

subdirs (src tests)

if(SOS_ENABLE_PYTHON)
    subdirs (src/python)
endif(SOS_ENABLE_PYTHON)


