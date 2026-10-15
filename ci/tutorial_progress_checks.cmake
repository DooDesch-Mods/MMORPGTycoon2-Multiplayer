add_executable(test_tutorial_progress
    "${MT2MP_SOURCE_ROOT}/test/test_tutorial_progress.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/network/node_state.cpp")
target_include_directories(test_tutorial_progress PRIVATE
    "${MT2MP_SOURCE_ROOT}/src/game"
    "${MT2MP_SOURCE_ROOT}/include")
target_compile_definitions(test_tutorial_progress PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_tutorial_progress PRIVATE /W4 /WX /utf-8)
endif()
add_test(NAME tutorial_progress COMMAND test_tutorial_progress)
set_tests_properties(tutorial_progress PROPERTIES TIMEOUT 30)
