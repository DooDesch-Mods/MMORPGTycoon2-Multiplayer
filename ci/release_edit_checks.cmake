add_executable(test_release_edit
    "${MT2MP_SOURCE_ROOT}/test/test_release_edit.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/network/node_state.cpp")
target_include_directories(test_release_edit PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_release_edit PRIVATE cxx_std_20)
target_compile_definitions(test_release_edit PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_release_edit PRIVATE /utf-8 /EHsc)
endif()
add_test(NAME release_edit COMMAND test_release_edit)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(release_generated "${CMAKE_CURRENT_BINARY_DIR}/release-edit-generated")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_release_edit.py" --source-root "${MT2MP_SOURCE_ROOT}" --output "${release_generated}" RESULT_VARIABLE release_extract)
if(NOT release_extract EQUAL 0)
    message(FATAL_ERROR "Release actual ownership/cleanup extraction failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MT2MP_SOURCE_ROOT}/src/game/release_runtime.cpp" "${MT2MP_SOURCE_ROOT}/test/extract_release_edit.py" "${MT2MP_SOURCE_ROOT}/src/game/general_project_runtime.cpp")
add_executable(test_release_edit_native "${MT2MP_SOURCE_ROOT}/test/test_release_edit_native.cpp")
target_include_directories(test_release_edit_native PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${release_generated}")
target_compile_features(test_release_edit_native PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_release_edit_native PRIVATE /utf-8 /EHsc)
endif()
add_test(NAME release_edit_native COMMAND test_release_edit_native)
