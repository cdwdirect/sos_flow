# Find Sqlite3
# ~~~~~~~~~~~~
# Copyright (c) 2007, Martin Dobias <wonder.sk at gmail.com>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# CMake module to search for Sqlite3 library
#
# If it's found it sets SQLite3_FOUND to TRUE
# and following variables are set:
#    SQLite3_INCLUDE_DIR
#    SQLite3_LIBRARY


if (SQLite3_DIR MATCHES "^not set$")
    # All upper case options
    if (DEFINED SQLITE3_DIR)
        set(SQLite3_DIR ${SQLITE3_DIR})
    endif (DEFINED SQLITE3_DIR)
    if (DEFINED SQLITE3_ROOT)
        set(SQLite3_DIR ${SQLITE3_ROOT})
    endif (DEFINED SQLITE3_ROOT)
    if (DEFINED ENV{SQLITE3_DIR})
        set(SQLite3_DIR $ENV{SQLITE3_DIR})
    endif (DEFINED ENV{SQLITE3_DIR})
    if (DEFINED ENV{SQLITE3_ROOT})
        set(SQLite3_DIR $ENV{SQLITE3_ROOT})
    endif (DEFINED ENV{SQLITE3_ROOT})

    # Mixed case options
    if (DEFINED SQLite3_ROOT)
        set(SQLite3_DIR ${SQLite3_ROOT})
    endif (DEFINED SQLite3_ROOT)
    if (DEFINED ENV{SQLite3_DIR})
        set(SQLite3_DIR $ENV{SQLite3_DIR})
    endif (DEFINED ENV{SQLite3_DIR})
    if (DEFINED ENV{SQLite3_ROOT})
        set(SQLite3_DIR $ENV{SQLite3_ROOT})
    endif (DEFINED ENV{SQLite3_ROOT})
endif (SQLite3_DIR MATCHES "^not set$")


# FIND_PATH and FIND_LIBRARY normally search standard locations
# before the specified paths. To search non-standard paths first,
# FIND_* is invoked first with specified paths and NO_DEFAULT_PATH
# and then again with no specified paths to search the default
# locations. When an earlier FIND_* succeeds, subsequent FIND_*s
# searching for the same item do nothing. 

# try to use framework on mac
# want clean framework path, not unix compatibility path
IF (APPLE)
  IF (CMAKE_FIND_FRAMEWORK MATCHES "FIRST"
      OR CMAKE_FRAMEWORK_PATH MATCHES "ONLY"
      OR NOT CMAKE_FIND_FRAMEWORK)
    SET (CMAKE_FIND_FRAMEWORK_save ${CMAKE_FIND_FRAMEWORK} CACHE STRING "" FORCE)
    SET (CMAKE_FIND_FRAMEWORK "ONLY" CACHE STRING "" FORCE)
    #FIND_PATH(SQLite3_INCLUDE_DIR SQLite3/sqlite3.h)
    FIND_LIBRARY(SQLite3_LIBRARY SQLite3)
    IF (SQLite3_LIBRARY)
      # FIND_PATH doesn't add "Headers" for a framework
      SET (SQLite3_INCLUDE_DIR ${SQLite3_LIBRARY}/Headers CACHE PATH "Path to a file.")
    ENDIF (SQLite3_LIBRARY)
    SET (CMAKE_FIND_FRAMEWORK ${CMAKE_FIND_FRAMEWORK_save} CACHE STRING "" FORCE)
  ENDIF ()
ENDIF (APPLE)

FIND_PATH(SQLite3_INCLUDE_DIR sqlite3.h ${SQLite3_DIR}/include)

# prefer libsqlite3.a over .so, if available
FIND_LIBRARY(SQLite3_LIBRARY NAMES libsqlite3.a sqlite3 sqlite3_i PATHS
  ${SQLite3_DIR}/lib)

if(NOT SQLite3_FIND_QUIETLY)
    message("SQLite3_INCLUDE_DIR: ${SQLite3_INCLUDE_DIR}")
    message("SQLite3_LIBRARY: ${SQLite3_LIBRARY}")
endif(NOT SQLite3_FIND_QUIETLY)

IF (SQLite3_INCLUDE_DIR AND SQLite3_LIBRARY)
   SET(SQLite3_FOUND TRUE)
ENDIF (SQLite3_INCLUDE_DIR AND SQLite3_LIBRARY)


IF (SQLite3_FOUND)

   IF (NOT SQLite3_FIND_QUIETLY)
      MESSAGE(STATUS "Found Sqlite3: ${SQLite3_LIBRARY}")
   ENDIF (NOT SQLite3_FIND_QUIETLY)

ELSE (SQLite3_FOUND)

   IF (SQLite3_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find Sqlite3")
   ENDIF (SQLite3_FIND_REQUIRED)

ENDIF (SQLite3_FOUND)
