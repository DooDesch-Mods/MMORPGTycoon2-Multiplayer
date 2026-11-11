add_executable(test_saved_direct_monster_identity "${MT2MP_SOURCE_ROOT}/test/test_saved_direct_monster_identity.cpp")
target_include_directories(test_saved_direct_monster_identity PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_saved_direct_monster_identity PRIVATE cxx_std_20)
target_compile_definitions(test_saved_direct_monster_identity PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(test_saved_direct_monster_identity PRIVATE /W4 /WX /utf-8 /EHsc)
add_test(NAME saved_direct_monster_identity COMMAND test_saved_direct_monster_identity)
set_tests_properties(saved_direct_monster_identity PROPERTIES TIMEOUT 60)
add_executable(test_saved_monster_protocol "${MT2MP_SOURCE_ROOT}/test/test_saved_monster_protocol.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
target_include_directories(test_saved_monster_protocol PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_saved_monster_protocol PRIVATE cxx_std_20)
target_compile_options(test_saved_monster_protocol PRIVATE /W4 /WX /utf-8 /EHsc)
add_test(NAME saved_monster_protocol COMMAND test_saved_monster_protocol)
set_tests_properties(saved_monster_protocol PROPERTIES TIMEOUT 60)
