if(WIN32)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    if(NOT PLACEMENT_READER_DEPENDENCY_ROOT)
        get_filename_component(PLACEMENT_READER_DEPENDENCY_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    endif()
    get_filename_component(_placement_reader_source "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
    set(_placement_reader_generated "${CMAKE_CURRENT_BINARY_DIR}/placement_reader_extracted")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${_placement_reader_source}/src/game/developer_staff/native_read.cpp"
        "${_placement_reader_source}/src/game/developer_staff/construction_native.cpp"
        "${_placement_reader_source}/src/game/tutorial_replication/staff_display_owner.inc"
        "${_placement_reader_source}/src/game/bug_authority/hooks.cpp"
        "${_placement_reader_source}/src/game/bug_authority/runtime.cpp"
        "${_placement_reader_source}/src/game/bug_authority/work_runtime.cpp"
        "${_placement_reader_source}/src/game/developer_staff/action_native.cpp"
        "${_placement_reader_source}/test/extract_placement_reader_scope.py")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${_placement_reader_source}/test/extract_placement_reader_scope.py"
        --root "${_placement_reader_source}" --dependencies "${PLACEMENT_READER_DEPENDENCY_ROOT}"
        --output "${_placement_reader_generated}" COMMAND_ERROR_IS_FATAL ANY)
    add_executable(test_placement_reader_scope "${_placement_reader_source}/test/test_placement_reader_scope.cpp"
        "${PLACEMENT_READER_DEPENDENCY_ROOT}/src/game/developer_staff/read.cpp"
        "${PLACEMENT_READER_DEPENDENCY_ROOT}/src/game/developer_staff/job_census.cpp")
    target_compile_features(test_placement_reader_scope PRIVATE cxx_std_20)
    target_compile_options(test_placement_reader_scope PRIVATE /W4 /WX /permissive- /utf-8 /EHsc)
    target_include_directories(test_placement_reader_scope PRIVATE "${_placement_reader_generated}"
        "${PLACEMENT_READER_DEPENDENCY_ROOT}/src/game" "${PLACEMENT_READER_DEPENDENCY_ROOT}/include")
    add_test(NAME placement_reader_scope COMMAND test_placement_reader_scope)
endif()
