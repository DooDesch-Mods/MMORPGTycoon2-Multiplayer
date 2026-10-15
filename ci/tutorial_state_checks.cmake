if(NOT DEFINED TUTORIAL_STATE_TEST_SOURCE)
    set(TUTORIAL_STATE_TEST_SOURCE "${MT2MP_SOURCE_ROOT}/test/test_tutorial_state.cpp")
endif()
add_executable(test_tutorial_state
    "${TUTORIAL_STATE_TEST_SOURCE}"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/network/node_state.cpp")
target_include_directories(test_tutorial_state PRIVATE
    "${MT2MP_SOURCE_ROOT}/src/game"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state"
    "${MT2MP_SOURCE_ROOT}/include")
if(MSVC)
    target_compile_options(test_tutorial_state PRIVATE /W4 /WX /utf-8)
endif()
add_test(NAME tutorial_state COMMAND test_tutorial_state)
set_tests_properties(tutorial_state PROPERTIES TIMEOUT 60)
