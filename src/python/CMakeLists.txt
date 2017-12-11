
# create a custom target out of the library compilation result
add_custom_target(ssos_python ALL python ${CMAKE_CURRENT_SOURCE_DIR}/build_ssos_python.py ${CMAKE_CURRENT_SOURCE_DIR}/.. ${sos_flow_BINARY_DIR}/tmplib
    COMMAND mv ${CMAKE_CURRENT_SOURCE_DIR}/ssos_python.so ${sos_flow_BINARY_DIR}/tmplib/.
    COMMAND rm -f ${CMAKE_CURRENT_SOURCE_DIR}/ssos_python.o
    DEPENDS ssos sosa
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../ssos.c
    )

INSTALL(FILES 
    ${sos_flow_BINARY_DIR}/tmplib/ssos_python.so
    DESTINATION lib)

