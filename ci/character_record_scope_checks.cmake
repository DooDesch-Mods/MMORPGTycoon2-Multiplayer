if(WIN32)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
get_filename_component(CHARACTER_RECORD_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(CHARACTER_RECORD_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/character_record_generated")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CHARACTER_RECORD_ROOT}/test/extract_character_record_scope.py"
    "${CHARACTER_RECORD_ROOT}/src/game/character_membership_sample.inc"
    "${CHARACTER_RECORD_ROOT}/src/game/character_actor_records_runtime.inc"
    "${CHARACTER_RECORD_ROOT}/src/game/scenery_kiss/native_catalog.cpp"
    "${CHARACTER_RECORD_ROOT}/src/game/general_project_runtime.cpp"
    "${CHARACTER_RECORD_ROOT}/src/game/gm_ticket_native.cpp"
    "${CHARACTER_RECORD_ROOT}/src/game/world_zone_current.inc"
    "${CHARACTER_RECORD_ROOT}/src/game/dungeon_scene_identity_runtime.cpp"
    "${CHARACTER_RECORD_ROOT}/src/game/subscriber_lifecycle/capture.cpp")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CHARACTER_RECORD_ROOT}/test/extract_character_record_scope.py"
    --root "${CHARACTER_RECORD_ROOT}" --output "${CHARACTER_RECORD_GENERATED}" COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_character_record_scope
    "${CHARACTER_RECORD_ROOT}/test/test_character_record_scope.cpp"
    "${CHARACTER_RECORD_ROOT}/src/game/subscriber_lifecycle/record_graph.cpp")
target_compile_features(test_character_record_scope PRIVATE cxx_std_20)
target_include_directories(test_character_record_scope PRIVATE "${CHARACTER_RECORD_GENERATED}"
    "${CHARACTER_RECORD_ROOT}/src/game" "${CHARACTER_RECORD_ROOT}/include")
target_compile_options(test_character_record_scope PRIVATE /W4 /WX /permissive-)
add_test(NAME character_record_scope COMMAND test_character_record_scope)
endif()
