set(CMAKE_POSITION_INDEPENDENT_CODE True)
set(POSITION_INDEPENDENT_CODE True)

include_directories("${sos_flow_SOURCE_DIR}/src")
#   link_directories("${sos_flow_SOURCE_DIR}/lib")

if(EXISTS "$ENV{SQLITE}")
include_directories("$ENV{SQLITE}/include")
   link_directories("$ENV{SQLITE}/lib")
endif()

if(DEFINED EVPATH_FOUND)
    include_directories(${EVPATH_INCLUDE_DIRS})
    link_directories(${EVPATH_LIBRARY_DIRS} ${EVPATH_LIBRARY_DIR})
    link_libraries(${EVPATH_LIBRARIES})
endif()

if(SOS_SILENCE_DEBUG)
  add_definitions("-DSOS_DEBUG=0")
endif()

if(SOSD_DAEMON_LOGGING)
  add_definitions("-DSOSD_DAEMON_LOGGING=1")
endif()


#TARGET ---> test
add_executable( "sos_test"
  "test.c"
  "pack.c"
  "buffer.c"
  "pub.c"
)

target_link_libraries("sos_test" "sos")
