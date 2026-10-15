foreach(target test_tutorial_start_intent test_tutorial_start_intent_controller)
    add_executable(${target} "${MT2MP_SOURCE_ROOT}/test/${target}.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp")
    if(target STREQUAL "test_tutorial_start_intent_controller")
        target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp")
    endif()
    target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 60)
endforeach()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/native.cpp"
    "${MT2MP_SOURCE_ROOT}/test/extract_tutorial_hook_confirmation.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_tutorial_hook_confirmation.py"
    --source-root "${MT2MP_SOURCE_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/hook_confirmation_extracted"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_tutorial_hook_confirmation "${MT2MP_SOURCE_ROOT}/test/test_tutorial_hook_confirmation.cpp")
target_include_directories(test_tutorial_hook_confirmation PRIVATE "${MT2MP_SOURCE_ROOT}/src/game"
    "${CMAKE_CURRENT_BINARY_DIR}/hook_confirmation_extracted")
target_compile_features(test_tutorial_hook_confirmation PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_tutorial_hook_confirmation PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME tutorial_hook_confirmation COMMAND test_tutorial_hook_confirmation)
set_tests_properties(tutorial_hook_confirmation PROPERTIES TIMEOUT 60)
