set(research_canonical_generated "${CMAKE_CURRENT_BINARY_DIR}/research-canonical-generated")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_general_project_canonical.py"
    --source-root "${MT2MP_SOURCE_ROOT}" --output "${research_canonical_generated}"
    COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/ordered_runtime.h"
    "${MT2MP_SOURCE_ROOT}/test/extract_general_project_canonical.py")
add_executable(test_general_project_ordered_canonical
    "${MT2MP_SOURCE_ROOT}/test/test_general_project_ordered_canonical.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/tick_execution.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/tick_authority.cpp")
target_link_libraries(test_general_project_ordered_canonical PRIVATE source_check_codecs_windows bcrypt)
target_include_directories(test_general_project_ordered_canonical PRIVATE
    "${research_canonical_generated}" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_general_project_ordered_canonical PRIVATE cxx_std_20)
target_compile_definitions(test_general_project_ordered_canonical PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_general_project_ordered_canonical PRIVATE /W4 /WX /utf-8 /EHsc /bigobj)
    target_link_options(test_general_project_ordered_canonical PRIVATE /STACK:8388608)
endif()
add_test(NAME general_project_ordered_canonical COMMAND test_general_project_ordered_canonical)
set_tests_properties(general_project_ordered_canonical PROPERTIES TIMEOUT 60 LABELS ordered)
