if(WIN32)
set(weapon_probe_dir "${MT2MP_SOURCE_ROOT}/src/game/toon_weapon_visual")
add_executable(test_toon_weapon_probe "${MT2MP_SOURCE_ROOT}/test/test_toon_weapon_probe.cpp"
    "${weapon_probe_dir}/reader.cpp" "${weapon_probe_dir}/orchestration.cpp" "${weapon_probe_dir}/probe.cpp")
add_executable(test_toon_weapon_probe_runtime "${MT2MP_SOURCE_ROOT}/test/test_toon_weapon_probe_runtime.cpp"
    "${weapon_probe_dir}/reader.cpp" "${weapon_probe_dir}/probe.cpp")
foreach(target test_toon_weapon_probe test_toon_weapon_probe_runtime)
    target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    add_test(NAME ${target} COMMAND ${target})
endforeach()
endif()
