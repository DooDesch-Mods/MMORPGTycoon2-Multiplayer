find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(TARGET control_session_checks)
    set(_pending_binding_session control_session_checks)
elseif(TARGET world_edit_session_checks)
    set(_pending_binding_session world_edit_session_checks)
else()
    include("${CMAKE_CURRENT_LIST_DIR}/control_checks.cmake")
    set(_pending_binding_session control_session_checks)
endif()
set(_pending_binding_generated "${CMAKE_CURRENT_BINARY_DIR}/pending_binding_generated")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action_runtime.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${MT2MP_SOURCE_ROOT}/test/native_state_regression/pending_binding/check.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B -X utf8
    "${MT2MP_SOURCE_ROOT}/test/native_state_regression/pending_binding/check.py"
    --source "${MT2MP_SOURCE_ROOT}" --output "${_pending_binding_generated}" COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_pending_binding "${MT2MP_SOURCE_ROOT}/test/native_state_regression/pending_binding/test_pending_binding.cpp")
target_include_directories(test_pending_binding PRIVATE "${_pending_binding_generated}" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_pending_binding PRIVATE cxx_std_20)
target_link_libraries(test_pending_binding PRIVATE ${_pending_binding_session} source_check_codecs_windows bcrypt)
if(MSVC)
    target_compile_options(test_pending_binding PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME pending_binding COMMAND test_pending_binding)
set_tests_properties(pending_binding PROPERTIES TIMEOUT 60)
