if(WIN32 AND MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(domain_alias_root "${MT2MP_SOURCE_ROOT}/test/native_state_regression/native_domain_alias")
    set(domain_alias_generated "${CMAKE_CURRENT_BINARY_DIR}/native_state_regression/native_domain_alias")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${domain_alias_root}/check.py" "${domain_alias_root}/changes.json"
        "${MT2MP_SOURCE_ROOT}/test/test_dungeon_quest_destination.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/intervention_actor_runtime.cpp")
    foreach(unit intervention_census.cpp boss_current.cpp dungeon_instance_runtime.cpp release_reference_runtime.inc
            character_membership_sample.inc dungeon_scene_identity_runtime.cpp npc_quest_target_runtime.cpp
            scene_clone/quest_preview.inc dungeon_cells_graph.inc ui_build_runtime.cpp)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MT2MP_SOURCE_ROOT}/src/game/${unit}")
    endforeach()
    execute_process(COMMAND "${Python3_EXECUTABLE}" -X utf8 "${domain_alias_root}/check.py"
        --source "${MT2MP_SOURCE_ROOT}" --output "${domain_alias_generated}"
        COMMAND_ERROR_IS_FATAL ANY)
    add_executable(test_native_domain_alias "${domain_alias_root}/test_native_domain_alias.cpp")
    target_include_directories(test_native_domain_alias PRIVATE "${domain_alias_generated}"
        "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(test_native_domain_alias PRIVATE cxx_std_20)
    target_compile_definitions(test_native_domain_alias PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(test_native_domain_alias PRIVATE /W4 /WX /utf-8 /EHsc)
    add_test(NAME native_domain_alias COMMAND test_native_domain_alias)
    set_tests_properties(native_domain_alias PROPERTIES TIMEOUT 30)
endif()
