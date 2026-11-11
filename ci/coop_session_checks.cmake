if(WIN32)
    foreach(group coop_cursor coop_cursor_native coop_quorum session_binding session_reload snapshot_unsent)
        set(target test_${group})
        add_executable(${target} "${MT2MP_SOURCE_ROOT}/test/${target}.cpp")
        target_compile_features(${target} PRIVATE cxx_std_20)
        target_compile_options(${target} PRIVATE /W4 /WX /EHsc /utf-8)
        target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game"
            "${MT2MP_SOURCE_ROOT}/src/game/typed_action" "${MT2MP_SOURCE_ROOT}/include" "${MT2MP_SOURCE_ROOT}/test")
        add_test(NAME ${group} COMMAND ${target})
    endforeach()
    target_sources(test_coop_quorum PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp" "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
endif()
