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
add_executable (generic_node main.c mpi_reader.c mpi_writer.c flexpath_reader.c flexpath_writer.c mandelbrot.c) 

# Link the executable to the correct libraries
target_link_libraries (generic_node ${LIBS} ${MPI_LINK_FLAGS} ${MPI_LIBRARIES} ${ADIOS_LIBRARIES} ${ENET_LDFLAGS} ${RCA_LDFLAGS} ${ADIOS_TAU_FLAGS}) 

INSTALL(TARGETS generic_node RUNTIME DESTINATION bin)

FILE(GLOB my_pythonscripts *.py)
FILE(GLOB my_jsons *.json)
INSTALL(FILES ${my_pythonscripts} ${my_jsons} DESTINATION bin
  PERMISSIONS OWNER_EXECUTE OWNER_WRITE 
  OWNER_READ GROUP_EXECUTE GROUP_READ
  WORLD_EXECUTE WORLD_READ)

