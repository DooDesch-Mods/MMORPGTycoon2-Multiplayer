add_executable(test_tutorial_client_commit_ui "${MT2MP_SOURCE_ROOT}/test/test_tutorial_client_commit_ui.cpp")
target_include_directories(test_tutorial_client_commit_ui PRIVATE "${MT2MP_SOURCE_ROOT}/src/game"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_startingpoint_fixture" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_tutorial_client_commit_ui PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_tutorial_client_commit_ui PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME tutorial_client_commit_ui COMMAND test_tutorial_client_commit_ui)
set_tests_properties(tutorial_client_commit_ui PROPERTIES TIMEOUT 60)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action_runtime.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_startingpoint_fixture/startingpoint_fixture.cpp")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_tutorial_pending_binding.py"
    --source-root "${MT2MP_SOURCE_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/pending_binding_extracted"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_tutorial_pending_binding "${MT2MP_SOURCE_ROOT}/test/test_tutorial_pending_binding.cpp")
target_include_directories(test_tutorial_pending_binding PRIVATE "${MT2MP_SOURCE_ROOT}/src/game"
    "${MT2MP_SOURCE_ROOT}/include" "${CMAKE_CURRENT_BINARY_DIR}/pending_binding_extracted")
target_compile_features(test_tutorial_pending_binding PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_tutorial_pending_binding PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME tutorial_pending_binding COMMAND test_tutorial_pending_binding)
set_tests_properties(tutorial_pending_binding PROPERTIES TIMEOUT 60)
