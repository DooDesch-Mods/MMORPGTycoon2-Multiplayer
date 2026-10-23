get_filename_component(STARTUP_CALENDAR_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
add_executable(test_startup_calendar_normalization
    "${STARTUP_CALENDAR_ROOT}/test/test_startup_calendar_normalization.cpp")
target_compile_features(test_startup_calendar_normalization PRIVATE cxx_std_20)
target_include_directories(test_startup_calendar_normalization PRIVATE "${STARTUP_CALENDAR_ROOT}/src/game")
if(MSVC)
    target_compile_options(test_startup_calendar_normalization PRIVATE /W4 /WX /permissive-)
endif()
add_test(NAME startup_calendar_normalization COMMAND test_startup_calendar_normalization)
