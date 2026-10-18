find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/bridge.cpp"
    "${MT2MP_SOURCE_ROOT}/test/check_query_phase_guards.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/check_query_phase_guards.py"
    --source-root "${MT2MP_SOURCE_ROOT}" COMMAND_ERROR_IS_FATAL ANY)
