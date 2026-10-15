find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_startingpoint_fixture/activity_read.inc")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_tutorial_staff_activity.py"
    --source-root "${MT2MP_SOURCE_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/staff_activity_extracted"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_tutorial_staff_activity "${MT2MP_SOURCE_ROOT}/test/test_tutorial_staff_activity.cpp")
target_include_directories(test_tutorial_staff_activity PRIVATE "${MT2MP_SOURCE_ROOT}/src/game"
    "${MT2MP_SOURCE_ROOT}/include" "${CMAKE_CURRENT_BINARY_DIR}/staff_activity_extracted")
target_compile_features(test_tutorial_staff_activity PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_tutorial_staff_activity PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME tutorial_staff_activity COMMAND test_tutorial_staff_activity)
set_tests_properties(tutorial_staff_activity PROPERTIES TIMEOUT 60)
