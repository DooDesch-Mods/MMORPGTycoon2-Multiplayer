if(WIN32)
    add_executable(test_chat_ordered
        "${MT2MP_SOURCE_ROOT}/tools/chat_slice/test_chat.cpp"
        "${MT2MP_SOURCE_ROOT}/src/net/chat_core.cpp"
        "${MT2MP_SOURCE_ROOT}/src/net/chat_protocol.cpp"
        "${MT2MP_SOURCE_ROOT}/src/net/chat_runtime.cpp")
    target_include_directories(test_chat_ordered PRIVATE "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(test_chat_ordered PRIVATE cxx_std_20)
    target_compile_definitions(test_chat_ordered PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(test_chat_ordered PRIVATE /W4 /WX /utf-8 /EHsc /UNDEBUG)
    endif()
    add_test(NAME chat_ordered COMMAND test_chat_ordered)
    set_tests_properties(chat_ordered PROPERTIES TIMEOUT 60)
endif()
