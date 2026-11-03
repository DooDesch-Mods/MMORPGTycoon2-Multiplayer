set(TOKEN_LABEL_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/token_label_capture")
set(TOKEN_LABEL_OUTPUTS
    "${TOKEN_LABEL_GENERATED}/command_token_label_actual.inc"
    "${TOKEN_LABEL_GENERATED}/command_token_label_adapter_actual.inc"
    "${TOKEN_LABEL_GENERATED}/character_variant_capture_actual.inc"
    "${TOKEN_LABEL_GENERATED}/ability_new_capture_actual.inc"
    "${TOKEN_LABEL_GENERATED}/dungeon_sound_capture_label_actual.inc"
    "${TOKEN_LABEL_GENERATED}/dungeon_sound_replay_label_actual.inc"
    "${TOKEN_LABEL_GENERATED}/dungeon_gizmo_capture_label_actual.inc"
    "${TOKEN_LABEL_GENERATED}/dungeon_gizmo_replay_label_actual.inc")
add_custom_command(OUTPUT ${TOKEN_LABEL_OUTPUTS}
    COMMAND "${Python3_EXECUTABLE}" -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_token_label_capture.py"
        --source "${MT2MP_SOURCE_ROOT}" --output "${TOKEN_LABEL_GENERATED}"
    DEPENDS "${MT2MP_SOURCE_ROOT}/test/extract_token_label_capture.py"
        "${MT2MP_SOURCE_ROOT}/src/game/command_token_label.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/character_membership_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/ability_membership_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/costume_edit_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/dungeon_sound_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/dungeon_gizmo_runtime.inc"
    VERBATIM)
add_custom_target(token_label_capture_extract DEPENDS ${TOKEN_LABEL_OUTPUTS})
if(WIN32 AND MSVC)
add_executable(test_token_label_capture "${MT2MP_SOURCE_ROOT}/test/test_token_label_capture.cpp")
add_dependencies(test_token_label_capture token_label_capture_extract)
target_include_directories(test_token_label_capture PRIVATE "${TOKEN_LABEL_GENERATED}" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_token_label_capture PRIVATE cxx_std_20)
target_compile_definitions(test_token_label_capture PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(test_token_label_capture PRIVATE /W4 /WX /utf-8 /EHsc /bigobj)
add_test(NAME token_label_capture COMMAND test_token_label_capture)
set_tests_properties(token_label_capture PROPERTIES TIMEOUT 60)
endif()
add_dependencies(test_costume_weapon_capture token_label_capture_extract)
target_include_directories(test_costume_weapon_capture PRIVATE "${TOKEN_LABEL_GENERATED}")
