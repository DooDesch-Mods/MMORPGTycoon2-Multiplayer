if(WIN32)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(_online_generated "${CMAKE_CURRENT_BINARY_DIR}/subscriber_online_native_generated")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_subscriber_online_native.py"
        --root "${MT2MP_SOURCE_ROOT}" --output "${_online_generated}" COMMAND_ERROR_IS_FATAL ANY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/day_rollover/economy/native_online.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/day_rollover/economy/monetary.cpp"
        "${MT2MP_SOURCE_ROOT}/test/extract_subscriber_online_native.py")
    add_executable(test_subscriber_online_native "${MT2MP_SOURCE_ROOT}/test/test_subscriber_online_native.cpp")
    target_include_directories(test_subscriber_online_native PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${_online_generated}")
    target_compile_features(test_subscriber_online_native PRIVATE cxx_std_20)
    target_compile_definitions(test_subscriber_online_native PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(test_subscriber_online_native PRIVATE /W4 /WX /utf-8 /EHsc)
    add_test(NAME subscriber_online_native COMMAND test_subscriber_online_native)
endif()
