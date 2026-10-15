find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_startingpoint_fixture/research_ui_read.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_startingpoint_fixture/client_research_fixture.inc")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_tutorial_staff_ui.py"
    --source-root "${MT2MP_SOURCE_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/staff_ui_extracted"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_tutorial_client_staff_ui "${MT2MP_SOURCE_ROOT}/test/test_tutorial_client_staff_ui.cpp")
target_include_directories(test_tutorial_client_staff_ui PRIVATE "${MT2MP_SOURCE_ROOT}/src/game"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_startingpoint_fixture" "${MT2MP_SOURCE_ROOT}/include"
    "${CMAKE_CURRENT_BINARY_DIR}/staff_ui_extracted")
target_compile_features(test_tutorial_client_staff_ui PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_tutorial_client_staff_ui PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME tutorial_client_staff_ui COMMAND test_tutorial_client_staff_ui)
set_tests_properties(tutorial_client_staff_ui PROPERTIES TIMEOUT 60)
