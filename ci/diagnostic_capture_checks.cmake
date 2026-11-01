find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action_runtime.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action_runtime.h"
    "${MT2MP_SOURCE_ROOT}/src/core/dll_main.cpp")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_diagnostic_capture.py"
    --source-root "${MT2MP_SOURCE_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/diagnostic_capture_extracted"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_diagnostic_capture "${MT2MP_SOURCE_ROOT}/test/test_diagnostic_capture.cpp")
target_include_directories(test_diagnostic_capture PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/diagnostic_capture_extracted" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_diagnostic_capture PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_diagnostic_capture PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME diagnostic_capture COMMAND test_diagnostic_capture)
set_tests_properties(diagnostic_capture PROPERTIES TIMEOUT 30)
