# - Try to find LibTAU
# Once done this will define
#  TAU_FOUND - System has TAU
#  TAU_INCLUDE_DIRS - The TAU include directories
#  TAU_LIBRARIES - The libraries needed to use TAU
#  TAU_DEFINITIONS - Compiler switches required for using TAU

find_package(PkgConfig)

# This if statement is specific to TAU, and should not be copied into other
# Find cmake scripts.
if(NOT TAU_ROOT AND NOT $ENV{HOME_TAU} STREQUAL "")
  set(TAU_ROOT $ENV{HOME_TAU})
endif()

if(NOT TAU_ARCH AND NOT $ENV{TAU_ARCH} STREQUAL "")
    set(TAU_ARCH $ENV{TAU_ARCH})
endif()

pkg_check_modules(PC_TAU QUIET TAU)
set(TAU_DEFINITIONS ${PC_TAU_CFLAGS_OTHER})

find_path(TAU_INCLUDE_DIR TAU.h
          HINTS ${TAU_ROOT}/include
          PATH_SUFFIXES TAU )

if (${APPLE})
    find_library(TAU_LIBRARY NAMES TAUsh${TAU_CONFIG} tau${TAU_CONFIG} TAU 
             HINTS ${TAU_ROOT}/apple/lib)
         find_path(TAU_LIBRARY_DIR NAMES TAUsh${TAU_CONFIG}.so tau${TAU_CONFIG}.a libTAU.dylib
             HINTS ${TAU_ROOT}/apple/lib)
else()
    find_library(TAU_LIBRARY NAMES TAUsh${TAU_CONFIG} tau${TAU_CONFIG} TAU 
      HINTS ${TAU_ROOT}/${TAU_ARCH}/lib ${TAU_ROOT}/${CMAKE_SYSTEM_PROCESSOR}/lib  ${TAU_ROOT}/*/lib )
  find_path(TAU_LIBRARY_DIR NAMES tau${TAU_CONFIG}.a TAUsh${TAU_CONFIG}.so libTAU.so libTAU.a libTAU.dylib
      HINTS ${TAU_ROOT}/${TAU_ARCH}/lib ${TAU_ROOT}/${CMAKE_SYSTEM_PROCESSOR}/lib  ${TAU_ROOT}/*/lib )
endif()

message(STATUS "Looking for TAU Pthread wrapper in ${TAU_LIBRARY_DIR}/static${TAU_CONFIG}")
find_path(TAU_LIBRARY_DIR_2 NAME libTauPthreadWrap.a
             PATHS ${TAU_LIBRARY_DIR}/static${TAU_CONFIG})

# set(TAU_LIBRARIES ${TAU_LIBRARY})
# get the full list of TAU libraries.
set(ENV{TAU_MAKEFILE} ${TAU_ROOT}/${TAU_ARCH}/lib/Makefile.tau${TAU_CONFIG})
set(TAU_CC_CMD ${TAU_ROOT}/${TAU_ARCH}/bin/tau_cc.sh)
message(STATUS "TAU CC: ${TAU_CC_CMD}")
execute_process(COMMAND ${TAU_CC_CMD} -tau:showlibs OUTPUT_VARIABLE TAU_LIBRARIES)
message(STATUS "TAU libraries: ${TAU_LIBRARIES}")

if (TAU_LIBRARY_DIR_2)
    message(STATUS "TAU Pthread wrapper found: ${TAU_LIBRARY_DIR_2}")
    file(READ ${TAU_LIBRARY_DIR}/wrappers/pthread_wrapper/link_options.tau TAU_PTHREAD_FLAGS)
    string(STRIP ${TAU_PTHREAD_FLAGS} TAU_PTHREAD_FLAGS_STRIPPED)
    set(TAU_PTHREAD_WRAPPER -L${TAU_LIBRARY_DIR_2} ${TAU_PTHREAD_FLAGS_STRIPPED})
    message(STATUS "TAU_PTHREAD_WRAPPER: ${TAU_PTHREAD_WRAPPER}")
else()
    set(TAU_PTHREAD_WRAPPER "")
endif()

set(TAU_INCLUDE_DIRS ${TAU_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set TAU_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(TAU  DEFAULT_MSG
                                  TAU_LIBRARY TAU_INCLUDE_DIR TAU_LIBRARIES )

mark_as_advanced(TAU_INCLUDE_DIR TAU_LIBRARY TAU_LIBRARIES )
set(TAU_DIR ${TAU_ROOT})

if(TAU_FOUND)
  add_definitions(-DSOS_HAVE_TAU)
endif()

