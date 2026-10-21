if(WIN32)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(NOT DEFINED QUERY_DEPENDENCY_ROOT)
    get_filename_component(QUERY_DEPENDENCY_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../test/check_query_adapter_tags.py"
    --root "${CMAKE_CURRENT_LIST_DIR}/.." COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/live_host_state/apply_timing.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/object_identity/remove_runtime/copy/world_prop_reader.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/object_identity/remove_runtime/copy/saved_scenery_reader.h"
    "${CMAKE_CURRENT_LIST_DIR}/../test/world_prop_attribution_cases.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../test/pure_scenery_field_cases.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../test/scenery_state_field_cases.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/scenery_kiss/scenery_state.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/scenery_kiss/scenery_state.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/typed_action/action_session.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/scenery_kiss/native_catalog.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../test/extract_query_scope_loops.py"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/character_actor_records_runtime.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/character_definition_identity.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/intervention_subscriber_live.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/subscriber_intervention.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/release_reference_runtime.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/boss_current.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/dungeon_scene_identity_runtime.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/party_saved_identity_runtime.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/dungeon_party_census.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/party_saved_identity.h"
    "${CMAKE_CURRENT_LIST_DIR}/../test/party_pool_scope_cases.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/world_zone_current.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/dungeon_zone_roster.h"
    "${CMAKE_CURRENT_LIST_DIR}/../test/world_zone_manager_scope_cases.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/intervention_census.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/boss_current.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/root_saved_building.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/intervention_monster_roster.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/monster_zone_read.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/character_boss_retirement_read.h"
    "${CMAKE_CURRENT_LIST_DIR}/../test/shared_census_scope_cases.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/general_project_runtime.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/general_project_identity_reader.h"
    "${CMAKE_CURRENT_LIST_DIR}/../test/general_project_identity_cases.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../test/composed_character_census_cases.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/dungeon_scene_roster.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/dungeon_scene_key.h"
    "${CMAKE_CURRENT_LIST_DIR}/../test/dungeon_roster_scope_cases.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/region_saved_identity.h"
    "${CMAKE_CURRENT_LIST_DIR}/../test/region_identity_scope_cases.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../test/saved_root_index_scope_cases.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../test/saved_root_batch_lifetime_cases.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/object_identity/remove_runtime/copy/world_prop_reader.h"
    "${CMAKE_CURRENT_LIST_DIR}/../test/saved_scenery_batch_source_boundary.py"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/object_identity/remove_runtime/copy/native_origin.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/root_scenery_snapshot.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../test/saved_root_origin_cases.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/object_identity/remove_runtime/copy/saved_scenery_batch.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/object_identity/remove_runtime/copy/saved_scenery_batch_runtime.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/release_reference_reader.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/release_runtime.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/character_membership_reader.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/native_open_witness.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/native_open_witness.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/npc_remove_runtime.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/npc_saved_identity_runtime.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/npc_saved_identity_runtime.h")
set(QUERY_SCOPE_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/query_scope_generated")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../test/extract_query_scope_loops.py"
    --root "${CMAKE_CURRENT_LIST_DIR}/.." --output "${QUERY_SCOPE_GENERATED}" COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_query_attribution "${CMAKE_CURRENT_LIST_DIR}/../test/test_query_attribution.cpp")
target_compile_features(test_query_attribution PRIVATE cxx_std_20)
target_include_directories(test_query_attribution PRIVATE "${QUERY_SCOPE_GENERATED}" "${CMAKE_CURRENT_LIST_DIR}/../src/game" "${QUERY_DEPENDENCY_ROOT}/src/game" "${QUERY_DEPENDENCY_ROOT}/include")
target_compile_options(test_query_attribution PRIVATE /W4 /WX /permissive-)
add_test(NAME query_attribution COMMAND test_query_attribution)
endif()
