file(GLOB TEST_SOURCES
    test_*.cpp
)

add_executable(m0rtis_tests
    ${TEST_SOURCES}
)

target_include_directories(m0rtis_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/m0rtis_hub/include
        ${CMAKE_SOURCE_DIR}/m0rtis_node/include
)

target_link_libraries(m0rtis_tests
    PRIVATE
        m0rtis_proto
)

target_compile_definitions(m0rtis_tests
    PRIVATE
        TEST_FILES_DIR="${CMAKE_SOURCE_DIR}/test_files"
        FIXTURE_INFERENCE_EVENTS_PATH="${CMAKE_SOURCE_DIR}/test_files/inference_events.json"
)

target_compile_options(m0rtis_tests
    PRIVATE
        -Wall
        -Wextra
        -Werror
        -pedantic
)

target_compile_features(m0rtis_tests PRIVATE cxx_std_20)

foreach(suite version envelope message_type event_type framing connection_state fixture fixture_reader)
    add_test(NAME ${suite} COMMAND m0rtis_tests ${suite})
endforeach()
