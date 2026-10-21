if(WIN32)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(_coop_root "${MT2MP_SOURCE_ROOT}")
    set(_coop_extracted "${CMAKE_CURRENT_BINARY_DIR}/coop_session_extracted")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${_coop_root}/src/game/typed_action_runtime.cpp"
        "${_coop_root}/src/game/typed_action/action_session.cpp"
        "${_coop_root}/src/net/session_wire.cpp"
        "${_coop_root}/src/game/typed_action/sidecar_adapter.cpp"
        "${_coop_root}/test/extract_coop_session.py")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -B -X utf8 "${_coop_root}/test/extract_coop_session.py"
        --root "${_coop_root}" --output "${_coop_extracted}" COMMAND_ERROR_IS_FATAL ANY)
    foreach(group coop_cursor coop_cursor_native coop_quorum session_binding local_membership session_reload snapshot_unsent move_history move_history_controller)
        set(target test_${group})
        add_executable(${target} "${_coop_root}/test/${target}.cpp")
        target_compile_features(${target} PRIVATE cxx_std_20)
        target_compile_options(${target} PRIVATE /W4 /WX /EHsc /utf-8)
        target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        target_include_directories(${target} PRIVATE "${_coop_extracted}" "${_coop_root}/src/game"
            "${_coop_root}/src/game/typed_action" "${_coop_root}/include" "${_coop_root}/test")
        add_test(NAME ${group} COMMAND ${target})
    endforeach()
    foreach(group coop_quorum move_history_controller)
        target_sources(test_${group} PRIVATE "${_coop_root}/src/game/typed_action/action_session.cpp"
            "${_coop_root}/src/game/tutorial_replication/startingpoint.cpp" "${_coop_root}/src/game/developer_staff/shared_state.cpp")
    endforeach()
    foreach(group move_history move_history_controller)
        target_sources(test_${group} PRIVATE "${_coop_root}/src/game/multiplayer_world/world_state.cpp")
    endforeach()
    target_sources(test_move_history_controller PRIVATE "${_coop_root}/src/game/live_host_state/cash_contract.cpp")
    add_test(NAME session_reload_mcp COMMAND "${Python3_EXECUTABLE}" -B -X utf8 "${_coop_root}/test/test_session_reload_mcp.py")
endif()
