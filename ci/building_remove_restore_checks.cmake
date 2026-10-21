if(WIN32)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(_restore_root "${MT2MP_SOURCE_ROOT}")
    set(_restore_extracted "${CMAKE_CURRENT_BINARY_DIR}/remove_restore_extracted")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${_restore_root}/src/game/typed_action_runtime.cpp"
        "${_restore_root}/src/game/typed_action/action_session.cpp"
        "${_restore_root}/src/game/remove_diagnostics.cpp"
        "${_restore_root}/src/game/building_history_slice/native_bridge.cpp"
        "${_restore_root}/test/extract_building_remove_restore.py")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${_restore_root}/test/extract_building_remove_restore.py"
        --root "${_restore_root}" --output "${_restore_extracted}" COMMAND_ERROR_IS_FATAL ANY)
    foreach(group history controller registry graph retire)
        set(target test_building_remove_restore_${group})
        add_executable(${target} "${_restore_root}/test/${target}.cpp")
        target_compile_features(${target} PRIVATE cxx_std_20)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
        target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        target_include_directories(${target} PRIVATE "${_restore_root}/src/game" "${_restore_root}/include" "${_restore_extracted}")
        add_test(NAME building_remove_restore_${group} COMMAND ${target})
    endforeach()
    foreach(group history controller graph retire)
        target_sources(test_building_remove_restore_${group} PRIVATE "${_restore_root}/src/game/multiplayer_world/world_state.cpp")
    endforeach()
    foreach(group history controller retire)
        target_sources(test_building_remove_restore_${group} PRIVATE "${_restore_root}/src/game/live_host_state/cash_contract.cpp")
    endforeach()
    target_sources(test_building_remove_restore_controller PRIVATE
        "${_restore_root}/src/game/typed_action/action_session.cpp"
        "${_restore_root}/src/game/tutorial_replication/startingpoint.cpp"
        "${_restore_root}/src/game/developer_staff/shared_state.cpp")
    target_sources(test_building_remove_restore_history PRIVATE
        "${_restore_root}/src/game/typed_action/action_session.cpp")
endif()

if(WIN32)
    add_test(NAME building_remove_restore_sdk COMMAND "${Python3_EXECUTABLE}" -X utf8 "${MT2MP_SOURCE_ROOT}/test/e2e/test_building_remove_restore.py")
    set_tests_properties(building_remove_restore_sdk PROPERTIES TIMEOUT 30)
endif()
