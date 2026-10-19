if(WIN32 AND MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(party_scope_root "${MT2MP_SOURCE_ROOT}/test/native_state_regression/party_frame_scope")
    set(party_scope_generated "${CMAKE_CURRENT_BINARY_DIR}/native_state_regression/party_frame_scope")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${party_scope_root}/check.py" "${party_scope_root}/changes.json"
        "${MT2MP_SOURCE_ROOT}/src/game/party_live_native.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/party_live.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/party_frame_native.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/intervention_actor_live.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/dungeon_instance_native.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/scene_clone/runtime.cpp"
        "${MT2MP_SOURCE_ROOT}/test/test_party_live.cpp")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -X utf8 "${party_scope_root}/check.py"
        --source "${MT2MP_SOURCE_ROOT}" --output "${party_scope_generated}"
        COMMAND_ERROR_IS_FATAL ANY)
    add_executable(test_party_frame_scope "${party_scope_root}/test_party_frame_scope.cpp")
    target_include_directories(test_party_frame_scope PRIVATE "${party_scope_generated}"
        "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(test_party_frame_scope PRIVATE cxx_std_20)
    target_compile_definitions(test_party_frame_scope PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(test_party_frame_scope PRIVATE /W4 /WX /utf-8 /EHsc)
    add_test(NAME party_frame_scope COMMAND test_party_frame_scope)
    set_tests_properties(party_frame_scope PROPERTIES TIMEOUT 30)
endif()
