if(WIN32 AND MSVC)
set(ORDERED_ABILITY_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/ordered_ability")
set(ORDERED_ABILITY_OUTPUTS
    ability_ordered_helpers_actual.inc ability_same_local_actual.inc ability_record_actual.inc ability_tick_actual.inc
    ability_perform_actual.inc ability_observe_actual.inc ability_capture_actual.inc ability_adjustment_calls_actual.inc
    ability_adjustment_perform_actual.inc ability_view_tail_actual.inc ability_parent_capture_binding_actual.inc ability_types_actual.inc)
list(TRANSFORM ORDERED_ABILITY_OUTPUTS PREPEND "${ORDERED_ABILITY_GENERATED}/")
add_custom_command(OUTPUT ${ORDERED_ABILITY_OUTPUTS}
    COMMAND "${Python3_EXECUTABLE}" -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_ordered_ability.py" --source "${MT2MP_SOURCE_ROOT}" --output "${ORDERED_ABILITY_GENERATED}"
    DEPENDS "${MT2MP_SOURCE_ROOT}/test/extract_ordered_ability.py" "${MT2MP_SOURCE_ROOT}/src/game/ability_edit_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/ability_adjustment_runtime.inc" "${MT2MP_SOURCE_ROOT}/src/game/character_rename_runtime.inc"
    VERBATIM)
add_custom_target(ordered_ability_extract DEPENDS ${ORDERED_ABILITY_OUTPUTS})
add_executable(test_ordered_ability_adjustment "${MT2MP_SOURCE_ROOT}/test/test_ordered_ability_adjustment.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/tick_authority.cpp")
add_dependencies(test_ordered_ability_adjustment ordered_ability_extract)
target_include_directories(test_ordered_ability_adjustment PRIVATE "${ORDERED_ABILITY_GENERATED}" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_ordered_ability_adjustment PRIVATE cxx_std_20)
target_compile_definitions(test_ordered_ability_adjustment PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(test_ordered_ability_adjustment PRIVATE /W4 /WX /utf-8 /EHsc /bigobj)
target_link_options(test_ordered_ability_adjustment PRIVATE /STACK:16777216)
target_link_libraries(test_ordered_ability_adjustment PRIVATE source_check_codecs_windows bcrypt)
add_test(NAME ordered_ability_adjustment COMMAND test_ordered_ability_adjustment)
set_tests_properties(ordered_ability_adjustment PROPERTIES TIMEOUT 60)
endif()
