if(WIN32)
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(weapon_visual_dir "${MT2MP_SOURCE_ROOT}/src/game/toon_weapon_visual")
set(weapon_visual_generated "${CMAKE_CURRENT_BINARY_DIR}/toon_weapon_visual_generated")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_toon_weapon_hook.py"
    --source-root "${MT2MP_SOURCE_ROOT}" --output "${weapon_visual_generated}/toon_weapon_equip_body.inc"
    RESULT_VARIABLE weapon_visual_extract)
if(NOT weapon_visual_extract EQUAL 0)
    message(FATAL_ERROR "Actual Toon Equip boundary extraction failed")
endif()
add_executable(test_toon_weapon_visual "${MT2MP_SOURCE_ROOT}/test/test_toon_weapon_visual.cpp"
    "${weapon_visual_dir}/reader.cpp" "${weapon_visual_dir}/orchestration.cpp")
add_executable(test_toon_weapon_hook "${MT2MP_SOURCE_ROOT}/test/test_toon_weapon_hook.cpp"
    "${weapon_visual_dir}/orchestration.cpp")
foreach(target test_toon_weapon_visual test_toon_weapon_hook)
    target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${weapon_visual_generated}")
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    add_test(NAME ${target} COMMAND ${target})
endforeach()
endif()
