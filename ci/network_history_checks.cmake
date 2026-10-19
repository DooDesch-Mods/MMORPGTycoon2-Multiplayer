add_executable(test_network_history
    "${MT2MP_SOURCE_ROOT}/test/test_network_history.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/state.cpp")
target_link_libraries(test_network_history PRIVATE world_edit_session_checks)
target_include_directories(test_network_history PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_network_history PRIVATE cxx_std_20)
target_compile_definitions(test_network_history PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_network_history PRIVATE /utf-8 /EHsc /W4 /WX)
endif()
add_test(NAME network_history COMMAND test_network_history)
set_tests_properties(network_history PROPERTIES TIMEOUT 60)
