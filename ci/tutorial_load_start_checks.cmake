find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/native.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/lifecycle_owner.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/lifecycle_hook.inc")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_tutorial_load_start.py"
    --source-root "${MT2MP_SOURCE_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/tutorial_load_start_extracted"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_tutorial_load_start "${MT2MP_SOURCE_ROOT}/test/test_tutorial_load_start.cpp")
target_include_directories(test_tutorial_load_start PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${CMAKE_CURRENT_BINARY_DIR}/tutorial_load_start_extracted")
target_compile_features(test_tutorial_load_start PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_tutorial_load_start PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME test_tutorial_load_start COMMAND test_tutorial_load_start)
set_tests_properties(test_tutorial_load_start PROPERTIES TIMEOUT 60)
