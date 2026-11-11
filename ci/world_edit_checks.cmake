add_library(world_edit_session_checks STATIC
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
target_include_directories(world_edit_session_checks PRIVATE "${MT2MP_SOURCE_ROOT}/include" "${MT2MP_SOURCE_ROOT}/src/game")
target_compile_features(world_edit_session_checks PRIVATE cxx_std_20)
target_compile_definitions(world_edit_session_checks PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(world_edit_session_checks PRIVATE /W4 /WX /utf-8 /EHsc)
target_link_libraries(world_edit_session_checks PRIVATE source_check_codecs_windows bcrypt)
foreach(target
    test_world_path_protocol
    test_mixed_path_graph
    test_mixed_path_reader
    test_wall_private_lifecycle
    test_region_appearance_protocol
    test_npc_property_protocol
    test_dungeon_config_ui
    test_region_sale_protocol
    test_region_sale_reader
    test_prop_text_protocol
    test_boss_phase_protocol
    test_boss_phase_reader
    test_service_membership_protocol
    test_service_membership_reader
    test_selection_move_protocol
    test_selection_move_native
    test_selection_history_runtime
    test_region_history_protocol
    test_quest_history_protocol
    test_quest_history_read
    test_terrain_history_protocol
    test_path_history_graph
    test_path_history_protocol
    test_path_history_runtime
    test_region_history_runtime
    test_district_saved_identity
    test_district_lifecycle
    test_district_stroke_graph
    test_district_stroke_protocol
    test_district_protocol
    test_district_title_protocol
    test_group_protocol
    test_group_reader
    test_group_native
    test_group_inverse
    test_duplicate_protocol
    test_duplicate_records
    test_duplicate_native
    test_duplicate_reader
    test_duplicate_binding
    test_group_history_protocol
    test_group_history_reader
    test_duplicate_history_protocol
    test_duplicate_history_reader
    test_duplicate_building_history
    test_duplicate_localized_history
    test_duplicate_owned_record
    test_duplicate_building_values
    test_duplicate_history_effects
    test_scenery_stroke_capture
    test_scenery_stroke_protocol
    test_scenery_stroke_history
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
    if(target STREQUAL "test_duplicate_owned_record" OR target STREQUAL "test_duplicate_building_values")
        target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/record_graph.cpp")
    endif()
    if(target STREQUAL "test_duplicate_binding")
        target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/multiplayer_world/world_state.cpp")
    endif()
    target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    if(target MATCHES "_protocol$" OR target MATCHES "^test_boss_phase_" OR target STREQUAL "test_group_native" OR target STREQUAL "test_duplicate_native")
        target_link_libraries(${target} PRIVATE world_edit_session_checks)
    endif()
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 60)
endforeach()
add_executable(test_scenery_gesture "${CMAKE_CURRENT_LIST_DIR}/../test/test_scenery_gesture.cpp")
target_include_directories(test_scenery_gesture PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../src/game")
target_compile_features(test_scenery_gesture PRIVATE cxx_std_20)
add_test(NAME scenery_gesture COMMAND test_scenery_gesture)
# Exercise ordered edit evidence.
foreach(name group_evidence ordered_edit_evidence)
    add_executable(test_${name} "${MT2MP_SOURCE_ROOT}/test/test_${name}.cpp")
    target_include_directories(test_${name} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(test_${name} PRIVATE cxx_std_20)
    target_compile_definitions(test_${name} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(test_${name} PRIVATE /utf-8 /EHsc /bigobj)
    target_link_options(test_${name} PRIVATE /STACK:16777216)
    if(name STREQUAL "group_evidence")
        target_link_libraries(test_${name} PRIVATE world_edit_session_checks)
    elseif(name STREQUAL "ordered_edit_evidence")
        target_link_libraries(test_${name} PRIVATE source_check_codecs_windows)
    endif()
    add_test(NAME test_${name} COMMAND test_${name})
endforeach()
add_executable(test_ordered_trace_scope "${MT2MP_SOURCE_ROOT}/test/test_ordered_trace_scope.cpp")
target_include_directories(test_ordered_trace_scope PRIVATE "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_ordered_trace_scope PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_ordered_trace_scope PRIVATE /W4 /WX /utf-8 /EHsc /UNDEBUG)
else()
    target_compile_options(test_ordered_trace_scope PRIVATE -UNDEBUG)
endif()
add_test(NAME ordered_trace_scope COMMAND test_ordered_trace_scope)
set_tests_properties(ordered_trace_scope PROPERTIES TIMEOUT 30 LABELS ordered)
