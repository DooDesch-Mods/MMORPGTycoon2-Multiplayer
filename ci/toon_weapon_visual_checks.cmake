if(WIN32)
    set(weapon_visual_dir "${MT2MP_SOURCE_ROOT}/src/game/toon_weapon_visual")
    add_executable(test_toon_weapon_visual "${MT2MP_SOURCE_ROOT}/test/test_toon_weapon_visual.cpp"
        "${weapon_visual_dir}/reader.cpp" "${weapon_visual_dir}/orchestration.cpp")
    target_include_directories(test_toon_weapon_visual PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(test_toon_weapon_visual PRIVATE cxx_std_20)
    target_compile_options(test_toon_weapon_visual PRIVATE /W4 /WX /utf-8 /EHsc)
    add_test(NAME test_toon_weapon_visual COMMAND test_toon_weapon_visual)
endif()
