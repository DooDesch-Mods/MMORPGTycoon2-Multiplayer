add_executable(test_research_action "${MT2MP_SOURCE_ROOT}/test/test_research_action.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/network/node_state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/day_rollover/day.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/day_rollover/economy/codec.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/day_rollover/economy/monetary.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/additions.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/payload.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/record_decode.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/record_graph.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/reference_map.cpp")
target_include_directories(test_research_action PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_research_action PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_research_action PRIVATE /W4 /WX /utf-8 /EHsc /Gy)
    target_link_options(test_research_action PRIVATE /OPT:REF /INCREMENTAL:NO)
endif()
add_test(NAME research_action COMMAND test_research_action)
set_tests_properties(research_action PROPERTIES TIMEOUT 60)
