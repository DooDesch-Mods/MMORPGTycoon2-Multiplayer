if(WIN32)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${MT2MP_SOURCE_ROOT}/test/extract_toon_time_timing.py"
        "${MT2MP_SOURCE_ROOT}/src/game/toon_time_fence/native.cpp")
    set(toon_timing_generated "${CMAKE_CURRENT_BINARY_DIR}/toon_time_timing")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_toon_time_timing.py"
        --source-root "${MT2MP_SOURCE_ROOT}" --output "${toon_timing_generated}" COMMAND_ERROR_IS_FATAL ANY)
    add_executable(test_toon_time_timing "${MT2MP_SOURCE_ROOT}/test/test_toon_time_timing.cpp")
    target_include_directories(test_toon_time_timing PRIVATE "${toon_timing_generated}" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${MT2MP_SOURCE_ROOT}/test")
    target_compile_features(test_toon_time_timing PRIVATE cxx_std_20)
    target_compile_definitions(test_toon_time_timing PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(test_toon_time_timing PRIVATE /W4 /WX /utf-8 /EHsc)
    add_test(NAME test_toon_time_timing COMMAND test_toon_time_timing)
endif()
