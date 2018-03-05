###############################################################################
# Copyright (c) 2015-2017, Lawrence Livermore National Security, LLC.
# 
# Produced at the Lawrence Livermore National Laboratory
# 
# LLNL-CODE-716457
# 
# All rights reserved.
# 
# This file is part of Alpine. 
# 
# For details, see: http://software.llnl.gov/alpine/.
# 
# Please also read alpine/LICENSE
# 
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions are met:
# 
# * Redistributions of source code must retain the above copyright notice, 
#   this list of conditions and the disclaimer below.
# 
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the disclaimer (as noted below) in the
#   documentation and/or other materials provided with the distribution.
# 
# * Neither the name of the LLNS/LLNL nor the names of its contributors may
#   be used to endorse or promote products derived from this software without
#   specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL LAWRENCE LIVERMORE NATIONAL SECURITY,
# LLC, THE U.S. DEPARTMENT OF ENERGY OR CONTRIBUTORS BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
# DAMAGES  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
# POSSIBILITY OF SUCH DAMAGE.
# 
###############################################################################

###############################################################################

if(DEFINED MPI_INCLUDE_PATH)
	include_directories("${MPI_INCLUDE_PATH}")
endif()

set(LULESH_SOURCES
    lulesh.cc 
    lulesh-comm.cc 
    lulesh-viz.cc 
    lulesh-util.cc 
    lulesh-init.cc)

include_directories("${sos_flow_SOURCE_DIR}/src")

if(OPENMP_FOUND)
   set(lulesh_openmp_flags "-DLULESH_USE_OPENMP")
else()
   set(lulesh_openmp_flags "")
endif()

if(DEFINED EVPATH_FOUND)
    include_directories(${EVPATH_INCLUDE_DIRS})
    link_directories(${EVPATH_LIBRARY_DIRS})
    link_libraries(${EVPATH_LIBRARIES})
    link_directories(${FABRIC_LIBRARY_DIRS})
    link_libraries(${FABRIC_LIBRARIES})
endif()

if(MPI_FOUND)
    add_executable(lulesh_par ${LULESH_SOURCES})

    set_target_properties(lulesh_par PROPERTIES COMPILE_FLAGS "${MPI_CXX_COMPILE_FLAGS} -DUSE_MPI=1  ${lulesh_openmp_flags}")

    set_target_properties(lulesh_par PROPERTIES COMPILE_FLAGS "${MPI_CXX_LINK_FLAGS}")

    target_link_libraries(lulesh_par
                          sos
                          ${MPI_CXX_LIBRARIES})
endif()






