if(WIN32 AND MSVC)
set(ABILITY_APPLY_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/ability_apply")
set(ABILITY_APPLY_OUTPUTS ability_apply_tails_actual.inc ability_apply_body_actual.inc ability_apply_capture_actual.inc ability_apply_disposal_actual.inc ability_proposal_actual.inc)
list(TRANSFORM ABILITY_APPLY_OUTPUTS PREPEND "${ABILITY_APPLY_GENERATED}/")
add_custom_command(OUTPUT ${ABILITY_APPLY_OUTPUTS}
    COMMAND "${Python3_EXECUTABLE}" -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_ability_apply.py"
        --source "${MT2MP_SOURCE_ROOT}" --output "${ABILITY_APPLY_GENERATED}"
    DEPENDS "${MT2MP_SOURCE_ROOT}/test/extract_ability_apply.py" "${MT2MP_SOURCE_ROOT}/src/game/ability_edit_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/ability_apply_native.inc" "${MT2MP_SOURCE_ROOT}/src/game/ability_apply_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/ability_proposal_lifetime.inc"
    VERBATIM)
add_custom_target(ability_apply_extract DEPENDS ${ABILITY_APPLY_OUTPUTS})
add_executable(test_ordered_ability_apply "${MT2MP_SOURCE_ROOT}/test/test_ordered_ability_apply.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/tick_authority.cpp")
add_dependencies(test_ordered_ability_apply ability_apply_extract ordered_ability_extract)
target_include_directories(test_ordered_ability_apply PRIVATE "${ABILITY_APPLY_GENERATED}" "${ORDERED_ABILITY_GENERATED}"
    "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_ordered_ability_apply PRIVATE cxx_std_20)
target_compile_definitions(test_ordered_ability_apply PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(test_ordered_ability_apply PRIVATE /W4 /WX /utf-8 /EHsc /bigobj)
target_link_options(test_ordered_ability_apply PRIVATE /STACK:16777216)
target_link_libraries(test_ordered_ability_apply PRIVATE source_check_codecs_windows bcrypt)
add_test(NAME ordered_ability_apply COMMAND test_ordered_ability_apply)
set_tests_properties(ordered_ability_apply PROPERTIES TIMEOUT 60)
add_executable(test_ability_proposal_lifetime "${MT2MP_SOURCE_ROOT}/test/test_ability_proposal_lifetime.cpp")
add_dependencies(test_ability_proposal_lifetime ability_apply_extract)
target_include_directories(test_ability_proposal_lifetime PRIVATE "${ABILITY_APPLY_GENERATED}")
target_compile_features(test_ability_proposal_lifetime PRIVATE cxx_std_20)
target_compile_definitions(test_ability_proposal_lifetime PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(test_ability_proposal_lifetime PRIVATE /W4 /WX /utf-8 /EHsc)
add_test(NAME ability_proposal_lifetime COMMAND test_ability_proposal_lifetime)
set_tests_properties(ability_proposal_lifetime PROPERTIES TIMEOUT 60)
endif()
