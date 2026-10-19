find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(time_item_fixture "${MT2MP_SOURCE_ROOT}/test/time_item_forwarding")
set(time_item_generated "${CMAKE_CURRENT_BINARY_DIR}/time_item_forwarding")
add_custom_command(OUTPUT "${time_item_generated}/time_item_hook.inc"
 COMMAND "${Python3_EXECUTABLE}" -X utf8 "${time_item_fixture}/extract_hook.py" --source "${MT2MP_SOURCE_ROOT}" --output "${time_item_generated}"
 DEPENDS "${time_item_fixture}/extract_hook.py" "${MT2MP_SOURCE_ROOT}/src/core/dll_main.cpp" "${MT2MP_SOURCE_ROOT}/src/game/control_input/control_hooks.inc"
 VERBATIM)
add_executable(time_item_forwarding "${time_item_fixture}/test_forwarding.cpp" "${time_item_generated}/time_item_hook.inc"
 "${MT2MP_SOURCE_ROOT}/src/game/hud_time_reader.cpp" "${MT2MP_SOURCE_ROOT}/src/game/hud_time_activation.cpp" "${MT2MP_SOURCE_ROOT}/src/game/time_session.cpp")
target_include_directories(time_item_forwarding PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${time_item_generated}")
target_compile_features(time_item_forwarding PRIVATE cxx_std_20)
target_compile_definitions(time_item_forwarding PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
 target_compile_options(time_item_forwarding PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME time_item_forwarding COMMAND time_item_forwarding)
