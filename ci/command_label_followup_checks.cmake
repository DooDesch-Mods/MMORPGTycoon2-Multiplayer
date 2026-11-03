if(WIN32 AND MSVC)
    set(COMMAND_LABEL_FOLLOWUP "${CMAKE_CURRENT_BINARY_DIR}/command_label_followup")
    set(COMMAND_LABEL_FOLLOWUP_OUTPUTS quest_command_gate_actual.inc selection_label_actual.inc appearance_command_gate_actual.inc dungeon_tab_command_actual.inc)
    list(TRANSFORM COMMAND_LABEL_FOLLOWUP_OUTPUTS PREPEND "${COMMAND_LABEL_FOLLOWUP}/")
    add_custom_command(OUTPUT ${COMMAND_LABEL_FOLLOWUP_OUTPUTS}
        COMMAND "${Python3_EXECUTABLE}" -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_command_label_followup.py" --source "${MT2MP_SOURCE_ROOT}" --output "${COMMAND_LABEL_FOLLOWUP}"
        DEPENDS "${MT2MP_SOURCE_ROOT}/test/extract_command_label_followup.py"
            "${MT2MP_SOURCE_ROOT}/src/game/npc_quest_list_runtime.inc" "${MT2MP_SOURCE_ROOT}/src/game/dungeon_config_ui.inc"
            "${MT2MP_SOURCE_ROOT}/src/game/region_styles_runtime.inc" "${MT2MP_SOURCE_ROOT}/src/game/region_appearance_runtime.inc"
        VERBATIM)
    add_executable(test_command_label_followup "${MT2MP_SOURCE_ROOT}/test/test_command_label_followup.cpp" ${COMMAND_LABEL_FOLLOWUP_OUTPUTS})
    add_dependencies(test_command_label_followup token_label_capture_extract)
    target_include_directories(test_command_label_followup PRIVATE "${COMMAND_LABEL_FOLLOWUP}" "${TOKEN_LABEL_GENERATED}" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(test_command_label_followup PRIVATE cxx_std_20)
    target_compile_definitions(test_command_label_followup PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(test_command_label_followup PRIVATE /W4 /WX /utf-8 /EHsc /bigobj)
    add_test(NAME command_label_followup COMMAND test_command_label_followup)
    set_tests_properties(command_label_followup PROPERTIES TIMEOUT 60)
endif()
