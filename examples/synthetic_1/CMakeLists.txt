# Make sure that spaces in linker lines don't cause CMake errors
if (POLICY CMP0004)
  cmake_policy(SET CMP0004 OLD)
endif()

# Make sure the compiler can find include files from MPI
include_directories (.)
include_directories("${MPI_INCLUDE_PATH}")


# Add executable called "mpi_test" that is built from the source file
# "mpi_test.cpp". The extensions are automatically found. 
add_executable (worker_a main.c worker_a.c) 

# Link the executable to the Apex library. 
target_link_libraries (worker_a ${LIBS} ${MPI_LINK_FLAGS} ${MPI_LIBRARIES}) 

INSTALL(TARGETS worker_a
  RUNTIME DESTINATION bin
)
