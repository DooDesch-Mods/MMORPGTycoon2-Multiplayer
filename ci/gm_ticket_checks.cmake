add_executable(test_gm_ticket "${MT2MP_SOURCE_ROOT}/test/test_gm_ticket.cpp")
target_include_directories(test_gm_ticket PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_gm_ticket PRIVATE cxx_std_20)
target_compile_definitions(test_gm_ticket PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_gm_ticket PRIVATE /utf-8 /EHsc /W4 /WX)
endif()
add_test(NAME gm_ticket COMMAND test_gm_ticket)
add_executable(test_gm_frame
    "${MT2MP_SOURCE_ROOT}/test/test_gm_frame.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/network/node_state.cpp")
target_include_directories(test_gm_frame PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_gm_frame PRIVATE cxx_std_20)
target_compile_definitions(test_gm_frame PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_gm_frame PRIVATE /utf-8 /EHsc)
endif()
add_test(NAME gm_frame COMMAND test_gm_frame)
