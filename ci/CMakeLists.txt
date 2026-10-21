cmake_minimum_required(VERSION 3.20)

project(MT2MP_SourceChecks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MT2MP_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." CACHE PATH "Canonical source root")

get_filename_component(MT2MP_SOURCE_ROOT "${MT2MP_SOURCE_ROOT}" REALPATH)

enable_testing()

foreach(name action_order software_snapshot snapshot_buffer)

    add_executable(test_${name} "${MT2MP_SOURCE_ROOT}/test/test_${name}.cpp")

    target_include_directories(test_${name} PRIVATE "${MT2MP_SOURCE_ROOT}/include")

    target_compile_definitions(test_${name} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)

    if(MSVC)

        target_compile_options(test_${name} PRIVATE /W4 /WX)

    endif()

    add_test(NAME ${name} COMMAND test_${name})

    set_tests_properties(${name} PROPERTIES TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

endforeach()

if(WIN32)

    target_link_libraries(test_snapshot_buffer PRIVATE bcrypt)

endif()



# Compile the actual native callback bodies against bounded test doubles.

set(icon_source "${MT2MP_SOURCE_ROOT}/src/game/native_lobby.cpp")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${icon_source}")

file(READ "${icon_source}" icon_text)

string(FIND "${icon_text}" "struct DesktopIconBinding" state_begin)

string(FIND "${icon_text}" "// The game owns the native icon" state_end)

string(FIND "${icon_text}" "void desktop_icon_launch(" callback_begin)

string(FIND "${icon_text}" "bool refresh_saved_worlds(" callback_end)

if(state_begin LESS 0 OR state_end LESS state_begin OR callback_begin LESS 0 OR callback_end LESS callback_begin)

    message(FATAL_ERROR "Native Desktop callback boundaries changed; update the affected test extraction")

endif()

math(EXPR state_size "${state_end}-${state_begin}")

math(EXPR callback_size "${callback_end}-${callback_begin}")

string(SUBSTRING "${icon_text}" ${state_begin} ${state_size} icon_state)

string(SUBSTRING "${icon_text}" ${callback_begin} ${callback_size} icon_callbacks)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/desktop_icon_behavior_body.inc" "${icon_state}\n${icon_callbacks}")

add_executable(test_desktop_icon_behavior "${MT2MP_SOURCE_ROOT}/test/test_desktop_icon_behavior.cpp")

target_include_directories(test_desktop_icon_behavior PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

if(MSVC)

    target_compile_options(test_desktop_icon_behavior PRIVATE /W4 /WX /utf-8)

endif()

add_test(NAME desktop_icon_behavior COMMAND test_desktop_icon_behavior)

set_tests_properties(desktop_icon_behavior PROPERTIES TIMEOUT 60)



string(FIND "${icon_text}" "bool select_world(" world_end)

if(callback_end LESS 0 OR world_end LESS callback_end)

    message(FATAL_ERROR "Native world selection boundaries changed; update the affected test extraction")

endif()

math(EXPR world_size "${world_end}-${callback_end}")

string(SUBSTRING "${icon_text}" ${callback_end} ${world_size} world_body)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/world_selection_body.inc" "${world_body}")

add_executable(test_world_selection "${MT2MP_SOURCE_ROOT}/test/test_world_selection.cpp")

target_include_directories(test_world_selection PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${MT2MP_SOURCE_ROOT}")

if(MSVC)

    target_compile_options(test_world_selection PRIVATE /W4 /WX /utf-8)

endif()

add_test(NAME world_selection COMMAND test_world_selection)

set_tests_properties(world_selection PROPERTIES TIMEOUT 60)



string(FIND "${icon_text}" "bool bind_desktop_icon_presentation(" presentation_begin)

string(FIND "${icon_text}" "bool desktop_entry(" presentation_end)

if(presentation_begin LESS 0 OR presentation_end LESS presentation_begin)

    message(FATAL_ERROR "Native presentation boundaries changed; update the affected test extraction")

endif()

math(EXPR presentation_size "${presentation_end}-${presentation_begin}")

string(SUBSTRING "${icon_text}" ${presentation_begin} ${presentation_size} presentation_body)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/desktop_icon_presentation_body.inc" "${presentation_body}")

add_executable(test_desktop_icon_presentation "${MT2MP_SOURCE_ROOT}/test/test_desktop_icon_presentation.cpp")

target_include_directories(test_desktop_icon_presentation PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

if(MSVC)

    target_compile_options(test_desktop_icon_presentation PRIVATE /W4 /WX /utf-8)

endif()

add_test(NAME desktop_icon_presentation COMMAND test_desktop_icon_presentation)

set_tests_properties(desktop_icon_presentation PROPERTIES TIMEOUT 60)



include("${CMAKE_CURRENT_LIST_DIR}/auto_world_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/invite_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/empty_world_bind_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/dropin_proof_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/dropin_write_dir_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/cold_time_retire_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/snapshot_world_path_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/tutorial_observer_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/empty_network_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/client_leave_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/original_saves_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/private_save_parent_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/named_world_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/diagnostic_mailbox_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_state_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_load_gate_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_development_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_progress_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_movement_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_startingpoint_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/tutorial_startingpoint_client_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/tutorial_creation_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/tutorial_commit_intent_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/tutorial_client_commit_ui_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_start_intent_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/tutorial_staff_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/tutorial_client_staff_ui_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_staff_activity_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_staff_display_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_message_close_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_completion_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_network_tab_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/tutorial_load_start_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/npc_economy_difference_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/locked_lobby_leave_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/npc_state_guard_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/npc_pose_phase_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/monster_zone_probe_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/developer_staff_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/developer_staff_retirement_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/developer_job_census_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/developer_staff_gameplay_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/developer_construction_portable_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/developer_construction_native_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/monster_zone_action_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/direct_monster_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/management_simulation_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/character_calendar_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dungeon_input_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dungeon_input_action_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/developer_job_target_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/developer_movement_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/snapshot_capture_policy_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/buildings_tab_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/building_remove_restore_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/bug_authority_portable_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/bug_authority_native_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/building_types_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/bug_type7_portable_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/bug_type7_native_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/bug_policy_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/host_departure_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/toon_equipment_value_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/frame_fragments_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/toon_weapon_records_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/prepare_sync_failure_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/toon_weapon_visual_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/toon_weapon_runtime_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/render_pool_guard_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/record_api_resolver_checks.cmake")



include(toon_weapon_probe_checks.cmake)

include("${CMAKE_CURRENT_LIST_DIR}/loaded_movement_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/toon_time_fence_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/bug_fix_action_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/bug_manual_native_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/research_action_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/research_reader_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/research_fixture_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/research_native_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/research_runtime_checks.cmake")



include(live_autosave_checks.cmake)



include(research_reader_scope_checks.cmake)



include(quest_reader_scope_checks.cmake)



include(query_attribution_checks.cmake)



include(query_phase_checks.cmake)



include(placement_reader_scope_checks.cmake)



include("${CMAKE_CURRENT_LIST_DIR}/tutorial_reader_scope_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/subscriber_npc_reference_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/subscriber_online_native_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/scenario_vip_authority_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/toon_time_timing_checks.cmake")



include(engine_phase_timing_checks.cmake)



include("${CMAKE_CURRENT_LIST_DIR}/vip_frame_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/vip_runtime_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/monster_edit_history_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/npc_history_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/character_import_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dungeon_building_codec_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dungeon_building_sample_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/shared_codec_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/time_host_capture_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/marketing_native_probe_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/marketing_frame_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/steam_lifecycle_admission_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/steam_hud_admission_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/chat_checks.cmake")



add_executable(test_steam_presence "${CMAKE_CURRENT_LIST_DIR}/../test/test_steam_presence.cpp")

target_compile_features(test_steam_presence PRIVATE cxx_std_17)

target_include_directories(test_steam_presence PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../include")

if(MSVC)

    target_compile_options(test_steam_presence PRIVATE /UNDEBUG)

else()

    target_compile_options(test_steam_presence PRIVATE -UNDEBUG)

endif()

add_test(NAME steam_presence_behavior COMMAND test_steam_presence)

include(reporting_checks.cmake)

include(lobby_presentation_checks.cmake)

include(steam_avatar_checks.cmake)

include(lobby_layout_checks.cmake)

include(lobby_assets_checks.cmake)

include("${CMAKE_CURRENT_LIST_DIR}/lobby_handoff_checks.cmake")



include("${CMAKE_CURRENT_LIST_DIR}/coop_session_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/world_edit_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/network_history_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/gm_policy_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/game_rules_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/intervention_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/native_state_regression_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/live_start_publication_checks.cmake")



# Extraction checks can name the same native input through different relative paths.

# Keep one dependency per file so Ninja can regenerate without duplicate outputs.

get_property(configure_inputs DIRECTORY PROPERTY CMAKE_CONFIGURE_DEPENDS)

set(canonical_configure_inputs)

foreach(input IN LISTS configure_inputs)

    get_filename_component(canonical_input "${input}" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

    list(APPEND canonical_configure_inputs "${canonical_input}")

endforeach()

list(REMOVE_DUPLICATES canonical_configure_inputs)

set_property(DIRECTORY PROPERTY CMAKE_CONFIGURE_DEPENDS "${canonical_configure_inputs}")



include(network_object_checks.cmake)

include(logo_action_checks.cmake)
include("${CMAKE_CURRENT_LIST_DIR}/party_frame_scope_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/native_domain_alias_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/scene_clone_confirmation_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/pending_binding_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/time_item_forwarding_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/object_ui_probe_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/placement_input_order_checks.cmake")
add_executable(mt2mp_test_move_selection_refresh "${MT2MP_SOURCE_ROOT}/test/test_move_selection_refresh.cpp")
target_include_directories(mt2mp_test_move_selection_refresh PRIVATE "${MT2MP_SOURCE_ROOT}/src/game")
target_compile_features(mt2mp_test_move_selection_refresh PRIVATE cxx_std_20)
add_test(NAME move_selection_refresh COMMAND mt2mp_test_move_selection_refresh)
add_executable(mt2mp_test_move_widget_refresh "${MT2MP_SOURCE_ROOT}/test/test_move_widget_refresh.cpp")
target_include_directories(mt2mp_test_move_widget_refresh PRIVATE "${MT2MP_SOURCE_ROOT}/src/game")
target_compile_features(mt2mp_test_move_widget_refresh PRIVATE cxx_std_20)
add_test(NAME move_widget_refresh COMMAND mt2mp_test_move_widget_refresh)


# Check normal-session hook admission using only the local source tree.
add_test(NAME startup_hook_catalog_consumer
    COMMAND "${Python3_EXECUTABLE}" -B -X utf8
    "${MT2MP_SOURCE_ROOT}/test/test_startup_hook_catalog_consumer.py"
    --source-root "${MT2MP_SOURCE_ROOT}")
