foreach(name protocol controller)
    set(target "test_npc_quest_edit_${name}")
    add_executable(${target} "${MT2MP_SOURCE_ROOT}/test/${target}.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
    target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME npc_quest_edit_${name} COMMAND ${target})
    set_tests_properties(npc_quest_edit_${name} PROPERTIES TIMEOUT 60)
endforeach()
foreach(name character_membership_reader character_npc_completion character_actor_records character_membership_protocol character_membership_controller character_remove_origin character_boss_retirement quest_cascade character_catalogue_proof)
    set(target "test_${name}")
    add_executable(${target} "${MT2MP_SOURCE_ROOT}/test/${target}.cpp")
    if(name STREQUAL "character_membership_protocol" OR name STREQUAL "character_membership_controller")
        target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
    elseif(name STREQUAL "character_remove_origin")
        target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/ui_remove/native_contract.cpp" "${MT2MP_SOURCE_ROOT}/src/game/ui_remove/ui_capture.cpp")
    endif()
    target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME ${name} COMMAND ${target})
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endforeach()
foreach(family rename stat)
    foreach(name protocol controller)
        set(target "test_character_${family}_${name}")
        add_executable(${target} "${MT2MP_SOURCE_ROOT}/test/${target}.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
        target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
        target_compile_features(${target} PRIVATE cxx_std_20)
        target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
        endif()
        add_test(NAME character_${family}_${name} COMMAND ${target})
        set_tests_properties(character_${family}_${name} PROPERTIES TIMEOUT 60)
    endforeach()
endforeach()
foreach(family ability_edit costume_edit dungeon_config dungeon_light dungeon_sound dungeon_palette dungeon_gizmo building_status)
    foreach(name protocol controller)
        set(target "test_${family}_${name}")
        add_executable(${target} "${MT2MP_SOURCE_ROOT}/test/${target}.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
        if(family STREQUAL "dungeon_gizmo" AND name STREQUAL "controller")
            target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/network/node_state.cpp")
        endif()
        target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
        target_compile_features(${target} PRIVATE cxx_std_20)
        target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
        endif()
        add_test(NAME ${family}_${name} COMMAND ${target})
        set_tests_properties(${family}_${name} PROPERTIES TIMEOUT 60)
    endforeach()
endforeach()
add_test(NAME npc_quest_edit_mcp COMMAND "${Python3_EXECUTABLE}" -B -X utf8
    "${MT2MP_SOURCE_ROOT}/test/test_npc_quest_edit_mcp.py" "${MT2MP_SOURCE_ROOT}/tools/mcp/server.py")
set_tests_properties(npc_quest_edit_mcp PROPERTIES TIMEOUT 60)
add_executable(test_dungeon_scene "${MT2MP_SOURCE_ROOT}/test/test_dungeon_scene.cpp")
target_include_directories(test_dungeon_scene PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_dungeon_scene PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_dungeon_scene PRIVATE /W4 /WX /utf-8 /EHsc)
endif()

add_executable(test_character_actor_party_owners "${MT2MP_SOURCE_ROOT}/test/test_character_actor_party_owners.cpp")
target_include_directories(test_character_actor_party_owners PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_character_actor_party_owners PRIVATE cxx_std_20)
target_compile_definitions(test_character_actor_party_owners PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_character_actor_party_owners PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME character_actor_party_owners COMMAND test_character_actor_party_owners)
set_tests_properties(character_actor_party_owners PROPERTIES TIMEOUT 60)
add_test(NAME dungeon_scene COMMAND test_dungeon_scene)
set_tests_properties(dungeon_scene PROPERTIES TIMEOUT 60)

set(journal_cleanup_source "${CMAKE_CURRENT_BINARY_DIR}/test_character_journal_retirement.cpp")
add_custom_command(OUTPUT "${journal_cleanup_source}"
    COMMAND "${Python3_EXECUTABLE}" -B -X utf8
        "${MT2MP_SOURCE_ROOT}/test/extract_character_journal_retirement.py"
        --source "${MT2MP_SOURCE_ROOT}" --emit "${journal_cleanup_source}"
    DEPENDS "${MT2MP_SOURCE_ROOT}/test/extract_character_journal_retirement.py"
        "${MT2MP_SOURCE_ROOT}/src/game/npc_quest_list_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/character_rename_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/ability_edit_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/building_status_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/costume_edit_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/dungeon_config_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/dungeon_light_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/dungeon_sound_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/dungeon_palette_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/dungeon_gizmo_runtime.inc"
    VERBATIM)
add_executable(test_character_journal_retirement "${journal_cleanup_source}")
target_compile_features(test_character_journal_retirement PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_character_journal_retirement PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME character_journal_retirement COMMAND test_character_journal_retirement)
set_tests_properties(character_journal_retirement PROPERTIES TIMEOUT 60)

add_executable(test_dungeon_config_input "${MT2MP_SOURCE_ROOT}/test/test_dungeon_config_input.cpp")
target_include_directories(test_dungeon_config_input PRIVATE "${MT2MP_SOURCE_ROOT}/src/game")
target_compile_features(test_dungeon_config_input PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_dungeon_config_input PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME dungeon_config_input COMMAND test_dungeon_config_input)
set_tests_properties(dungeon_config_input PROPERTIES TIMEOUT 60)

add_executable(test_dungeon_sound_grid "${MT2MP_SOURCE_ROOT}/test/test_dungeon_sound_grid.cpp")
target_include_directories(test_dungeon_sound_grid PRIVATE "${MT2MP_SOURCE_ROOT}/src/game")
target_compile_features(test_dungeon_sound_grid PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_dungeon_sound_grid PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME dungeon_sound_grid COMMAND test_dungeon_sound_grid)
set_tests_properties(dungeon_sound_grid PROPERTIES TIMEOUT 60)

add_executable(test_dungeon_palette_native "${MT2MP_SOURCE_ROOT}/test/test_dungeon_palette_native.cpp")
target_include_directories(test_dungeon_palette_native PRIVATE "${MT2MP_SOURCE_ROOT}/src/game")
target_compile_features(test_dungeon_palette_native PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_dungeon_palette_native PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME dungeon_palette_native COMMAND test_dungeon_palette_native)
set_tests_properties(dungeon_palette_native PROPERTIES TIMEOUT 60)
add_executable(test_dungeon_layout_reader "${MT2MP_SOURCE_ROOT}/test/test_dungeon_layout_reader.cpp")
target_include_directories(test_dungeon_layout_reader PRIVATE "${MT2MP_SOURCE_ROOT}/src/game")
target_compile_features(test_dungeon_layout_reader PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_dungeon_layout_reader PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME dungeon_layout_reader COMMAND test_dungeon_layout_reader)
set_tests_properties(dungeon_layout_reader PROPERTIES TIMEOUT 60)

add_executable(test_dungeon_template_owner "${MT2MP_SOURCE_ROOT}/test/test_dungeon_template_owner.cpp")
target_include_directories(test_dungeon_template_owner PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_dungeon_template_owner PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_dungeon_template_owner PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME dungeon_template_owner COMMAND test_dungeon_template_owner)
set_tests_properties(dungeon_template_owner PROPERTIES TIMEOUT 60)

add_executable(test_dungeon_gizmo_reader "${MT2MP_SOURCE_ROOT}/test/test_dungeon_gizmo_reader.cpp")
target_include_directories(test_dungeon_gizmo_reader PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_dungeon_gizmo_reader PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_dungeon_gizmo_reader PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME dungeon_gizmo_reader COMMAND test_dungeon_gizmo_reader)
set_tests_properties(dungeon_gizmo_reader PROPERTIES TIMEOUT 60)

add_executable(test_dungeon_gizmo_identity "${MT2MP_SOURCE_ROOT}/test/test_dungeon_gizmo_identity.cpp")
target_include_directories(test_dungeon_gizmo_identity PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_dungeon_gizmo_identity PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_dungeon_gizmo_identity PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME dungeon_gizmo_identity COMMAND test_dungeon_gizmo_identity)
set_tests_properties(dungeon_gizmo_identity PROPERTIES TIMEOUT 60)

add_executable(test_dungeon_scene_roster "${MT2MP_SOURCE_ROOT}/test/test_dungeon_scene_roster.cpp")
target_include_directories(test_dungeon_scene_roster PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_dungeon_scene_roster PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_dungeon_scene_roster PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME dungeon_scene_roster COMMAND test_dungeon_scene_roster)
set_tests_properties(dungeon_scene_roster PROPERTIES TIMEOUT 60)

add_executable(test_dungeon_gizmo_native "${MT2MP_SOURCE_ROOT}/test/test_dungeon_gizmo_native.cpp")
target_include_directories(test_dungeon_gizmo_native PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_dungeon_gizmo_native PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_dungeon_gizmo_native PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME dungeon_gizmo_native COMMAND test_dungeon_gizmo_native)
set_tests_properties(dungeon_gizmo_native PROPERTIES TIMEOUT 60)
add_executable(test_character_definition_identity "${MT2MP_SOURCE_ROOT}/test/test_character_definition_identity.cpp")
target_include_directories(test_character_definition_identity PRIVATE "${MT2MP_SOURCE_ROOT}/src/game")
target_compile_features(test_character_definition_identity PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_character_definition_identity PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME character_definition_identity COMMAND test_character_definition_identity)
set_tests_properties(character_definition_identity PROPERTIES TIMEOUT 60)

set(DUNGEON_CONFIG_HELPERS_EXTRACTED "${CMAKE_CURRENT_BINARY_DIR}/dungeon_config_helpers_extracted")
add_custom_command(
    OUTPUT "${DUNGEON_CONFIG_HELPERS_EXTRACTED}/dungeon_config_helpers_actual.inc"
    COMMAND "${Python3_EXECUTABLE}" -B -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_dungeon_config_helpers.py" "${MT2MP_SOURCE_ROOT}" "${DUNGEON_CONFIG_HELPERS_EXTRACTED}"
    DEPENDS "${MT2MP_SOURCE_ROOT}/test/extract_dungeon_config_helpers.py" "${MT2MP_SOURCE_ROOT}/src/game/dungeon_config_runtime.inc"
    VERBATIM)
add_executable(test_dungeon_config_helper_routes "${MT2MP_SOURCE_ROOT}/test/test_dungeon_config_helper_routes.cpp" "${DUNGEON_CONFIG_HELPERS_EXTRACTED}/dungeon_config_helpers_actual.inc")
target_include_directories(test_dungeon_config_helper_routes PRIVATE "${DUNGEON_CONFIG_HELPERS_EXTRACTED}")
target_compile_features(test_dungeon_config_helper_routes PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_dungeon_config_helper_routes PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME dungeon_config_helper_routes COMMAND test_dungeon_config_helper_routes)
set_tests_properties(dungeon_config_helper_routes PROPERTIES TIMEOUT 60)

set(DUNGEON_REFERENCE_EXTRACTED "${CMAKE_CURRENT_BINARY_DIR}/dungeon_reference_extracted")
add_custom_command(
    OUTPUT "${DUNGEON_REFERENCE_EXTRACTED}/dungeon_quest_destination_actual.inc"
    COMMAND "${Python3_EXECUTABLE}" -B -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_dungeon_quest_destination.py" "${MT2MP_SOURCE_ROOT}" "${DUNGEON_REFERENCE_EXTRACTED}"
    DEPENDS "${MT2MP_SOURCE_ROOT}/test/extract_dungeon_quest_destination.py" "${MT2MP_SOURCE_ROOT}/src/game/npc_quest_target_runtime.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/npc_quest_authoring_runtime.inc"
    VERBATIM)

add_executable(test_dungeon_monster_reference "${MT2MP_SOURCE_ROOT}/test/test_dungeon_monster_reference.cpp")
target_include_directories(test_dungeon_monster_reference PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${DUNGEON_REFERENCE_EXTRACTED}")
target_compile_features(test_dungeon_monster_reference PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_dungeon_monster_reference PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME dungeon_monster_reference COMMAND test_dungeon_monster_reference)
set_tests_properties(dungeon_monster_reference PROPERTIES TIMEOUT 60)

add_executable(test_dungeon_quest_destination "${MT2MP_SOURCE_ROOT}/test/test_dungeon_quest_destination.cpp" "${DUNGEON_REFERENCE_EXTRACTED}/dungeon_quest_destination_actual.inc")
target_include_directories(test_dungeon_quest_destination PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${DUNGEON_REFERENCE_EXTRACTED}")
target_compile_features(test_dungeon_quest_destination PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_dungeon_quest_destination PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME dungeon_quest_destination COMMAND test_dungeon_quest_destination)
set_tests_properties(dungeon_quest_destination PROPERTIES TIMEOUT 60)

add_executable(test_dungeon_zone_roster "${MT2MP_SOURCE_ROOT}/test/test_dungeon_zone_roster.cpp")
target_include_directories(test_dungeon_zone_roster PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${DUNGEON_REFERENCE_EXTRACTED}")
target_compile_features(test_dungeon_zone_roster PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_dungeon_zone_roster PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME dungeon_zone_roster COMMAND test_dungeon_zone_roster)
set_tests_properties(dungeon_zone_roster PROPERTIES TIMEOUT 60)

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    enable_language(C)
    set(NATIVE_UI_BOUNDARIES "${CMAKE_CURRENT_BINARY_DIR}/native_ui_boundaries")
    set(boundary_sources)
    foreach(family ability_edit costume_edit dungeon_config dungeon_light dungeon_sound dungeon_palette)
        list(APPEND boundary_sources "${MT2MP_SOURCE_ROOT}/src/game/${family}_runtime.inc")
    endforeach()
    set(boundary_legacy)
    if(MT2MP_UI_LEGACY_PALETTE)
        list(APPEND boundary_legacy --legacy-palette "${MT2MP_UI_LEGACY_PALETTE}")
        list(APPEND boundary_sources "${MT2MP_UI_LEGACY_PALETTE}")
    endif()
    add_custom_command(OUTPUT "${NATIVE_UI_BOUNDARIES}/native_ui_boundaries.inc"
        COMMAND "${Python3_EXECUTABLE}" -B -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_native_ui_boundaries.py" "${MT2MP_SOURCE_ROOT}" "${NATIVE_UI_BOUNDARIES}" ${boundary_legacy}
        DEPENDS "${MT2MP_SOURCE_ROOT}/test/extract_native_ui_boundaries.py" ${boundary_sources}
        VERBATIM)
    set(boundary_minhook "${MT2MP_SOURCE_ROOT}/include/vendor/minhook")
    add_library(mt2mp_boundary_minhook STATIC "${boundary_minhook}/src/hook.c" "${boundary_minhook}/src/buffer.c" "${boundary_minhook}/src/trampoline.c" "${boundary_minhook}/src/hde/hde64.c")
    target_include_directories(mt2mp_boundary_minhook PUBLIC "${boundary_minhook}/include")
    add_executable(test_native_ui_boundaries "${MT2MP_SOURCE_ROOT}/test/test_native_ui_boundaries.cpp" "${NATIVE_UI_BOUNDARIES}/native_ui_boundaries.inc")
    target_include_directories(test_native_ui_boundaries PRIVATE "${NATIVE_UI_BOUNDARIES}")
    target_compile_features(test_native_ui_boundaries PRIVATE cxx_std_20)
    target_compile_definitions(test_native_ui_boundaries PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(test_native_ui_boundaries PRIVATE mt2mp_boundary_minhook)
    if(MSVC)
        target_compile_options(test_native_ui_boundaries PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME native_ui_boundaries COMMAND test_native_ui_boundaries)
    set_tests_properties(native_ui_boundaries PROPERTIES TIMEOUT 60)
endif()

add_executable(test_building_npc_owner "${MT2MP_SOURCE_ROOT}/test/test_building_npc_owner.cpp")
target_include_directories(test_building_npc_owner PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_building_npc_owner PRIVATE cxx_std_20)
target_compile_definitions(test_building_npc_owner PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_building_npc_owner PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME building_npc_owner COMMAND test_building_npc_owner)
set_tests_properties(building_npc_owner PROPERTIES TIMEOUT 60)

add_executable(test_building_npc_protocol "${MT2MP_SOURCE_ROOT}/test/test_building_npc_protocol.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
target_include_directories(test_building_npc_protocol PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_building_npc_protocol PRIVATE cxx_std_20)
target_compile_definitions(test_building_npc_protocol PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_building_npc_protocol PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME building_npc_protocol COMMAND test_building_npc_protocol)
set_tests_properties(building_npc_protocol PROPERTIES TIMEOUT 60)

foreach(name dungeon_cells_native dungeon_scene_records dungeon_zone_cascade dungeon_cells_protocol dungeon_cells_controller dungeon_cells_history)
    set(target "test_${name}")
    add_executable(${target} "${MT2MP_SOURCE_ROOT}/test/${target}.cpp")
    if(name STREQUAL "dungeon_cells_protocol" OR name STREQUAL "dungeon_cells_controller")
        target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
    endif()
    target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME ${name} COMMAND ${target})
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endforeach()
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(DUNGEON_CELL_BOUNDARIES "${CMAKE_CURRENT_BINARY_DIR}/dungeon_cell_boundaries")
    add_custom_command(OUTPUT "${DUNGEON_CELL_BOUNDARIES}/native_ui_boundaries.inc"
        COMMAND "${Python3_EXECUTABLE}" -B -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_dungeon_cells_boundaries.py" "${MT2MP_SOURCE_ROOT}" "${DUNGEON_CELL_BOUNDARIES}"
        DEPENDS "${MT2MP_SOURCE_ROOT}/test/extract_dungeon_cells_boundaries.py"
            "${MT2MP_SOURCE_ROOT}/src/game/dungeon_cells_runtime.inc" "${MT2MP_SOURCE_ROOT}/src/game/dungeon_cells_ui_codes.h"
            "${MT2MP_SOURCE_ROOT}/src/game/dungeon_cell_graph_codes.h"
        VERBATIM)
    add_executable(test_dungeon_cell_boundaries "${MT2MP_SOURCE_ROOT}/test/test_native_ui_boundaries.cpp" "${DUNGEON_CELL_BOUNDARIES}/native_ui_boundaries.inc")
    target_include_directories(test_dungeon_cell_boundaries PRIVATE "${DUNGEON_CELL_BOUNDARIES}")
    target_compile_features(test_dungeon_cell_boundaries PRIVATE cxx_std_20)
    target_compile_definitions(test_dungeon_cell_boundaries PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(test_dungeon_cell_boundaries PRIVATE mt2mp_boundary_minhook)
    if(MSVC)
        target_compile_options(test_dungeon_cell_boundaries PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME dungeon_cell_boundaries COMMAND test_dungeon_cell_boundaries)
    set_tests_properties(dungeon_cell_boundaries PROPERTIES TIMEOUT 60)
    set(CHARACTER_MEMBERSHIP_BOUNDARIES "${CMAKE_CURRENT_BINARY_DIR}/character_membership_boundaries")
    add_custom_command(OUTPUT "${CHARACTER_MEMBERSHIP_BOUNDARIES}/native_ui_boundaries.inc"
        COMMAND "${Python3_EXECUTABLE}" -B -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_character_membership_boundaries.py" "${MT2MP_SOURCE_ROOT}" "${CHARACTER_MEMBERSHIP_BOUNDARIES}"
        DEPENDS "${MT2MP_SOURCE_ROOT}/test/extract_character_membership_boundaries.py"
            "${MT2MP_SOURCE_ROOT}/src/game/character_membership_runtime.inc" "${MT2MP_SOURCE_ROOT}/src/game/character_membership_codes.h"
        VERBATIM)
    add_executable(test_character_membership_boundaries "${MT2MP_SOURCE_ROOT}/test/test_native_ui_boundaries.cpp" "${CHARACTER_MEMBERSHIP_BOUNDARIES}/native_ui_boundaries.inc")
    target_include_directories(test_character_membership_boundaries PRIVATE "${CHARACTER_MEMBERSHIP_BOUNDARIES}")
    target_compile_features(test_character_membership_boundaries PRIVATE cxx_std_20)
    target_compile_definitions(test_character_membership_boundaries PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(test_character_membership_boundaries PRIVATE mt2mp_boundary_minhook)
    if(MSVC)
        target_compile_options(test_character_membership_boundaries PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME character_membership_boundaries COMMAND test_character_membership_boundaries)
    set_tests_properties(character_membership_boundaries PROPERTIES TIMEOUT 60)
endif()

foreach(name ability_membership_protocol ability_membership_controller ability_receiver_cleanup)
    set(target "test_${name}")
    add_executable(${target} "${MT2MP_SOURCE_ROOT}/test/${target}.cpp")
    if(NOT name STREQUAL "ability_receiver_cleanup")
        target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
    endif()
    target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME ${name} COMMAND ${target})
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endforeach()
if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ABILITY_MEMBERSHIP_BOUNDARIES "${CMAKE_CURRENT_BINARY_DIR}/ability_membership_boundaries")
    add_custom_command(OUTPUT "${ABILITY_MEMBERSHIP_BOUNDARIES}/native_ui_boundaries.inc"
        COMMAND "${Python3_EXECUTABLE}" -B -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_ability_membership_boundaries.py" "${MT2MP_SOURCE_ROOT}" "${ABILITY_MEMBERSHIP_BOUNDARIES}"
        DEPENDS "${MT2MP_SOURCE_ROOT}/test/extract_ability_membership_boundaries.py"
            "${MT2MP_SOURCE_ROOT}/test/ability_membership_boundaries.json"
            "${MT2MP_SOURCE_ROOT}/src/game/ability_membership_runtime.inc" "${MT2MP_SOURCE_ROOT}/src/game/ability_membership_codes.h"
        VERBATIM)
    add_executable(test_ability_membership_boundaries "${MT2MP_SOURCE_ROOT}/test/test_native_ui_boundaries.cpp" "${ABILITY_MEMBERSHIP_BOUNDARIES}/native_ui_boundaries.inc")
    target_include_directories(test_ability_membership_boundaries PRIVATE "${ABILITY_MEMBERSHIP_BOUNDARIES}")
    target_compile_features(test_ability_membership_boundaries PRIVATE cxx_std_20)
    target_compile_definitions(test_ability_membership_boundaries PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(test_ability_membership_boundaries PRIVATE mt2mp_boundary_minhook)
    if(MSVC)
        target_compile_options(test_ability_membership_boundaries PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME ability_membership_boundaries COMMAND test_ability_membership_boundaries)
    set_tests_properties(ability_membership_boundaries PROPERTIES TIMEOUT 60)
endif()

foreach(name dungeon_party_census party_saved_identity party_snapshot_companion dungeon_instance_live dungeon_instance_lifecycle)
    add_executable(test_${name} "${MT2MP_SOURCE_ROOT}/test/test_${name}.cpp")
    target_include_directories(test_${name} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(test_${name} PRIVATE cxx_std_20)
    target_compile_definitions(test_${name} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(test_${name} PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME ${name} COMMAND test_${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endforeach()

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    foreach(name party_live party_record_decode party_toon_order)
        add_executable(test_${name} "${MT2MP_SOURCE_ROOT}/test/test_${name}.cpp")
        if(name STREQUAL "party_live")
            target_sources(test_${name} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/party_live.cpp")
        elseif(name STREQUAL "party_record_decode")
            target_sources(test_${name} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/record_decode.cpp")
        else()
            target_sources(test_${name} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/reference_map.cpp"
                "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/cohort.cpp")
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
endif()

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    foreach(name party_frame_state subscriber_append_batch)
        add_executable(test_${name} "${MT2MP_SOURCE_ROOT}/test/test_${name}.cpp")
        if(name STREQUAL "party_frame_state")
            target_sources(test_${name} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/scene_clone/state.cpp")
        elseif(name STREQUAL "subscriber_append_batch")
            foreach(unit additions payload record_decode reference_map cohort)
                target_sources(test_${name} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/${unit}.cpp")
            endforeach()
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
endif()

add_executable(test_scene_clone "${MT2MP_SOURCE_ROOT}/tests/test_scene_clone.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/scene_clone/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/scene_clone/plan.cpp")
target_include_directories(test_scene_clone PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_scene_clone PRIVATE cxx_std_20)
target_sources(test_scene_clone PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/object_identity/remove_runtime/copy/scenery_witness.cpp")
target_compile_definitions(test_scene_clone PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_scene_clone PRIVATE /utf-8 /EHsc)
endif()
add_test(NAME scene_clone COMMAND test_scene_clone)
set_tests_properties(scene_clone PROPERTIES TIMEOUT 60)

add_executable(test_costume_weapon "${MT2MP_SOURCE_ROOT}/test/test_costume_weapon.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
target_include_directories(test_costume_weapon PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_costume_weapon PRIVATE cxx_std_20)
target_compile_definitions(test_costume_weapon PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_costume_weapon PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME costume_weapon COMMAND test_costume_weapon)
set_tests_properties(costume_weapon PROPERTIES TIMEOUT 60)
set(COSTUME_WEAPON_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/costume_weapon")
add_custom_command(OUTPUT "${COSTUME_WEAPON_GENERATED}/costume_weapon_capture_body.inc"
    COMMAND "${Python3_EXECUTABLE}" -B -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_costume_weapon.py"
        --source "${MT2MP_SOURCE_ROOT}" --output "${COSTUME_WEAPON_GENERATED}"
    DEPENDS "${MT2MP_SOURCE_ROOT}/test/extract_costume_weapon.py"
        "${MT2MP_SOURCE_ROOT}/src/game/costume_edit_runtime.inc" VERBATIM)
add_executable(test_costume_weapon_capture "${MT2MP_SOURCE_ROOT}/test/test_costume_weapon_capture.cpp"
    "${COSTUME_WEAPON_GENERATED}/costume_weapon_capture_body.inc")
target_include_directories(test_costume_weapon_capture PRIVATE "${COSTUME_WEAPON_GENERATED}"
    "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_costume_weapon_capture PRIVATE cxx_std_20)
target_compile_definitions(test_costume_weapon_capture PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_costume_weapon_capture PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME costume_weapon_capture COMMAND test_costume_weapon_capture)
set_tests_properties(costume_weapon_capture PROPERTIES TIMEOUT 60)

add_executable(test_ability_adjustment "${MT2MP_SOURCE_ROOT}/test/test_ability_adjustment.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
target_include_directories(test_ability_adjustment PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_ability_adjustment PRIVATE cxx_std_20)
target_compile_definitions(test_ability_adjustment PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_ability_adjustment PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME ability_adjustment COMMAND test_ability_adjustment)
set_tests_properties(ability_adjustment PROPERTIES TIMEOUT 60)
set(ABILITY_ADJUSTMENT_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/ability_adjustment")
add_custom_command(OUTPUT "${ABILITY_ADJUSTMENT_GENERATED}/ability_adjustment_capture_body.inc" "${ABILITY_ADJUSTMENT_GENERATED}/ability_adjustment_tail_body.inc" "${ABILITY_ADJUSTMENT_GENERATED}/ability_adjustment_record_body.inc"
    COMMAND "${Python3_EXECUTABLE}" -B -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_ability_adjustment.py" --source "${MT2MP_SOURCE_ROOT}" --output "${ABILITY_ADJUSTMENT_GENERATED}"
    DEPENDS "${MT2MP_SOURCE_ROOT}/test/extract_ability_adjustment.py" "${MT2MP_SOURCE_ROOT}/src/game/ability_adjustment_runtime.inc" "${MT2MP_SOURCE_ROOT}/src/game/ability_edit_runtime.inc" VERBATIM)
add_executable(test_ability_adjustment_capture "${MT2MP_SOURCE_ROOT}/test/test_ability_adjustment_capture.cpp"
    "${ABILITY_ADJUSTMENT_GENERATED}/ability_adjustment_capture_body.inc" "${ABILITY_ADJUSTMENT_GENERATED}/ability_adjustment_tail_body.inc" "${ABILITY_ADJUSTMENT_GENERATED}/ability_adjustment_record_body.inc")
target_include_directories(test_ability_adjustment_capture PRIVATE "${ABILITY_ADJUSTMENT_GENERATED}" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_ability_adjustment_capture PRIVATE cxx_std_20)
target_compile_definitions(test_ability_adjustment_capture PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_ability_adjustment_capture PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME ability_adjustment_capture COMMAND test_ability_adjustment_capture)
set_tests_properties(ability_adjustment_capture PROPERTIES TIMEOUT 60)
