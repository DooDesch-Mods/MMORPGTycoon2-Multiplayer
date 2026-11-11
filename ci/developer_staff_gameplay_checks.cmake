if(WIN32)
    foreach(group pool controller frame ui frame_apply)
        add_executable(test_developer_${group} "${MT2MP_SOURCE_ROOT}/test/test_developer_${group}.cpp")
        target_include_directories(test_developer_${group} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game"
            "${MT2MP_SOURCE_ROOT}/src/game/live_host_state" "${MT2MP_SOURCE_ROOT}/include")
        target_compile_features(test_developer_${group} PRIVATE cxx_std_20)
        if(MSVC)
            target_compile_options(test_developer_${group} PRIVATE /W4 /WX /utf-8 /EHsc)
        endif()
        add_test(NAME test_developer_${group} COMMAND test_developer_${group})
        set_tests_properties(test_developer_${group} PROPERTIES TIMEOUT 60)
    endforeach()
    foreach(group pool controller frame frame_apply)
        target_sources(test_developer_${group} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
    endforeach()
    target_sources(test_developer_controller PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp")
    target_compile_definitions(test_developer_controller PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        # Match the supported game's PE stack reserve for the real Controller path.
        target_link_options(test_developer_controller PRIVATE /STACK:2097152)
    endif()
    target_link_libraries(test_developer_ui PRIVATE bcrypt)
    set(developer_frame_sources tutorial_replication/state.cpp tutorial_replication/startingpoint.cpp
        live_host_state/state.cpp live_host_state/network/node_state.cpp live_host_state/day_rollover/day.cpp
        live_host_state/day_rollover/economy/codec.cpp live_host_state/day_rollover/economy/monetary.cpp
        subscriber_lifecycle/additions.cpp subscriber_lifecycle/payload.cpp subscriber_lifecycle/record_decode.cpp
        subscriber_lifecycle/record_graph.cpp subscriber_lifecycle/reference_map.cpp)
    list(TRANSFORM developer_frame_sources PREPEND "${MT2MP_SOURCE_ROOT}/src/game/")
    target_sources(test_developer_frame PRIVATE ${developer_frame_sources})
    if(MSVC)
        target_compile_options(test_developer_frame PRIVATE /Gy)
        target_link_options(test_developer_frame PRIVATE /OPT:REF /INCREMENTAL:NO)
    endif()
    # Both actual codecs now call the shared Developer pool codec.
    get_property(developer_link_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS developer_link_targets)
        get_target_property(sources ${target} SOURCES)
        if(sources MATCHES "/src/game/(live_host_state/state|typed_action/action_session)[.]cpp"
            AND NOT sources MATCHES "/src/game/developer_staff/shared_state[.]cpp")
            target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
        endif()
    endforeach()
endif()
