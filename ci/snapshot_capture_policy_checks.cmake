find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(snapshot_policy_sources
    "${MT2MP_SOURCE_ROOT}/src/core/dll_main.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/building_capture_identity.cpp"
    "${MT2MP_SOURCE_ROOT}/test/extract_snapshot_capture_policy.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${snapshot_policy_sources})
execute_process(COMMAND "${Python3_EXECUTABLE}"
    "${MT2MP_SOURCE_ROOT}/test/extract_snapshot_capture_policy.py"
    --source-root "${MT2MP_SOURCE_ROOT}"
    --output "${CMAKE_CURRENT_BINARY_DIR}/snapshot_capture_policy_body.inc"
    RESULT_VARIABLE snapshot_policy_result
    OUTPUT_VARIABLE snapshot_policy_output ERROR_VARIABLE snapshot_policy_error)
if(NOT snapshot_policy_result EQUAL 0)
    message(FATAL_ERROR "Snapshot capture policy extraction failed: ${snapshot_policy_output}${snapshot_policy_error}")
endif()
add_executable(test_snapshot_capture_policy "${MT2MP_SOURCE_ROOT}/test/test_snapshot_capture_policy.cpp")
target_include_directories(test_snapshot_capture_policy PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(test_snapshot_capture_policy PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_snapshot_capture_policy PRIVATE /W4 /WX /utf-8)
endif()
add_test(NAME snapshot_capture_policy COMMAND test_snapshot_capture_policy)
set_tests_properties(snapshot_capture_policy PROPERTIES TIMEOUT 30)
