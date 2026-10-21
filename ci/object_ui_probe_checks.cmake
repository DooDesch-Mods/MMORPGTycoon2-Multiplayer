if(WIN32 AND MSVC)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(object_ui_generated "${CMAKE_CURRENT_BINARY_DIR}/object_ui_probe")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${MT2MP_SOURCE_ROOT}/test/extract_object_ui_probe.py"
        "${MT2MP_SOURCE_ROOT}/src/game/object_ui_probe/runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/typed_action_runtime.cpp")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_object_ui_probe.py"
        --root "${MT2MP_SOURCE_ROOT}" --output "${object_ui_generated}" COMMAND_ERROR_IS_FATAL ANY)
    add_executable(test_object_ui_probe "${MT2MP_SOURCE_ROOT}/test/test_object_ui_probe.cpp")
    target_compile_features(test_object_ui_probe PRIVATE cxx_std_20)
    target_include_directories(test_object_ui_probe PRIVATE "${object_ui_generated}" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_options(test_object_ui_probe PRIVATE /W4 /WX /EHsc /utf-8)
    add_test(NAME object_ui_probe COMMAND test_object_ui_probe)
endif()
