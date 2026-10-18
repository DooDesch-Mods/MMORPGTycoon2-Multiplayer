if(WIN32)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(NOT DEFINED QUEST_READER_DEPENDENCY_ROOT)
    get_filename_component(QUEST_READER_DEPENDENCY_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
set(quest_reader_generated "${CMAKE_CURRENT_BINARY_DIR}/quest_reader_generated")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../test/extract_quest_reader_scope.py"
    --root "${CMAKE_CURRENT_LIST_DIR}/.." --dependencies "${QUEST_READER_DEPENDENCY_ROOT}" --output "${quest_reader_generated}"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_quest_reader_scope "${CMAKE_CURRENT_LIST_DIR}/../test/test_quest_reader_scope.cpp")
target_compile_features(test_quest_reader_scope PRIVATE cxx_std_20)
target_include_directories(test_quest_reader_scope PRIVATE "${quest_reader_generated}" "${QUEST_READER_DEPENDENCY_ROOT}/src/game" "${QUEST_READER_DEPENDENCY_ROOT}/include")
target_compile_options(test_quest_reader_scope PRIVATE /W4 /WX /permissive-)
add_test(NAME quest_reader_scope COMMAND test_quest_reader_scope)
add_executable(test_root_region_metadata "${CMAKE_CURRENT_LIST_DIR}/../test/test_quest_reader_scope.cpp")
target_compile_features(test_root_region_metadata PRIVATE cxx_std_20)
target_compile_definitions(test_root_region_metadata PRIVATE MT2MP_ROOT_REGION_SCOPE_ONLY WIN32_LEAN_AND_MEAN NOMINMAX)
target_include_directories(test_root_region_metadata PRIVATE "${quest_reader_generated}"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game" "${CMAKE_CURRENT_LIST_DIR}/../include"
    "${QUEST_READER_DEPENDENCY_ROOT}/src/game" "${QUEST_READER_DEPENDENCY_ROOT}/include")
target_compile_options(test_root_region_metadata PRIVATE /W4 /WX /permissive-)
add_test(NAME root_region_metadata_baseline COMMAND test_root_region_metadata baseline)
add_test(NAME root_region_metadata_candidate COMMAND test_root_region_metadata candidate)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../test/extract_quest_scalar_batch.py"
    --root "${CMAKE_CURRENT_LIST_DIR}/.." --output "${quest_reader_generated}"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_quest_scalar_batch "${CMAKE_CURRENT_LIST_DIR}/../test/test_quest_scalar_batch.cpp")
target_compile_features(test_quest_scalar_batch PRIVATE cxx_std_20)
target_include_directories(test_quest_scalar_batch PRIVATE "${quest_reader_generated}" "${QUEST_READER_DEPENDENCY_ROOT}/src/game" "${QUEST_READER_DEPENDENCY_ROOT}/include")
target_compile_options(test_quest_scalar_batch PRIVATE /W4 /WX /permissive-)
target_compile_definitions(test_quest_scalar_batch PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
add_test(NAME quest_scalar_batch COMMAND test_quest_scalar_batch)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/object_identity/remove_runtime/copy/world_prop_reader.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/live_host_state/memory_query_timing.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/live_host_state/apply_timing.h"
    "${CMAKE_CURRENT_LIST_DIR}/../test/test_quest_reader_scope.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../test/root_region_scope_entry.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../test/root_region_scope_cases.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../test/extract_quest_scalar_batch.py"
    "${CMAKE_CURRENT_LIST_DIR}/../test/extract_quest_reader_scope.py"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/live_host_state/day_rollover/economy/native_quest_state.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../test/extract_quest_edit_batch.py"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/subscriber_lifecycle/runtime_append.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/subscriber_lifecycle/runtime_append.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/subscriber_lifecycle/quest_edit_batch.inc"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/subscriber_lifecycle/save_scope.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/subscriber_lifecycle/save_scope.h")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../test/extract_quest_edit_batch.py"
    --root "${CMAKE_CURRENT_LIST_DIR}/.." --output "${quest_reader_generated}"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_quest_edit_batch "${CMAKE_CURRENT_LIST_DIR}/../test/test_quest_edit_batch.cpp")
target_compile_features(test_quest_edit_batch PRIVATE cxx_std_20)
target_include_directories(test_quest_edit_batch PRIVATE "${quest_reader_generated}"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/subscriber_lifecycle"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game"
    "${QUEST_READER_DEPENDENCY_ROOT}/src/game" "${QUEST_READER_DEPENDENCY_ROOT}/include")
target_compile_options(test_quest_edit_batch PRIVATE /W4 /WX /permissive-)
target_compile_definitions(test_quest_edit_batch PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
add_test(NAME quest_edit_batch COMMAND test_quest_edit_batch)
endif()
