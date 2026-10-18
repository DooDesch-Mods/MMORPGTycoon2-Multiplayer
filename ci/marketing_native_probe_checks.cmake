add_executable(test_marketing_native_probe
    "${MT2MP_SOURCE_ROOT}/test/test_marketing_native_probe.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/marketing_native_probe/contract.cpp")
target_include_directories(test_marketing_native_probe PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_marketing_native_probe PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_marketing_native_probe PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME marketing_native_probe COMMAND test_marketing_native_probe
    "${MT2MP_SOURCE_ROOT}/tools/developer_staff_slice/astra_vip_trigger/marketing_native_probe/marketing-target.json")
set_tests_properties(marketing_native_probe PROPERTIES TIMEOUT 30)
if(WIN32)
    add_executable(test_marketing_record_decode
        "${MT2MP_SOURCE_ROOT}/test/test_marketing_record_decode.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/record_decode.cpp")
    target_include_directories(test_marketing_record_decode PRIVATE "${MT2MP_SOURCE_ROOT}/src/game")
    target_compile_features(test_marketing_record_decode PRIVATE cxx_std_20)
    target_compile_definitions(test_marketing_record_decode PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(test_marketing_record_decode PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME marketing_record_decode COMMAND test_marketing_record_decode)
    set_tests_properties(marketing_record_decode PROPERTIES TIMEOUT 30)
endif()
