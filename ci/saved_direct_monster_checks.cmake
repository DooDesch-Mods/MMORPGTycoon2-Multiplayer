add_executable(test_saved_direct_monster_identity "${MT2MP_SOURCE_ROOT}/test/test_saved_direct_monster_identity.cpp")
target_include_directories(test_saved_direct_monster_identity PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_saved_direct_monster_identity PRIVATE cxx_std_20)
target_compile_definitions(test_saved_direct_monster_identity PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(test_saved_direct_monster_identity PRIVATE /W4 /WX /utf-8 /EHsc)
add_test(NAME saved_direct_monster_identity COMMAND test_saved_direct_monster_identity)
set_tests_properties(saved_direct_monster_identity PROPERTIES TIMEOUT 60)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/test/extract_saved_monster_bodies.py"
    "${MT2MP_SOURCE_ROOT}/src/game/monster_saved_identity_runtime.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/npc_quest_target_runtime.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/network_identity.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/network_object_identity_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/net/snapshot_sync.cpp")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_saved_monster_bodies.py"
    "${MT2MP_SOURCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/saved_monster_extracted" COMMAND_ERROR_IS_FATAL ANY)
foreach(kind seal transfer)
    add_executable(test_saved_monster_${kind} "${MT2MP_SOURCE_ROOT}/test/test_saved_monster_${kind}.cpp")
    target_include_directories(test_saved_monster_${kind} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${CMAKE_CURRENT_BINARY_DIR}/saved_monster_extracted")
    target_compile_features(test_saved_monster_${kind} PRIVATE cxx_std_20)
    target_compile_definitions(test_saved_monster_${kind} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(test_saved_monster_${kind} PRIVATE /W4 /WX /utf-8 /EHsc)
endforeach()
add_test(NAME saved_monster_seal COMMAND test_saved_monster_seal)
add_test(NAME saved_monster_transfer COMMAND test_saved_monster_transfer "${CMAKE_CURRENT_BINARY_DIR}/saved_monster_transfer_files")
set_tests_properties(saved_monster_seal saved_monster_transfer PROPERTIES TIMEOUT 60)
add_executable(test_saved_monster_protocol "${MT2MP_SOURCE_ROOT}/test/test_saved_monster_protocol.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
target_include_directories(test_saved_monster_protocol PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_saved_monster_protocol PRIVATE cxx_std_20)
target_compile_options(test_saved_monster_protocol PRIVATE /W4 /WX /utf-8 /EHsc)
add_test(NAME saved_monster_protocol COMMAND test_saved_monster_protocol)
set_tests_properties(saved_monster_protocol PROPERTIES TIMEOUT 60)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MT2MP_SOURCE_ROOT}/src/core/dll_main.cpp" "${MT2MP_SOURCE_ROOT}/src/game/lobby_operations.cpp" "${MT2MP_SOURCE_ROOT}/test/extract_selected_snapshot.py")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MT2MP_SOURCE_ROOT}/src/game/native_engine.cpp")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_selected_snapshot.py"
    "${MT2MP_SOURCE_ROOT}" "${CMAKE_CURRENT_BINARY_DIR}/selected_snapshot_extracted" COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_selected_snapshot "${MT2MP_SOURCE_ROOT}/test/test_selected_snapshot.cpp")
target_include_directories(test_selected_snapshot PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${CMAKE_CURRENT_BINARY_DIR}/selected_snapshot_extracted")
target_compile_features(test_selected_snapshot PRIVATE cxx_std_20)
target_compile_definitions(test_selected_snapshot PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(test_selected_snapshot PRIVATE /W4 /WX /utf-8 /EHsc)
target_link_libraries(test_selected_snapshot PRIVATE bcrypt)
add_test(NAME selected_snapshot COMMAND test_selected_snapshot "${CMAKE_CURRENT_BINARY_DIR}/selected_snapshot_files")
set_tests_properties(selected_snapshot PROPERTIES TIMEOUT 60)

add_executable(test_dungeon_quest_seal "${MT2MP_SOURCE_ROOT}/test/test_dungeon_quest_seal.cpp")
target_include_directories(test_dungeon_quest_seal PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${CMAKE_CURRENT_BINARY_DIR}/saved_monster_extracted")
target_compile_features(test_dungeon_quest_seal PRIVATE cxx_std_20)
target_compile_definitions(test_dungeon_quest_seal PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_dungeon_quest_seal PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME dungeon_quest_seal COMMAND test_dungeon_quest_seal)
set_tests_properties(dungeon_quest_seal PROPERTIES TIMEOUT 60)
