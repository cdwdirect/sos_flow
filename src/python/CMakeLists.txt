
# Annoyingly, CFFI only compiles with GCC.  So if SOS is configured with
# something else, we have to change the compiler settings. Here's how
# we do that: use the "-E env CC=gcc" mode for the python call.

# create a custom target out of the library compilation result
add_custom_target(ssos_python ALL ${CMAKE_COMMAND} -E env CC=gcc python ${CMAKE_CURRENT_SOURCE_DIR}/build_ssos_python.py ${CMAKE_CURRENT_SOURCE_DIR}/.. ${sos_flow_BINARY_DIR}/tmplib
    COMMAND mv ${CMAKE_CURRENT_SOURCE_DIR}/ssos_python*.so ${sos_flow_BINARY_DIR}/tmplib/ssos_python.so
    COMMAND rm -f ${CMAKE_CURRENT_SOURCE_DIR}/ssos_python.o
    DEPENDS sos
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../ssos.c
    )

INSTALL(FILES 
    ${sos_flow_BINARY_DIR}/tmplib/ssos_python.so
    DESTINATION lib)

INSTALL(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/ssos.py
    DESTINATION bin)

