if(WIN32)
    add_executable(test_toon_weapon_records
        "${MT2MP_SOURCE_ROOT}/test/test_toon_weapon_records.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/record_decode.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/native_calls.cpp")
    target_include_directories(test_toon_weapon_records PRIVATE "${MT2MP_SOURCE_ROOT}/src/game")
    target_compile_definitions(test_toon_weapon_records PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(test_toon_weapon_records PRIVATE /W4 /WX /utf-8)
    endif()
    add_test(NAME toon_weapon_records COMMAND test_toon_weapon_records)
    set_tests_properties(toon_weapon_records PROPERTIES TIMEOUT 30)
    add_executable(test_toon_weapon_native
        "${MT2MP_SOURCE_ROOT}/test/test_toon_weapon_native.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/record_decode.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/native_calls.cpp")
    target_include_directories(test_toon_weapon_native PRIVATE "${MT2MP_SOURCE_ROOT}/src/game")
    target_compile_definitions(test_toon_weapon_native PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(test_toon_weapon_native PRIVATE /W4 /WX /utf-8)
    endif()
    add_test(NAME toon_weapon_native COMMAND test_toon_weapon_native)
    set_tests_properties(toon_weapon_native PROPERTIES TIMEOUT 30)
endif()
