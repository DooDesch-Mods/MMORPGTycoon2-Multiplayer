add_executable(test_character_calendar "${MT2MP_SOURCE_ROOT}/test/test_character_calendar.cpp")
target_include_directories(test_character_calendar PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_character_calendar PRIVATE cxx_std_20)
target_compile_definitions(test_character_calendar PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_character_calendar PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME character_calendar COMMAND test_character_calendar)
set_tests_properties(character_calendar PROPERTIES TIMEOUT 60)

# Exercise the production Calendar/Subscriber paired reader with controlled native inputs.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/character_calendar_native.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/character_calendar_native.h"
    "${MT2MP_SOURCE_ROOT}/src/game/management_live_port.inc"
    "${MT2MP_SOURCE_ROOT}/test/extract_calendar_demand_capture.py"
    "${MT2MP_SOURCE_ROOT}/test/calendar_demand_capture_cases.inc")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_calendar_demand_capture.py"
    --root "${MT2MP_SOURCE_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}"
    COMMAND_ERROR_IS_FATAL ANY)
target_include_directories(test_character_calendar PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
