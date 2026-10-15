find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/bridge.h"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/bridge.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/economic_fence.h"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/economic_fence.cpp")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_locked_lobby_leave.py"
    --source-root "${MT2MP_SOURCE_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/locked_lobby_leave_extracted"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_locked_lobby_leave "${MT2MP_SOURCE_ROOT}/test/test_locked_lobby_leave.cpp")
target_include_directories(test_locked_lobby_leave PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/locked_lobby_leave_extracted"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_locked_lobby_leave PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_locked_lobby_leave PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME locked_lobby_leave COMMAND test_locked_lobby_leave)
set_tests_properties(locked_lobby_leave PROPERTIES TIMEOUT 30)
