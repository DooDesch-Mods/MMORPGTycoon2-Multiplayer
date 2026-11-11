add_executable(test_tutorial_commit_intent "${MT2MP_SOURCE_ROOT}/test/test_tutorial_commit_intent.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp")
target_include_directories(test_tutorial_commit_intent PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_tutorial_commit_intent PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_tutorial_commit_intent PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME tutorial_commit_intent COMMAND test_tutorial_commit_intent)
set_tests_properties(tutorial_commit_intent PROPERTIES TIMEOUT 60)
