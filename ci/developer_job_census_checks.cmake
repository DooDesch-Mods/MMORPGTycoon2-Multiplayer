if(WIN32)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/probe_process.cpp")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_developer_job_observe.py"
        --source "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/probe_process.cpp" --output "${CMAKE_CURRENT_BINARY_DIR}/developer_job_extracted"
        COMMAND_ERROR_IS_FATAL ANY)
    add_executable(test_developer_job_census "${MT2MP_SOURCE_ROOT}/test/test_developer_job_census.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/job_census.cpp")
    add_executable(test_developer_job_observe "${MT2MP_SOURCE_ROOT}/test/test_developer_job_observe.cpp")
    foreach(target test_developer_job_census test_developer_job_observe)
        target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${CMAKE_CURRENT_BINARY_DIR}/developer_job_extracted")
        target_compile_features(${target} PRIVATE cxx_std_20)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
        endif()
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(${target} PROPERTIES TIMEOUT 60)
    endforeach()
endif()
