cmake_minimum_required(VERSION 3.20)

project(MT2MP_SourceChecks LANGUAGES CXX)

if(WIN32)
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

if(MSVC)
    add_compile_options(/bigobj)
endif()

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(MT2MP_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." CACHE PATH "Canonical source root")

get_filename_component(MT2MP_SOURCE_ROOT "${MT2MP_SOURCE_ROOT}" REALPATH)

if(MSVC)
    # Keep project warnings strict; bundled third-party headers have their own policy.
    add_compile_options("/external:I${MT2MP_SOURCE_ROOT}/include/vendor" /external:W0)
    # Source-body fixtures use the supported game executable stack reserve.
    add_link_options(/STACK:2097152)
endif()

enable_testing()

foreach(name action_order software_snapshot snapshot_buffer slow_frame_diagnostics queued_project_admission party_capture_references scene_building_registry_preview)

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

include("${CMAKE_CURRENT_LIST_DIR}/dropin_proof_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/dropin_write_dir_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/snapshot_world_path_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_observer_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/empty_network_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_state_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_development_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_progress_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_movement_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_startingpoint_client_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_creation_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_commit_intent_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_client_commit_ui_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_start_intent_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_staff_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_staff_display_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_message_close_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_completion_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_network_tab_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/npc_economy_difference_checks.cmake")

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
include("${CMAKE_CURRENT_LIST_DIR}/dungeon_input_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dungeon_input_action_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/developer_job_target_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/developer_movement_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/buildings_tab_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/bug_authority_portable_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/bug_authority_native_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/bug_type7_portable_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/bug_type7_native_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/bug_policy_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/toon_equipment_value_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/toon_weapon_records_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/toon_weapon_visual_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/toon_weapon_runtime_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/render_pool_guard_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/record_api_resolver_checks.cmake")

include(toon_weapon_probe_checks.cmake)

include("${CMAKE_CURRENT_LIST_DIR}/toon_time_fence_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/bug_fix_action_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/bug_manual_native_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/research_action_checks.cmake")


include("${CMAKE_CURRENT_LIST_DIR}/research_native_checks.cmake")

include(query_phase_checks.cmake)

include("${CMAKE_CURRENT_LIST_DIR}/scenario_vip_authority_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/toon_time_timing_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/vip_frame_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/vip_runtime_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/monster_edit_history_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/npc_history_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/character_import_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dungeon_building_codec_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/dungeon_building_sample_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/marketing_native_probe_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/marketing_frame_checks.cmake")

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

include("${CMAKE_CURRENT_LIST_DIR}/coop_session_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/world_edit_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/network_history_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/gm_policy_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/game_rules_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/intervention_checks.cmake")

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

add_executable(mt2mp_test_move_selection_refresh "${MT2MP_SOURCE_ROOT}/test/test_move_selection_refresh.cpp")
target_include_directories(mt2mp_test_move_selection_refresh PRIVATE "${MT2MP_SOURCE_ROOT}/src/game")
target_compile_features(mt2mp_test_move_selection_refresh PRIVATE cxx_std_20)
add_test(NAME move_selection_refresh COMMAND mt2mp_test_move_selection_refresh)
add_executable(mt2mp_test_move_widget_refresh "${MT2MP_SOURCE_ROOT}/test/test_move_widget_refresh.cpp")
target_include_directories(mt2mp_test_move_widget_refresh PRIVATE "${MT2MP_SOURCE_ROOT}/src/game")
target_compile_features(mt2mp_test_move_widget_refresh PRIVATE cxx_std_20)
add_test(NAME move_widget_refresh COMMAND mt2mp_test_move_widget_refresh)

include("${CMAKE_CURRENT_LIST_DIR}/shared_codec_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ordered_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/startup_calendar_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/source_shards.cmake")
