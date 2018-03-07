# Find CZMQ Headers/Libs

# Variables
# CZMQ_DIR - set this to a location where ZeroMQ may be found
#
# CZMQ_FOUND        - True if CZMQ found
# CZMQ_INCLUDE_DIRS - Location of CZMQ includes
# CZMQ_LIBRARIES    - CZMQ libraries

include(FindPackageHandleStandardArgs)

if (NOT CZMQ_DIR)
    set(CZMQ_DIR "$ENV{CZMQ_DIR}")
endif()

if (NOT CZMQ_DIR)
    find_path(_CZMQ_ROOT NAMES include/czmq.h)
else()
    set(_CZMQ_ROOT "${CZMQ_DIR}")
endif()

find_path(CZMQ_INCLUDE_DIRS NAMES czmq.h HINTS ${_CZMQ_ROOT}/include)

if (CZMQ_INCLUDE_DIRS)
    set(_CZMQ_H ${CZMQ_INCLUDE_DIRS}/czmq.h)
    find_library(CZMQ_LIBRARIES NAMES czmq HINTS ${_CZMQ_ROOT}/lib)
endif()

find_package_handle_standard_args(CZMQ FOUND_VAR CZMQ_FOUND
    REQUIRED_VARS CZMQ_INCLUDE_DIRS CZMQ_LIBRARIES)

if (CZMQ_FOUND)
    mark_as_advanced(CZMQ_INCLUDE_DIRS CZMQ_LIBRARIES)
endif()
