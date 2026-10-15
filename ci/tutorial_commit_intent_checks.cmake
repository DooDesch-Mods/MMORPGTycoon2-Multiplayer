find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(commit_source "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint_commit_intent.inc")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${commit_source}")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_tutorial_commit_intent.py"
    --source-root "${MT2MP_SOURCE_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/commit_intent_extracted"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_tutorial_commit_intent "${MT2MP_SOURCE_ROOT}/test/test_tutorial_commit_intent.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp")
add_executable(test_tutorial_native_commit_intent "${MT2MP_SOURCE_ROOT}/test/test_tutorial_native_commit_intent.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp")
foreach(target test_tutorial_commit_intent test_tutorial_native_commit_intent)
    target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${CMAKE_CURRENT_BINARY_DIR}/commit_intent_extracted")
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
endforeach()
add_test(NAME tutorial_commit_intent COMMAND test_tutorial_commit_intent)
add_test(NAME tutorial_native_commit_intent COMMAND test_tutorial_native_commit_intent)
set_tests_properties(tutorial_commit_intent tutorial_native_commit_intent PROPERTIES TIMEOUT 60)
