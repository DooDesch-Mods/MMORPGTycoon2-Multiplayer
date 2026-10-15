set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MT2MP_SOURCE_ROOT}/src/game/tutorial_movement/movement_process.cpp")
execute_process(COMMAND python "${MT2MP_SOURCE_ROOT}/test/extract_tutorial_focus.py"
    --source "${MT2MP_SOURCE_ROOT}/src/game/tutorial_movement/movement_process.cpp"
    --output "${CMAKE_CURRENT_BINARY_DIR}/focus_bodies.inc" RESULT_VARIABLE focus_extraction)
if(NOT focus_extraction EQUAL 0)
    message(FATAL_ERROR "Actual tutorial focus API extraction failed")
endif()
add_executable(test_tutorial_movement
    "${MT2MP_SOURCE_ROOT}/test/test_tutorial_movement.cpp"
    "${MT2MP_SOURCE_ROOT}/test/test_tutorial_focus.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_movement/movement_input.cpp")
target_include_directories(test_tutorial_movement PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/tutorial_movement" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(test_tutorial_movement PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_tutorial_movement PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME tutorial_movement COMMAND test_tutorial_movement)
set_tests_properties(tutorial_movement PROPERTIES TIMEOUT 60)
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
