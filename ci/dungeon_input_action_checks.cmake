foreach(name dungeon_input_action dungeon_input_capture dungeon_input_authored dungeon_input_protocol dungeon_input_controller)
    add_executable(test_${name} "${MT2MP_SOURCE_ROOT}/test/test_${name}.cpp")
    if(name STREQUAL "dungeon_input_protocol" OR name STREQUAL "dungeon_input_controller")
        target_sources(test_${name} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
    endif()
    target_include_directories(test_${name} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(test_${name} PRIVATE cxx_std_20)
    target_compile_definitions(test_${name} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(test_${name} PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME ${name} COMMAND test_${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endforeach()

target_sources(test_dungeon_input_action PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/scene_clone/state.cpp")
