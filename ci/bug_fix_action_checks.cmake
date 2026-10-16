if(WIN32)
    add_executable(test_bug_fix_action "${MT2MP_SOURCE_ROOT}/test/test_bug_fix_action.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp")
    target_include_directories(test_bug_fix_action PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(test_bug_fix_action PRIVATE cxx_std_20)
    target_compile_options(test_bug_fix_action PRIVATE /W4 /WX /utf-8 /EHsc)
    add_test(NAME test_bug_fix_action COMMAND test_bug_fix_action)
endif()
