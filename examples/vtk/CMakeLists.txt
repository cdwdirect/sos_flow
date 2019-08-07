cmake_minimum_required(VERSION 2.8.12.1)

PROJECT(readvtk)
SET(VTK_DIR /g/g17/wood67/src/vtk6/build)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package(VTK REQUIRED)
include(${VTK_USE_FILE})

#FIND_PACKAGE( OpenMP REQUIRED)
#if(OPENMP_FOUND)
#    message("OPENMP FOUND")
#    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
#    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
#    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
#endif()

add_executable(readvtk readvtk)
set_target_properties(readvtk PROPERTIES LINKER_LANGUAGE CXX)


if(VTK_LIBRARIES)
  target_link_libraries(readvtk ${VTK_LIBRARIES})
else()
  target_link_libraries(readvtk vtkHybrid)
endif()

