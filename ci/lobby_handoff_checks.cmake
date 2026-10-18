find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(lobby_handoff_generated "${CMAKE_CURRENT_BINARY_DIR}/lobby_handoff_generated")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/framework_lobby_domain.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/native_lobby_framework.inc"
    "${MT2MP_SOURCE_ROOT}/test/extract_lobby_handoff.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_lobby_handoff.py"
    --root "${MT2MP_SOURCE_ROOT}" --output "${lobby_handoff_generated}"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_lobby_handoff "${MT2MP_SOURCE_ROOT}/test/test_lobby_handoff.cpp")
target_compile_features(test_lobby_handoff PRIVATE cxx_std_20)
target_include_directories(test_lobby_handoff PRIVATE "${lobby_handoff_generated}" "${MT2MP_SOURCE_ROOT}/include")
if(MSVC)
    target_compile_options(test_lobby_handoff PRIVATE /W4 /WX /utf-8)
else()
    target_compile_options(test_lobby_handoff PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME lobby_handoff COMMAND test_lobby_handoff)
set_tests_properties(lobby_handoff PROPERTIES TIMEOUT 30)

add_test(NAME lobby_wizard_automation
    COMMAND "${Python3_EXECUTABLE}" -B -X utf8 -m unittest discover
        -s "${MT2MP_SOURCE_ROOT}/test/lobby_ui" -p test_wizard_automation.py)
set_tests_properties(lobby_wizard_automation PROPERTIES TIMEOUT 30)
