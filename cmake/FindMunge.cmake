# Find Munge
# ~~~~~~~~~~~~
# Copyright (c) 2017, Kevin Huck <khuck at cs.uoregon.edu>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# CMake module to search for Munge library
#
# If it's found it sets Munge_FOUND to TRUE
# and following variables are set:
#    Munge_INCLUDE_DIR
#    Munge_LIBRARIES

# First, look in only one variable, Munge_DIR.  This script will accept any of:
# MUNGE_DIR, MUNGE_ROOT, Munge_DIR, Munge_ROOT, or environment variables
# using the same set of names.


if (Munge_DIR STREQUAL "not set")

    # All upper case options
    if (DEFINED MUNGE_DIR)
        set(Munge_DIR ${MUNGE_DIR})
    endif (DEFINED MUNGE_DIR)
    if (DEFINED MUNGE_ROOT)
        set(Munge_DIR ${MUNGE_ROOT})
    endif (DEFINED MUNGE_ROOT)
    if (DEFINED ENV{MUNGE_DIR})
        set(Munge_DIR $ENV{MUNGE_DIR})
    endif (DEFINED ENV{MUNGE_DIR})
    if (DEFINED ENV{MUNGE_ROOT})
        set(Munge_DIR $ENV{MUNGE_ROOT})

    endif (DEFINED ENV{MUNGE_ROOT})

    # Mixed case options
    if (DEFINED Munge_ROOT)
        set(Munge_DIR ${Munge_ROOT})
    endif (DEFINED Munge_ROOT)
    if (DEFINED ENV{Munge_DIR})
        set(Munge_DIR $ENV{Munge_DIR})
    endif (DEFINED ENV{Munge_DIR})
    if (DEFINED ENV{Munge_ROOT})
        set(Munge_DIR $ENV{Munge_ROOT})
    endif (DEFINED ENV{Munge_ROOT})
endif (Munge_DIR STREQUAL "not set")


# FIND_PATH and FIND_LIBRARY normally search standard locations
# before the specified paths. To search non-standard paths first,
# FIND_* is invoked first with specified paths and NO_DEFAULT_PATH
# and then again with no specified paths to search the default
# locations. When an earlier FIND_* succeeds, subsequent FIND_*s
# searching for the same item do nothing. 

FIND_PATH(Munge_INCLUDE_DIR munge.h 
    ${Munge_DIR}/include NO_DEFAULT_PATH)
    FIND_PATH(Munge_INCLUDE_DIR munge.h)

FIND_LIBRARY(Munge_LIBRARIES NAMES munge PATHS
    ${Munge_DIR}/lib NO_DEFAULT_PATH)
    FIND_LIBRARY(Munge_LIBRARIES NAMES munge)

IF (Munge_INCLUDE_DIR AND Munge_LIBRARIES)
   SET(Munge_FOUND TRUE)
ENDIF (Munge_INCLUDE_DIR AND Munge_LIBRARIES)

IF (Munge_FOUND)

   IF (NOT Munge_FIND_QUIETLY)
      MESSAGE(STATUS "Found Munge: ${Munge_LIBRARIES}")
   ENDIF (NOT Munge_FIND_QUIETLY)

ELSE (Munge_FOUND)

   IF (Munge_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find Munge")
   ENDIF (Munge_FIND_REQUIRED)

ENDIF (Munge_FOUND)
