find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(WORLD_EDIT_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/world_edit_generated")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/test/extract_world_edit_bodies.py"
    "${MT2MP_SOURCE_ROOT}/src/core/dll_main.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/boss_phase_property_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/boss_phase_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/district_text_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/district_title_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/duplicate_history_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/duplicate_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/group_history_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/group_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/move_kiss/move_kiss.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/npc_property_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/prop_text_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/region_appearance_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/region_sale_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/selection_move_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/service_membership_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action_runtime.cpp"
 )
execute_process(COMMAND "${Python3_EXECUTABLE}" -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_world_edit_bodies.py"
    --source "${MT2MP_SOURCE_ROOT}" --output "${WORLD_EDIT_GENERATED}"
    RESULT_VARIABLE WORLD_EDIT_EXTRACT_RESULT OUTPUT_VARIABLE WORLD_EDIT_EXTRACT_STDOUT ERROR_VARIABLE WORLD_EDIT_EXTRACT_STDERR)
if(NOT WORLD_EDIT_EXTRACT_RESULT EQUAL 0)
    message(FATAL_ERROR "World edit actual body extraction failed: ${WORLD_EDIT_EXTRACT_STDOUT}${WORLD_EDIT_EXTRACT_STDERR}")
endif()
add_library(world_edit_session_checks STATIC
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
target_include_directories(world_edit_session_checks PRIVATE "${MT2MP_SOURCE_ROOT}/include" "${MT2MP_SOURCE_ROOT}/src/game")
target_compile_features(world_edit_session_checks PRIVATE cxx_std_20)
target_compile_definitions(world_edit_session_checks PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(world_edit_session_checks PRIVATE /W4 /WX /utf-8 /EHsc)
foreach(target
    test_world_path_protocol
    test_mixed_path_graph
    test_mixed_path_reader
    test_wall_private_lifecycle
    test_region_appearance_protocol
    test_region_appearance_capture
    test_npc_property_protocol
    test_npc_property_capture
    test_region_sale_protocol
    test_region_sale_reader
    test_region_sale_inverse
    test_prop_text_protocol
    test_prop_text_native
    test_boss_phase_protocol
    test_boss_phase_reader
    test_boss_phase_native
    test_service_membership_protocol
    test_service_membership_reader
    test_service_membership_native
    test_selection_move_protocol
    test_selection_move_reader
    test_selection_move_native
    test_selection_move_binding
    test_selection_history_runtime
    test_region_history_protocol
    test_quest_history_protocol
    test_quest_history_read
    test_terrain_history_protocol
    test_scenery_make_history
    test_path_history_graph
    test_path_history_protocol
    test_path_history_runtime
    test_region_history_runtime
    test_district_saved_identity
    test_district_protocol
    test_district_native
    test_district_property_wrappers
    test_district_title_protocol
    test_district_title_native
    test_group_protocol
    test_group_reader
    test_group_native
    test_group_inverse
    test_group_runtime
    test_duplicate_protocol
    test_duplicate_records
    test_duplicate_native
    test_duplicate_reader
    test_duplicate_binding
    test_duplicate_runtime
    test_group_history_protocol
    test_group_history_reader
    test_group_history_runtime
    test_group_history_selection
    test_duplicate_history_protocol
    test_duplicate_history_reader
    test_duplicate_history_runtime
    test_duplicate_building_history
    test_duplicate_localized_history
    test_duplicate_owned_record
    test_duplicate_building_values
    test_duplicate_history_effects
    test_scenery_stroke_capture
    test_scenery_stroke_protocol
    test_scenery_stroke_state
    test_scenery_stroke_history
    test_scenery_alias_order
    test_group_object_capture
    test_group_object_move_reader
    test_group_object_graph
    test_group_object_protocol
    test_boss_placement_reader
    test_boss_placement_transaction
    test_boss_placement_state
    test_boss_placement_protocol
 )
    add_executable(${target} "${MT2MP_SOURCE_ROOT}/test/${target}.cpp")
    if(target STREQUAL "test_group_object_capture")
        target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/ui_remove/ui_capture.cpp")
    endif()
    if(target STREQUAL "test_selection_history_runtime" OR target STREQUAL "test_region_history_runtime" OR target STREQUAL "test_path_history_runtime")
        target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/move_contract.cpp")
        target_link_libraries(${target} PRIVATE world_edit_session_checks)
    endif()
    if(target STREQUAL "test_path_history_runtime" OR target STREQUAL "test_region_history_runtime")
        target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/cash_contract.cpp")
    endif()
    if(target STREQUAL "test_selection_move_binding")
        target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/multiplayer_world/world_state.cpp" "${MT2MP_SOURCE_ROOT}/src/game/move_contract.cpp")
        target_link_libraries(${target} PRIVATE world_edit_session_checks)
    endif()
    if(target STREQUAL "test_group_runtime" OR target STREQUAL "test_group_history_runtime" OR target STREQUAL "test_duplicate_runtime" OR target STREQUAL "test_duplicate_history_runtime")
        target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/move_contract.cpp")
    endif()
    if(target STREQUAL "test_duplicate_owned_record" OR target STREQUAL "test_duplicate_building_values")
        target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/record_graph.cpp")
    endif()
    if(target STREQUAL "test_duplicate_binding" OR target STREQUAL "test_scenery_stroke_state")
        target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/multiplayer_world/world_state.cpp")
    endif()
    target_include_directories(${target} PRIVATE "${WORLD_EDIT_GENERATED}" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    if(target MATCHES "_protocol$" OR target STREQUAL "test_npc_property_capture" OR target STREQUAL "test_prop_text_native" OR target STREQUAL "test_service_membership_native" OR target MATCHES "^test_district.*_native$" OR target MATCHES "^test_boss_phase_" OR target STREQUAL "test_group_native" OR target STREQUAL "test_group_runtime" OR target STREQUAL "test_group_history_runtime" OR target STREQUAL "test_duplicate_native" OR target STREQUAL "test_duplicate_runtime" OR target STREQUAL "test_duplicate_history_runtime" OR target STREQUAL "test_scenery_stroke_state")
        target_link_libraries(${target} PRIVATE world_edit_session_checks)
    endif()
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 60)
endforeach()
