if(WIN32 AND MSVC)
set(ORDERED_COSTUME_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/ordered_costume")
set(ORDERED_COSTUME_OUTPUTS costume_types_actual.inc costume_record_actual.inc costume_helpers_actual.inc costume_capture_actual.inc
    costume_tick_actual.inc costume_perform_actual.inc costume_observe_actual.inc costume_parent_capture_binding_actual.inc)
list(TRANSFORM ORDERED_COSTUME_OUTPUTS PREPEND "${ORDERED_COSTUME_GENERATED}/")
add_custom_command(OUTPUT ${ORDERED_COSTUME_OUTPUTS}
    COMMAND "${Python3_EXECUTABLE}" -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_ordered_costume.py" --source "${MT2MP_SOURCE_ROOT}" --output "${ORDERED_COSTUME_GENERATED}"
    DEPENDS "${MT2MP_SOURCE_ROOT}/test/extract_ordered_costume.py" "${MT2MP_SOURCE_ROOT}/src/game/costume_edit_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/character_rename_runtime.inc" VERBATIM)
add_custom_target(ordered_costume_extract DEPENDS ${ORDERED_COSTUME_OUTPUTS})
add_executable(test_ordered_costume_weapon "${MT2MP_SOURCE_ROOT}/test/test_ordered_costume_weapon.cpp" "${MT2MP_SOURCE_ROOT}/src/game/typed_action/tick_authority.cpp")
add_dependencies(test_ordered_costume_weapon ordered_costume_extract)
target_include_directories(test_ordered_costume_weapon PRIVATE "${ORDERED_COSTUME_GENERATED}" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_ordered_costume_weapon PRIVATE cxx_std_20)
target_compile_definitions(test_ordered_costume_weapon PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(test_ordered_costume_weapon PRIVATE /W4 /WX /utf-8 /EHsc /bigobj)
target_link_options(test_ordered_costume_weapon PRIVATE /STACK:16777216)
target_link_libraries(test_ordered_costume_weapon PRIVATE source_check_codecs_windows bcrypt)
add_test(NAME ordered_costume_weapon COMMAND test_ordered_costume_weapon)
set_tests_properties(ordered_costume_weapon PROPERTIES TIMEOUT 60)
endif()
