# Make sure that spaces in linker lines don't cause CMake errors
if (POLICY CMP0004)
  cmake_policy(SET CMP0004 OLD)
endif()

# Make sure the compiler can find include files from MPI
include_directories (.)
include_directories("${MPI_INCLUDE_PATH}")
include_directories("${ADIOS_INCLUDE_DIRS}")

# add our executables
add_executable (synthetic_worker_a main.c worker_a.c mandelbrot.c) 
add_executable (synthetic_worker_b main.c worker_b.c) 
add_executable (synthetic_worker_c main.c worker_c.c) 

# Link the executable to the correct libraries
target_link_libraries (synthetic_worker_a ${LIBS} ${MPI_LINK_FLAGS} ${MPI_LIBRARIES} ${ADIOS_LIBRARIES}) 
target_link_libraries (synthetic_worker_b ${LIBS} ${MPI_LINK_FLAGS} ${MPI_LIBRARIES} ${ADIOS_LIBRARIES}) 
target_link_libraries (synthetic_worker_c ${LIBS} ${MPI_LINK_FLAGS} ${MPI_LIBRARIES} ${ADIOS_LIBRARIES}) 

INSTALL(TARGETS 
  synthetic_worker_a
  synthetic_worker_b
  synthetic_worker_c
  RUNTIME DESTINATION bin
)
