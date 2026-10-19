if(WIN32)
    add_executable(test_empty_network
        "${MT2MP_SOURCE_ROOT}/test/test_empty_network.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/native_network.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/network_identity.cpp")
    target_include_directories(test_empty_network PRIVATE "${MT2MP_SOURCE_ROOT}/include" "${MT2MP_SOURCE_ROOT}/include/vendor" "${MT2MP_SOURCE_ROOT}/src/game")
    target_compile_definitions(test_empty_network PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX MT2MP_NETWORK_TEST MT2MP_NETWORK_IDENTITY_TEST)
    target_compile_features(test_empty_network PRIVATE cxx_std_20)
    target_link_libraries(test_empty_network PRIVATE bcrypt)
    if(MSVC)
        target_compile_options(test_empty_network PRIVATE /W4 /WX /utf-8)
    endif()
    add_test(NAME empty_network COMMAND test_empty_network)
    set_tests_properties(empty_network PROPERTIES TIMEOUT 60)
endif()
