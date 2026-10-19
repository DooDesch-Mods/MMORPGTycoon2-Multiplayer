if(WIN32 AND MSVC)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(live_start_root "${MT2MP_SOURCE_ROOT}/test/live_start_publication")
    set(live_start_generated "${CMAKE_CURRENT_BINARY_DIR}/live_start_publication")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${MT2MP_SOURCE_ROOT}/src/game/typed_action_runtime.cpp"
        "${live_start_root}/check.py"
        "${live_start_root}/test_live_start_publication.cpp")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -X utf8
        "${live_start_root}/check.py" --source "${MT2MP_SOURCE_ROOT}"
        --output "${live_start_generated}"
        COMMAND_ERROR_IS_FATAL ANY)
    add_executable(test_live_start_publication
        "${live_start_root}/test_live_start_publication.cpp")
    target_include_directories(test_live_start_publication PRIVATE "${live_start_generated}")
    target_compile_features(test_live_start_publication PRIVATE cxx_std_20)
    target_compile_options(test_live_start_publication PRIVATE /W4 /WX /utf-8 /EHsc)
    add_test(NAME live_start_publication COMMAND test_live_start_publication)
    set_tests_properties(live_start_publication PROPERTIES TIMEOUT 30)
endif()
