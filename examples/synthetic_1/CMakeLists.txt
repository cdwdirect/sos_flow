# Make sure that spaces in linker lines don't cause CMake errors
if (POLICY CMP0004)
  cmake_policy(SET CMP0004 OLD)
endif()

# TAU is NOT required.
if (DEFINED TAU_ROOT AND USE_TAU)
    find_package(TAU)
    if (TAU_FOUND)
        include_directories(${TAU_INCLUDE_DIRS})
        set(LIBS ${LIBS} ${TAU_LIBRARIES})
        set (CMAKE_INSTALL_RPATH ${CMAKE_INSTALL_RPATH} ${TAU_LIBRARY_DIR})
    endif()
endif()

# Make sure the compiler can find include files from MPI
include_directories (.)
include_directories("${MPI_INCLUDE_PATH}")
include_directories("${ADIOS_INCLUDE_DIRS}")
include_directories("${sos_flow_SOURCE_DIR}/src")

# add our executables
add_executable (synthetic_worker_a main.c worker_a.c mandelbrot.c) 
add_executable (synthetic_worker_b main.c worker_b.c mandelbrot.c) 
add_executable (synthetic_worker_c main.c worker_c.c mandelbrot.c) 

# NOTE: This is commented out because FindADIOS.cmake it likely handling all of it now.
#
# Somewhat wonky, hopefully portable ADIOS stuff...
#  ...should grab the right includes and libs that could be needed
#     based on how ADIOS itself was configured.
# exec_program("adios_config -c" OUTPUT_VARIABLE ADIOS_COMPONENT_CFLAGS)
# exec_program("adios_config -l" OUTPUT_VARIABLE ADIOS_COMPONENT_LDFLAGS)

# Link the executable to the correct libraries
link_directories("${sos_flow_SOURCE_DIR}/lib")
set (SYNTHETIC_LINK_OPTIONS ${LIBS} ${MPI_LINK_FLAGS} ${MPI_LIBRARIES} ${ADIOS_LIBRARIES} ${ENET_LDFLAGS} ${RCA_LDFLAGS})
target_link_libraries (synthetic_worker_a ${SYNTHETIC_LINK_OPTIONS}) 
target_link_libraries (synthetic_worker_b ${SYNTHETIC_LINK_OPTIONS})
target_link_libraries (synthetic_worker_c ${SYNTHETIC_LINK_OPTIONS})

INSTALL(TARGETS 
  synthetic_worker_a
  synthetic_worker_b
  synthetic_worker_c
  RUNTIME DESTINATION bin
)
