if(WIN32)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(_steam_lifecycle_generated "${CMAKE_CURRENT_BINARY_DIR}/steam_lifecycle_admission_generated")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_steam_lifecycle_admission.py"
        --root "${MT2MP_SOURCE_ROOT}" --output "${_steam_lifecycle_generated}" COMMAND_ERROR_IS_FATAL ANY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${MT2MP_SOURCE_ROOT}/test/extract_steam_lifecycle_admission.py"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/runtime_capture.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/runtime_append.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/client_world.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/quest_edit_batch.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/day_rollover/economy/native_monetary.cpp")
    add_executable(test_steam_lifecycle_admission "${MT2MP_SOURCE_ROOT}/test/test_steam_lifecycle_admission.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/client_world.cpp")
    target_include_directories(test_steam_lifecycle_admission PRIVATE
        "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${_steam_lifecycle_generated}")
    target_compile_features(test_steam_lifecycle_admission PRIVATE cxx_std_20)
    target_compile_definitions(test_steam_lifecycle_admission PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(test_steam_lifecycle_admission PRIVATE /W4 /WX /utf-8 /EHsc)
    add_test(NAME steam_lifecycle_admission COMMAND test_steam_lifecycle_admission)
endif()
