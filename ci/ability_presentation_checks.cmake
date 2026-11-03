if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ABILITY_PRESENTATION "${CMAKE_CURRENT_BINARY_DIR}/ability_presentation")
    add_custom_command(OUTPUT "${ABILITY_PRESENTATION}/ability_presentation_actual.inc" "${ABILITY_PRESENTATION}/ability_presentation_binding_actual.inc"
        COMMAND "${Python3_EXECUTABLE}" -B -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_ability_presentation.py" "${MT2MP_SOURCE_ROOT}" "${ABILITY_PRESENTATION}"
        DEPENDS "${MT2MP_SOURCE_ROOT}/test/extract_ability_presentation.py"
            "${MT2MP_SOURCE_ROOT}/src/game/ability_membership_runtime.inc" "${MT2MP_SOURCE_ROOT}/src/game/ability_membership_presentation.inc" "${MT2MP_SOURCE_ROOT}/src/game/typed_action_runtime.cpp"
        VERBATIM)
    add_executable(test_ability_presentation "${MT2MP_SOURCE_ROOT}/test/test_ability_presentation.cpp" "${ABILITY_PRESENTATION}/ability_presentation_actual.inc")
    target_include_directories(test_ability_presentation PRIVATE "${ABILITY_PRESENTATION}" "${MT2MP_SOURCE_ROOT}/src/game")
    target_compile_features(test_ability_presentation PRIVATE cxx_std_20)
    target_compile_definitions(test_ability_presentation PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(test_ability_presentation PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME ability_presentation COMMAND test_ability_presentation)
    set_tests_properties(ability_presentation PROPERTIES TIMEOUT 60)
endif()
