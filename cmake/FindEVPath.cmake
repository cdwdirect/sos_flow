# Find EVPath
# ~~~~~~~~~~~~
# Copyright (c) 2017, Kevin Huck <khuck at cs.uoregon.edu>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# CMake module to search for EVPath library
#
# If it's found it sets EVPath_FOUND to TRUE
# and following variables are set:
#    EVPath_INCLUDE_DIR
#    EVPath_LIBRARIES

# First, look in only one variable, EVPath_DIR.

if (NOT DEFINED EVPath_DIR)

    # All upper case options
    if (DEFINED EVPATH_DIR)
        set(EVPath_DIR ${EVPATH_DIR} CACHE STRING "Path to EVPath installation")
    endif (DEFINED EVPATH_DIR)
    if (DEFINED EVPATH_ROOT)
        set(EVPATH_DIR ${EVPATH_ROOT} CACHE STRING "Path to EVPath installation")
    endif (DEFINED EVPATH_ROOT)
    if (DEFINED ENV{EVPATH_DIR})
        set(EVPATH_DIR $ENV{EVPATH_DIR} CACHE STRING "Path to EVPath installation")
    endif (DEFINED ENV{EVPATH_DIR})
    if (DEFINED ENV{EVPATH_ROOT})
        set(EVPATH_DIR $ENV{EVPATH_ROOT} CACHE STRING "Path to EVPath installation")
    endif (DEFINED ENV{EVPATH_ROOT})

    # Mixed case options
    if (DEFINED EVPath_ROOT)
        set(EVPath_DIR ${EVPath_ROOT} CACHE STRING "Path to EVPath installation")
    endif (DEFINED EVPath_ROOT)
    if (DEFINED ENV{EVPath_DIR})
        set(EVPath_DIR $ENV{EVPath_DIR} CACHE STRING "Path to EVPath installation")
    endif (DEFINED ENV{EVPath_DIR})
    if (DEFINED ENV{EVPath_ROOT})
        set(EVPath_DIR $ENV{EVPath_ROOT} CACHE STRING "Path to EVPath installation")
    endif (DEFINED ENV{EVPath_ROOT})
endif (NOT DEFINED EVPath_DIR)

# First, see if the evpath_config program is in our path.  
# If so, use it.

message("FindEVPath: looking for evpath_config")
find_program (EVPath_CONFIG NAMES evpath_config 
              PATHS 
              "${EVPath_DIR}/bin"
              NO_DEFAULT_PATH)

  if(EVPath_CONFIG)
    message("FindEVPath: run evpath_config")
    execute_process(COMMAND ${EVPath_CONFIG} "-l"
        OUTPUT_VARIABLE evpath_config_out
        RESULT_VARIABLE evpath_config_ret
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message("FindEVPath: return value = ${evpath_config_ret}")
    message("FindEVPath: output = ${evpath_config_out}")
    if(evpath_config_ret EQUAL 0)
        string(REPLACE " " ";" evpath_config_list ${evpath_config_out})
    message("FindEVPath: list = ${evpath_config_list}")
        set(evpath_libs)
        set(evpath_lib_hints)
        set(evpath_lib_flags)
        foreach(OPT IN LISTS evpath_config_list)
            if(OPT MATCHES "^-L(.*)")
                list(APPEND evpath_lib_hints "${CMAKE_MATCH_1}")
            elseif(OPT MATCHES "^-l(.*)")
                list(APPEND evpath_libs "${CMAKE_MATCH_1}")
            else()
                list(APPEND evpath_libs "${OPT}")
            endif()
        endforeach()
        set(HAVE_EVPath 1)
    endif()
    message("FindEVPath: hints = ${evpath_lib_hints}")
    message("FindEVPath: libs = ${evpath_libs}")
    message("FindEVPath: flags = ${evpath_lib_flags}")
    set(EVPath_LIBRARIES)
    foreach(lib IN LISTS evpath_libs)
      find_library(evpath_${lib}_LIBRARY NAME ${lib} HINTS ${evpath_lib_hints})
      if(evpath_${lib}_LIBRARY)
        list(APPEND EVPath_LIBRARIES ${evpath_${lib}_LIBRARY})
      else()
        list(APPEND EVPath_LIBRARIES ${lib})
      endif()
    endforeach()
    set(HAVE_EVPath 1)
    set(EVPath_LIBRARIES "${EVPath_LIBRARIES}" CACHE STRING "")
    FIND_PATH(EVPath_INCLUDE_DIR evpath.h "${EVPath_DIR}/include" NO_DEFAULT_PATH)

else(EVPath_CONFIG)

    pkg_search_module(EVPath REQUIRED libenet QUIET)
    # could be needed on some platforms
    pkg_search_module(FABRIC libfabric QUIET)

    if(NOT EVPath_FOUND)

    # FIND_PATH and FIND_LIBRARY normally search standard locations
    # before the specified paths. To search non-standard paths first,
    # FIND_* is invoked first with specified paths and NO_DEFAULT_PATH
    # and then again with no specified paths to search the default
    # locations. When an earlier FIND_* succeeds, subsequent FIND_*s
    # searching for the same item do nothing. 

    FIND_PATH(EVPath_INCLUDE_DIR evpath.h 
        "${EVPath_DIR}/include" NO_DEFAULT_PATH)
    FIND_PATH(EVPath_INCLUDE_DIR evpath.h)

    FIND_LIBRARY(EVPath_LIBRARIES NAMES libenet.a PATHS
        "${EVPath_DIR}/lib" NO_DEFAULT_PATH)
    FIND_LIBRARY(EVPath_LIBRARIES NAMES enet)

    endif()
    
endif(EVPath_CONFIG)

IF (EVPath_INCLUDE_DIR AND EVPath_LIBRARIES)
   SET(EVPath_FOUND TRUE)
ENDIF (EVPath_INCLUDE_DIR AND EVPath_LIBRARIES)

IF (EVPath_FOUND)

   IF (NOT EVPath_FIND_QUIETLY)
      MESSAGE(STATUS "Found EVPath: ${EVPath_LIBRARY}")
   ENDIF (NOT EVPath_FIND_QUIETLY)

ELSE (EVPath_FOUND)

   IF (EVPath_FIND_REQUIRED)
      MESSAGE(FATAL_ERROR "Could not find EVPath")
   ENDIF (EVPath_FIND_REQUIRED)

ENDIF (EVPath_FOUND)
