find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_npc_state_guard.py"
    --source-root "${MT2MP_SOURCE_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/npc_state_guard_extracted" COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_quest_completion_aggregate "${MT2MP_SOURCE_ROOT}/test/test_quest_completion_aggregate.cpp")
target_include_directories(test_quest_completion_aggregate PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${CMAKE_CURRENT_BINARY_DIR}/npc_state_guard_extracted")
target_compile_features(test_quest_completion_aggregate PRIVATE cxx_std_20)
target_compile_options(test_quest_completion_aggregate PRIVATE /W4 /WX /utf-8 /EHsc)
add_test(NAME quest_completion_aggregate COMMAND test_quest_completion_aggregate)
set_tests_properties(quest_completion_aggregate PROPERTIES TIMEOUT 60)
