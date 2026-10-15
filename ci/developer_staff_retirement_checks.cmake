if(WIN32)
    add_executable(test_developer_retirement
        "${MT2MP_SOURCE_ROOT}/test/test_developer_retirement.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/retirement/retire.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/read.cpp")
    add_library(developer_retirement_native_compile OBJECT
        "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/retirement/retire_calls.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/retirement/retire_process.cpp")
    foreach(target test_developer_retirement developer_retirement_native_compile)
        target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/retirement" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
        target_compile_features(${target} PRIVATE cxx_std_20)
        target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
        endif()
    endforeach()
    add_test(NAME developer_retirement COMMAND test_developer_retirement)
    set_tests_properties(developer_retirement PROPERTIES TIMEOUT 60)
endif()
