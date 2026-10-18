set(vip_frame_sources
    tutorial_replication/state.cpp tutorial_replication/startingpoint.cpp developer_staff/shared_state.cpp
    live_host_state/state.cpp live_host_state/network/node_state.cpp live_host_state/day_rollover/day.cpp
    live_host_state/day_rollover/economy/codec.cpp live_host_state/day_rollover/economy/monetary.cpp
    subscriber_lifecycle/additions.cpp subscriber_lifecycle/payload.cpp subscriber_lifecycle/record_decode.cpp
    subscriber_lifecycle/record_graph.cpp subscriber_lifecycle/reference_map.cpp)
list(TRANSFORM vip_frame_sources PREPEND "${MT2MP_SOURCE_ROOT}/src/game/")
add_executable(test_vip_frame_retention "${MT2MP_SOURCE_ROOT}/test/test_vip_frame_retention.cpp" ${vip_frame_sources})
target_include_directories(test_vip_frame_retention PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_vip_frame_retention PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_vip_frame_retention PRIVATE /W4 /WX /utf-8 /EHsc /Gy)
    target_link_options(test_vip_frame_retention PRIVATE /OPT:REF /INCREMENTAL:NO)
endif()
add_test(NAME vip_frame_retention COMMAND test_vip_frame_retention)
set_tests_properties(vip_frame_retention PROPERTIES TIMEOUT 60)

include("${CMAKE_CURRENT_LIST_DIR}/vip_frame_wiring_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/vip_alert_client_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/vip_disconnect_checks.cmake")
