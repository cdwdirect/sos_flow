set(CMAKE_POSITION_INDEPENDENT_CODE True)
set(POSITION_INDEPENDENT_CODE True)

include_directories("${sos_flow_SOURCE_DIR}/src")

if(SQLite3_FOUND)
    include_directories(${SQLite3_INCLUDE_DIR})
    link_libraries(${SQLite3_LIBRARY})
endif(SQLite3_FOUND)

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

#TARGET ---> kevin_test
add_executable(kevin_test kevin_test.c)
target_link_libraries(kevin_test sos)

INSTALL(TARGETS
    sos_test kevin_test
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib)

