if(WIN32 AND MSVC)
set(CHARACTER_OFFERED_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/character_offered")
set(CHARACTER_OFFERED_OUTPUTS character_open_array_actual.inc character_offered_costume_driver_actual.inc
    character_offered_ability_driver_actual.inc character_offered_public_actual.inc character_offered_control_state_actual.inc character_offered_page_actual.inc character_offered_icons_actual.inc)
list(TRANSFORM CHARACTER_OFFERED_OUTPUTS PREPEND "${CHARACTER_OFFERED_GENERATED}/")
add_custom_command(OUTPUT ${CHARACTER_OFFERED_OUTPUTS}
    COMMAND "${Python3_EXECUTABLE}" -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_character_offered.py"
        --source "${MT2MP_SOURCE_ROOT}" --output "${CHARACTER_OFFERED_GENERATED}"
    DEPENDS "${MT2MP_SOURCE_ROOT}/test/extract_character_offered.py" "${MT2MP_SOURCE_ROOT}/src/game/character_rename_open.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/character_offered_ui.inc" "${MT2MP_SOURCE_ROOT}/src/game/character_offered_ability.inc"
    VERBATIM)
add_custom_target(character_offered_extract DEPENDS ${CHARACTER_OFFERED_OUTPUTS})
foreach(name controls driver queue pages icons)
    add_executable(test_character_offered_${name} "${MT2MP_SOURCE_ROOT}/test/test_character_offered_${name}.cpp")
    add_dependencies(test_character_offered_${name} character_offered_extract)
    target_include_directories(test_character_offered_${name} PRIVATE "${CHARACTER_OFFERED_GENERATED}" "${CHARACTER_RENAME_GENERATED}"
        "${MT2MP_SOURCE_ROOT}/test" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(test_character_offered_${name} PRIVATE cxx_std_20)
    target_compile_definitions(test_character_offered_${name} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(test_character_offered_${name} PRIVATE /W4 /WX /utf-8 /EHsc /bigobj)
    add_test(NAME character_offered_${name} COMMAND test_character_offered_${name})
    set_tests_properties(character_offered_${name} PROPERTIES TIMEOUT 60)
endforeach()
add_test(NAME character_edit_mcp COMMAND "${Python3_EXECUTABLE}" -B -X utf8
    "${MT2MP_SOURCE_ROOT}/test/test_character_edit_mcp.py" "${MT2MP_SOURCE_ROOT}/tools/mcp/server.py")
set_tests_properties(character_edit_mcp PROPERTIES TIMEOUT 60)
endif()
