add_executable(test_general_project
    "${MT2MP_SOURCE_ROOT}/test/test_general_project.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/network/node_state.cpp")
target_include_directories(test_general_project PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_general_project PRIVATE cxx_std_20)
target_compile_definitions(test_general_project PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_general_project PRIVATE /utf-8 /EHsc)
endif()
add_test(NAME general_project COMMAND test_general_project)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(project_generated "${CMAKE_CURRENT_BINARY_DIR}/general-project-generated")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_general_project.py" --source-root "${MT2MP_SOURCE_ROOT}" --output "${project_generated}" RESULT_VARIABLE project_extract)
if(NOT project_extract EQUAL 0)
    message(FATAL_ERROR "General project actual callback extraction failed")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MT2MP_SOURCE_ROOT}/src/game/general_project_runtime.cpp" "${MT2MP_SOURCE_ROOT}/test/extract_general_project.py")
add_executable(test_general_project_native "${MT2MP_SOURCE_ROOT}/test/test_general_project_native.cpp")
target_include_directories(test_general_project_native PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${project_generated}")
target_compile_features(test_general_project_native PRIVATE cxx_std_20)
target_compile_definitions(test_general_project_native PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_general_project_native PRIVATE /utf-8 /EHsc)
endif()
add_test(NAME general_project_native COMMAND test_general_project_native)
