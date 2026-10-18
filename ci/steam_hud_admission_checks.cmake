if(WIN32)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(_steam_hud_generated "${CMAKE_CURRENT_BINARY_DIR}/steam_hud_admission_generated")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_steam_hud_admission.py"
    --root "${MT2MP_SOURCE_ROOT}" --output "${_steam_hud_generated}" COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/test/extract_steam_hud_admission.py"
    "${MT2MP_SOURCE_ROOT}/src/game/hud_time_runtime.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/hud_calendar_runtime.cpp")
add_executable(test_steam_hud_admission "${MT2MP_SOURCE_ROOT}/test/test_steam_hud_admission.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/hud_calendar_transport.cpp")
target_include_directories(test_steam_hud_admission PRIVATE
    "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${_steam_hud_generated}")
target_compile_features(test_steam_hud_admission PRIVATE cxx_std_20)
target_compile_definitions(test_steam_hud_admission PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_steam_hud_admission PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME steam_hud_admission COMMAND test_steam_hud_admission)
set_tests_properties(steam_hud_admission PROPERTIES TIMEOUT 60)
endif()
