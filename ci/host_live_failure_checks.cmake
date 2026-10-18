find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(_host_failure_generated "${CMAKE_CURRENT_BINARY_DIR}/host_live_failure_generated")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/net/snapshot_sync.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action_runtime.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/native_lobby.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${MT2MP_SOURCE_ROOT}/test/extract_host_live_failure.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_host_live_failure.py"
    --source-root "${MT2MP_SOURCE_ROOT}" --output "${_host_failure_generated}" COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_host_live_failure "${MT2MP_SOURCE_ROOT}/test/test_host_live_failure.cpp")
target_include_directories(test_host_live_failure PRIVATE "${_host_failure_generated}"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_host_live_failure PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_host_live_failure PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME host_live_failure COMMAND test_host_live_failure)
set_tests_properties(host_live_failure PROPERTIES TIMEOUT 60)
