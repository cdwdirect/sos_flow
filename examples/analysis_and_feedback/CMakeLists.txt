
include_directories(.)
include_directories(../../src)

add_executable(client   client.c   client.h   protocol.c protocol.h)
add_executable(analysis analysis.c analysis.h protocol.c protocol.h)

target_link_libraries(client   sos)
target_link_libraries(analysis sos)

install(
    TARGETS
        client
        analysis
    RUNTIME DESTINATION bin/examples/analysis_and_feedback
    ARCHIVE DESTINATION lib 
    LIBRARY DESTINATION lib)

install(
    FILES
        launch_example.sh
        DESTINATION bin/examples/analysis_and_feedback
        PERMISSIONS OWNER_READ OWNER_EXECUTE OWNER_WRITE
                    GROUP_READ GROUP_EXECUTE
                    WORLD_READ WORLD_EXECUTE)

