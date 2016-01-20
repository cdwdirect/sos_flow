# Make sure that spaces in linker lines don't cause CMake errors
if (POLICY CMP0004)
  cmake_policy(SET CMP0004 OLD)
endif()

# Make sure the compiler can find include files from MPI
include_directories (.)
include_directories("${MPI_INCLUDE_PATH}")

# add our executables
add_executable (worker_a main.c worker_a.c) 
add_executable (worker_b main.c worker_b.c) 
add_executable (worker_c main.c worker_c.c) 

# Link the executable to the Apex library. 
target_link_libraries (worker_a ${LIBS} ${MPI_LINK_FLAGS} ${MPI_LIBRARIES}) 
target_link_libraries (worker_b ${LIBS} ${MPI_LINK_FLAGS} ${MPI_LIBRARIES}) 
target_link_libraries (worker_c ${LIBS} ${MPI_LINK_FLAGS} ${MPI_LIBRARIES}) 

INSTALL(TARGETS worker_a
  RUNTIME DESTINATION bin
)
