set(completion_state_sources "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp")
add_executable(test_tutorial_completion "${MT2MP_SOURCE_ROOT}/test/test_tutorial_completion.cpp" ${completion_state_sources})
add_executable(test_tutorial_completion_owned "${MT2MP_SOURCE_ROOT}/test/test_tutorial_completion_owned.cpp")
set(completion_frame_sources
    tutorial_replication/state.cpp tutorial_replication/startingpoint.cpp
    live_host_state/state.cpp live_host_state/network/node_state.cpp
    live_host_state/day_rollover/day.cpp live_host_state/day_rollover/economy/codec.cpp
    live_host_state/day_rollover/economy/monetary.cpp subscriber_lifecycle/additions.cpp
    subscriber_lifecycle/payload.cpp subscriber_lifecycle/record_decode.cpp subscriber_lifecycle/record_graph.cpp
    subscriber_lifecycle/reference_map.cpp)
list(TRANSFORM completion_frame_sources PREPEND "${MT2MP_SOURCE_ROOT}/src/game/")
add_executable(test_tutorial_completion_frame "${MT2MP_SOURCE_ROOT}/test/test_tutorial_completion_frame.cpp" ${completion_frame_sources})
foreach(target test_tutorial_completion test_tutorial_completion_owned test_tutorial_completion_frame)
    target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 60)
endforeach()
if(MSVC)
    target_compile_options(test_tutorial_completion_frame PRIVATE /Gy)
    target_link_options(test_tutorial_completion_frame PRIVATE /OPT:REF /INCREMENTAL:NO)
endif()
