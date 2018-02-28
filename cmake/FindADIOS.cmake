# Find ADIOS
# ~~~~~~~~~~~~
# Copyright (c) 2017, Kevin Huck <khuck at cs.uoregon.edu>
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#
# CMake module to search for ADIOS library
#
# If it's found it sets ADIOS_FOUND to TRUE
# and following variables are set:
#    ADIOS_INCLUDE_DIR
#    ADIOS_LIBRARIES

# First, look in only one variable, ADIOS_DIR.  This script will accept any of:
# EVPATH_DIR, EVPATH_ROOT, ADIOS_DIR, ADIOS_ROOT, or environment variables
# using the same set of names.


if ("${ADIOS_DIR} " STREQUAL " ")
    IF (NOT ADIOS_FIND_QUIETLY)
        message("ADIOS_DIR not set, trying alternatives...")
    ENDIF (NOT ADIOS_FIND_QUIETLY)

    if (DEFINED ADIOS_ROOT)
        set(ADIOS_DIR ${ADIOS_ROOT})
    endif (DEFINED ADIOS_ROOT)
    if (DEFINED ENV{ADIOS_DIR})
        set(ADIOS_DIR $ENV{ADIOS_DIR})
    endif (DEFINED ENV{ADIOS_DIR})
    if (DEFINED ENV{ADIOS_ROOT})
        set(ADIOS_DIR $ENV{ADIOS_ROOT})
    endif (DEFINED ENV{ADIOS_ROOT})
endif ("${ADIOS_DIR} " STREQUAL " ")

IF (NOT ADIOS_FIND_QUIETLY)
MESSAGE(STATUS "ADIOS_DIR set to: '${ADIOS_DIR}'")
ENDIF (NOT ADIOS_FIND_QUIETLY)


# First, see if the adios_config program is in our path.  
# If so, use it.

IF (NOT ADIOS_FIND_QUIETLY)
    message("FindADIOS: looking for adios_config")
ENDIF (NOT ADIOS_FIND_QUIETLY)
find_program (ADIOS_CONFIG NAMES adios_config 
              PATHS 
              "${ADIOS_DIR}/bin"
              NO_DEFAULT_PATH)

  if(ADIOS_CONFIG)
    IF (NOT ADIOS_FIND_QUIETLY)
        message("FindADIOS: run ${ADIOS_CONFIG}")
    ENDIF (NOT ADIOS_FIND_QUIETLY)
    execute_process(COMMAND ${ADIOS_CONFIG} "-l"
        OUTPUT_VARIABLE adios_config_out
        RESULT_VARIABLE adios_config_ret
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    IF (NOT ADIOS_FIND_QUIETLY)
        message("FindADIOS: return value = ${adios_config_ret}")
        message("FindADIOS: output = ${adios_config_out}")
    ENDIF (NOT ADIOS_FIND_QUIETLY)
    if(adios_config_ret EQUAL 0)
        string(REPLACE " " ";" adios_config_list ${adios_config_out})
        IF (NOT ADIOS_FIND_QUIETLY)
            message("FindADIOS: list = ${adios_config_list}")
        ENDIF (NOT ADIOS_FIND_QUIETLY)
        set(adios_libs)
        set(adios_lib_hints)
        set(adios_lib_flags)
        foreach(OPT IN LISTS adios_config_list)
            if(OPT MATCHES "^-L(.*)")
                list(APPEND adios_lib_hints "${CMAKE_MATCH_1}")
            elseif(OPT MATCHES "^-l(.*)")
                list(APPEND adios_libs "${CMAKE_MATCH_1}")
            else()
                list(APPEND adios_libs "${OPT}")
            endif()
        endforeach()
        set(HAVE_ADIOS 1)
    endif()
    IF (NOT ADIOS_FIND_QUIETLY)
        message("FindADIOS: hints = ${adios_lib_hints}")
        message("FindADIOS: libs = ${adios_libs}")
        message("FindADIOS: flags = ${adios_lib_flags}")
    ENDIF (NOT ADIOS_FIND_QUIETLY)
    set(ADIOS_LIBRARIES)
    foreach(lib IN LISTS adios_libs)
      find_library(adios_${lib}_LIBRARY NAME ${lib} HINTS ${adios_lib_hints})
      if(adios_${lib}_LIBRARY)
        list(APPEND ADIOS_LIBRARIES ${adios_${lib}_LIBRARY})
      else()
        list(APPEND ADIOS_LIBRARIES ${lib})
      endif()
    endforeach()
    set(HAVE_ADIOS 1)
    set(ADIOS_LIBRARIES "${ADIOS_LIBRARIES}" CACHE STRING "")
    FIND_PATH(ADIOS_INCLUDE_DIR adios.h "${ADIOS_DIR}/include" NO_DEFAULT_PATH)

else(ADIOS_CONFIG)
    message("FindADIOS: adios_config not found, trying pkg-config method...")
    find_package(PkgConfig REQUIRED)

    pkg_search_module(ADIOS libadios QUIET)

    if(NOT ADIOS_FOUND)

    # FIND_PATH and FIND_LIBRARY normally search standard locations
    # before the specified paths. To search non-standard paths first,
    # FIND_* is invoked first with specified paths and NO_DEFAULT_PATH
    # and then again with no specified paths to search the default
    # locations. When an earlier FIND_* succeeds, subsequent FIND_*s
    # searching for the same item do nothing. 

    FIND_PATH(ADIOS_INCLUDE_DIR adios.h 
        "${ADIOS_DIR}/include" NO_DEFAULT_PATH)
    FIND_PATH(ADIOS_INCLUDE_DIR adios.h)

    FIND_LIBRARY(ADIOS_LIBRARIES NAMES libadios.a PATHS
        "${ADIOS_DIR}/lib" NO_DEFAULT_PATH)
    FIND_LIBRARY(ADIOS_LIBRARIES NAMES adios)

    endif()
    
endif(ADIOS_CONFIG)

IF (ADIOS_INCLUDE_DIR AND ADIOS_LIBRARIES)
   SET(ADIOS_FOUND TRUE)
ENDIF (ADIOS_INCLUDE_DIR AND ADIOS_LIBRARIES)

IF (ADIOS_FOUND)

   IF (NOT ADIOS_FIND_QUIETLY)
      MESSAGE(STATUS "Found ADIOS: ${ADIOS_LIBRARIES}")
   ENDIF (NOT ADIOS_FIND_QUIETLY)

ELSE (ADIOS_FOUND)

   message("FindADIOS: Could not find ADIOS")

ENDIF (ADIOS_FOUND)
