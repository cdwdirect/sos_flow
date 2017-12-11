cmake_minimum_required (VERSION 2.8)

option(SOS_FORCE_RPATH
    "Add the rpath to the libraries when linking examples?"
    ON)

option(SOS_CLOUD_SYNC_WITH_MPI
    "Use MPI discover and synchronize data between daemons?  (STALE BRANCH)"
    OFF)

option(SOS_CLOUD_SYNC_WITH_EVPATH
    "Use EVPATH synchronize data between daemons?  (RECOMMENDED)"
    ON)

option(SOS_SECURE_WITH_MUNGE
    "Use the Munge library to verify SOSflow messages?"
    OFF)

option(SOS_ENABLE_ADIOS_EXAMPLES
    "Build the examples that require the presence of ADIOS?"
    OFF)

option(SOS_DAEMON_DETACH
    "Have the daemon fork, detach, and get its own session?  (EXPERIMENTAL)"
    OFF)

option(SOS_CLOUD_SYNC
    "Activate cloud_sync operations for the daemon?"
    ON)

###########
# Set some general project CMake settings
###########

#set(RULE_MESSAGES OFF)
set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})
set(PROJECT_NAME sos_flow)

set(PEDANTIC TRUE CACHE BOOL "Treat all compiler warnings as errors")

include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC GIT_SHA1)
add_definitions("-DGIT_SHA1=${GIT_SHA1}")

project(${PROJECT_NAME} C CXX)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99")
endif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DSOS_BUILDER=$ENV{USER} -DSOS_BUILT_FOR=${CMAKE_SYSTEM_NAME} -DSOS_HOST_NODE_NAME=$ENV{SOS_HOST_NODE_NAME} -DSOS_HOST_DETAILED=$ENV{SOS_HOST_DETAILED} -DSOS_HOST_KNOWN_AS=$ENV{SOS_HOST_KNOWN_AS} -DGIT_SHA1=${GIT_SHA1}")
if(DEFINED SOS_SANITIZE_ADDRESS AND SOS_SANITIZE_ADDRESS)
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=address")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=address")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=address")
endif(DEFINED SOS_SANITIZE_ADDRESS AND SOS_SANITIZE_ADDRESS)
if(DEFINED SOS_SANITIZE_THREAD AND SOS_SANITIZE_THREAD)
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -fsanitize=thread")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=thread")
  set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -fsanitize=thread")
endif(DEFINED SOS_SANITIZE_THREAD AND SOS_SANITIZE_THREAD)

if(MAKE_CXX_COMPILER_VERSION VERSION_GREATER 4.7)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif()

set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)
set(POSITION_INDEPENDENT_CODE TRUE)

message(STATUS "Building on: ${CMAKE_SYSTEM_NAME}")

# Required packages
find_package(PkgConfig REQUIRED)
find_package("Sqlite3" REQUIRED)

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
# SOS_CLOUD_SYNC_WITH_MPI OPTIONS
###########

if(SOS_CLOUD_SYNC_WITH_MPI AND SOS_CLOUD_SYNC_WITH_EVPATH)
  message(FATAL_ERROR
  "You cannot use both MPI and EVPATH for daemon cloud_sync operations!  Please select only one.")
endif()

if((SOS_CLOUD_SYNC_WITH_MPI OR SOS_CLOUD_SYNC_WITH_EVPATH) AND NOT SOS_CLOUD_SYNC)
  message(FATAL_ERROR
  "You have specified a method for cloud_sync, but disabled cloud_sync.  Verify your build options.")
endif()

###########
# SOS_SECURE_WITH_MUNGE OPTIONS
###########

if(SOS_SECURE_WITH_MUNGE)
    add_definitions("-DUSE_MUNGE=1")
    message(STATUS "MUNGE:  Munge is ENABLED!")
endif()

###########
# SOS_CLOUD_SYNC_WITH_EVPATH OPTIONS
###########

if(SOS_CLOUD_SYNC_WITH_EVPATH)
  message(STATUS "EVPATH:  Sync with EVPath is ENABLED")
  message(STATUS "EVPATH:  Locating package...")
  find_package(EVPath REQUIRED QUIET)
  add_definitions(-DSOSD_CLOUD_SYNC_WITH_EVPATH)
endif()

###########
# SOS_CLOUD_SYNC_WITH_MPI OPTIONS
###########

if(SOS_CLOUD_SYNC_WITH_MPI)
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
find_package(Threads)
if(Threads_FOUND)
    set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
else()
	set(LIBS ${LIBS} -lpthread)
endif()

###########
# Special cases handled here
###########

# check for Cray static linking
#if (CMAKE_C_COMPILER EQUAL "cc" AND CMAKE_CXX_COMPILER EQUAL "CC")
	find_library(DYNAMICLIB dl)
	set(LIBS ${LIBS} ${DYNAMICLIB})
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
