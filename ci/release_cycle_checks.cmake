add_executable(test_release_cycle
    "${MT2MP_SOURCE_ROOT}/test/test_release_cycle.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/network/node_state.cpp")
target_include_directories(test_release_cycle PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_release_cycle PRIVATE cxx_std_20)
target_compile_definitions(test_release_cycle PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
 target_compile_options(test_release_cycle PRIVATE /utf-8 /EHsc)
endif()
add_test(NAME release_cycle COMMAND test_release_cycle)
