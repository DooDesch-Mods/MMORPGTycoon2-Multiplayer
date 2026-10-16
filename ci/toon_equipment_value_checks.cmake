add_executable(test_toon_equipment_value
    "${MT2MP_SOURCE_ROOT}/test/test_toon_equipment_value.cpp"
    ${type7_frame_sources})
target_include_directories(test_toon_equipment_value PRIVATE
    "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_toon_equipment_value PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_toon_equipment_value PRIVATE /W4 /WX /utf-8 /EHsc /Gy)
    target_link_options(test_toon_equipment_value PRIVATE /OPT:REF /INCREMENTAL:NO)
endif()
add_test(NAME toon_equipment_value COMMAND test_toon_equipment_value)
set_tests_properties(toon_equipment_value PROPERTIES TIMEOUT 60)
