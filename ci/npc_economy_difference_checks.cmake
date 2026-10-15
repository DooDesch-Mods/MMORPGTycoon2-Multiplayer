set(npc_difference_sources
    live_host_state/day_rollover/economy/monetary.cpp live_host_state/day_rollover/day.cpp
    live_host_state/state.cpp live_host_state/network/node_state.cpp
    tutorial_replication/state.cpp tutorial_replication/startingpoint.cpp
    subscriber_lifecycle/additions.cpp subscriber_lifecycle/payload.cpp
    subscriber_lifecycle/record_decode.cpp subscriber_lifecycle/record_graph.cpp subscriber_lifecycle/reference_map.cpp)
list(TRANSFORM npc_difference_sources PREPEND "${MT2MP_SOURCE_ROOT}/src/game/")
add_executable(test_npc_economy_difference "${MT2MP_SOURCE_ROOT}/test/test_npc_economy_difference.cpp" ${npc_difference_sources})
target_include_directories(test_npc_economy_difference PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_npc_economy_difference PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_npc_economy_difference PRIVATE /W4 /WX /utf-8 /EHsc /Gy)
    target_link_options(test_npc_economy_difference PRIVATE /OPT:REF /INCREMENTAL:NO)
endif()
add_test(NAME test_npc_economy_difference COMMAND test_npc_economy_difference)
set_tests_properties(test_npc_economy_difference PROPERTIES TIMEOUT 60)
