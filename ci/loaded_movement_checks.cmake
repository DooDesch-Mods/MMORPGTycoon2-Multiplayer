find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(_loaded_movement_generated "${CMAKE_CURRENT_BINARY_DIR}/loaded_movement_generated")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_loaded_movement.py"
    --root "${MT2MP_SOURCE_ROOT}" --output "${_loaded_movement_generated}"
    COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/test/extract_loaded_movement.py"
    "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/construction_native.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action_runtime.cpp")
add_executable(test_loaded_movement "${MT2MP_SOURCE_ROOT}/test/test_loaded_movement.cpp")
target_include_directories(test_loaded_movement PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${_loaded_movement_generated}")
target_compile_features(test_loaded_movement PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_loaded_movement PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME loaded_movement COMMAND test_loaded_movement)
set_tests_properties(loaded_movement PROPERTIES TIMEOUT 30)
