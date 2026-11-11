add_executable(test_tutorial_movement_late
    "${MT2MP_SOURCE_ROOT}/test/test_tutorial_movement_late.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_movement/movement_input.cpp")
target_include_directories(test_tutorial_movement_late PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/tutorial_movement")
target_compile_features(test_tutorial_movement_late PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_tutorial_movement_late PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME tutorial_movement_late COMMAND test_tutorial_movement_late)
set_tests_properties(tutorial_movement_late PROPERTIES TIMEOUT 60)
