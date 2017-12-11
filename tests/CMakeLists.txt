set(CMAKE_POSITION_INDEPENDENT_CODE True)
set(POSITION_INDEPENDENT_CODE True)

include_directories("${sos_flow_SOURCE_DIR}/src")

if(SQLITE3_FOUND)
    include_directories(${SQLITE3_INCLUDE_DIR})
    link_libraries(${SQLITE3_LIBRARY})
endif(SQLITE3_FOUND)

if(EVPath_FOUND)
    include_directories(${EVPath_INCLUDE_DIR})
    link_libraries(${EVPath_LIBRARIES})
endif()

if(Munge_FOUND)
    include_directories(${Munge_INCLUDE_DIR})
    link_libraries(${Munge_LIBRARIES})
endif()

if(SOS_SILENCE_DEBUG)
  add_definitions("-DSOS_DEBUG=0")
endif()

if(SOSD_DAEMON_LOGGING)
  add_definitions("-DSOSD_DAEMON_LOGGING=1")
endif()

#TARGET ---> test
add_executable(sos_test test.c pack.c buffer.c pub.c)

target_link_libraries(sos_test sos)
