find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/bridge.cpp"
    "${MT2MP_SOURCE_ROOT}/test/extract_bug_frame.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_bug_frame.py"
    --source-root "${MT2MP_SOURCE_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/bug_frame_extracted"
    COMMAND_ERROR_IS_FATAL ANY)
set(bug_frame_sources
    tutorial_replication/state.cpp tutorial_replication/startingpoint.cpp
    developer_staff/shared_state.cpp
    live_host_state/state.cpp live_host_state/network/node_state.cpp
    live_host_state/day_rollover/day.cpp live_host_state/day_rollover/economy/codec.cpp
    live_host_state/day_rollover/economy/monetary.cpp subscriber_lifecycle/additions.cpp
    subscriber_lifecycle/payload.cpp subscriber_lifecycle/record_decode.cpp subscriber_lifecycle/record_graph.cpp
    subscriber_lifecycle/reference_map.cpp)
list(TRANSFORM bug_frame_sources PREPEND "${MT2MP_SOURCE_ROOT}/src/game/")
foreach(group codec frame boundary)
    add_executable(test_bug_authority_${group} "${MT2MP_SOURCE_ROOT}/test/test_bug_authority_${group}.cpp")
    target_include_directories(test_bug_authority_${group} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game"
        "${MT2MP_SOURCE_ROOT}/include" "${CMAKE_CURRENT_BINARY_DIR}/bug_frame_extracted")
    target_compile_features(test_bug_authority_${group} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(test_bug_authority_${group} PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME bug_authority_${group} COMMAND test_bug_authority_${group})
    set_tests_properties(bug_authority_${group} PROPERTIES TIMEOUT 60)
endforeach()
target_sources(test_bug_authority_frame PRIVATE ${bug_frame_sources})
if(MSVC)
    target_compile_options(test_bug_authority_frame PRIVATE /Gy)
    target_link_options(test_bug_authority_frame PRIVATE /OPT:REF /INCREMENTAL:NO)
endif()
