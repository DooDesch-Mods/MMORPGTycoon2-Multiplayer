if(WIN32)
    add_executable(test_record_api_resolver
        "${MT2MP_SOURCE_ROOT}/test/test_record_api_resolver.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/append_calls.cpp")
    target_include_directories(test_record_api_resolver PRIVATE "${MT2MP_SOURCE_ROOT}/src/game")
    target_compile_features(test_record_api_resolver PRIVATE cxx_std_20)
    target_compile_definitions(test_record_api_resolver PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(test_record_api_resolver PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME record_api_resolver COMMAND test_record_api_resolver)
    set_tests_properties(record_api_resolver PROPERTIES TIMEOUT 30)
endif()
