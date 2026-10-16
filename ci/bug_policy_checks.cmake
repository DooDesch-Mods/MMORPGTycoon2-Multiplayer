if(WIN32)
    foreach(group ui read)
        add_executable(test_bug_policy_${group}
            "${MT2MP_SOURCE_ROOT}/test/test_bug_policy_${group}.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/bug_policy.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/read.cpp")
        target_compile_features(test_bug_policy_${group} PRIVATE cxx_std_20)
        target_compile_options(test_bug_policy_${group} PRIVATE /W4 /WX /utf-8 /EHsc)
        target_compile_definitions(test_bug_policy_${group} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        target_include_directories(test_bug_policy_${group} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
        add_test(NAME bug_policy_${group} COMMAND test_bug_policy_${group})
    endforeach()
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME bug_policy_sdk COMMAND "${Python3_EXECUTABLE}" -X utf8 "${MT2MP_SOURCE_ROOT}/test/mcp/test_bug_policy.py")
    set_tests_properties(bug_policy_ui bug_policy_read bug_policy_sdk PROPERTIES TIMEOUT 60)
endif()
