# Compile the actual typed action bind method against bounded world and source witnesses.
set(empty_bind_source "${MT2MP_SOURCE_ROOT}/src/game/typed_action_runtime.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${empty_bind_source}")
file(READ "${empty_bind_source}" empty_bind_text)
string(FIND "${empty_bind_text}" "    bool bind(const std::string& snapshot,action::Binding& out,std::string& error)override {" empty_bind_begin)
string(FIND "${empty_bind_text}" "    bool current(const action::Binding& binding,const std::string&,std::string& error)override {" empty_bind_end)
if(empty_bind_begin LESS 0 OR empty_bind_end LESS empty_bind_begin)
    message(FATAL_ERROR "Native empty-world bind boundaries changed; update the affected test extraction")
endif()
math(EXPR empty_bind_size "${empty_bind_end}-${empty_bind_begin}")
string(SUBSTRING "${empty_bind_text}" ${empty_bind_begin} ${empty_bind_size} empty_bind_body)
string(REPLACE ")override {" ") {" empty_bind_body "${empty_bind_body}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/empty_world_bind_body.inc" "${empty_bind_body}")
add_executable(test_empty_world_bind "${MT2MP_SOURCE_ROOT}/test/test_empty_world_bind.cpp")
target_include_directories(test_empty_world_bind PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${MT2MP_SOURCE_ROOT}/include")
if(MSVC)
    target_compile_options(test_empty_world_bind PRIVATE /W4 /WX /utf-8 /UNDEBUG)
else()
    target_compile_options(test_empty_world_bind PRIVATE -UNDEBUG)
endif()
add_test(NAME empty_world_bind COMMAND test_empty_world_bind)
set_tests_properties(empty_world_bind PROPERTIES TIMEOUT 60)

# Exercise the actual Capture/Load selector with retained prior-session capture state.
set(binding_source "${MT2MP_SOURCE_ROOT}/src/game/remove_diagnostics.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${binding_source}")
file(READ "${binding_source}" binding_source_text)
string(FIND "${binding_source_text}" "    const auto capture=building_capture_identity::status();" binding_source_begin)
string(FIND "${binding_source_text}" "    if(details==BindingDetails::Refund&&!attach_refunds(world,list,error))" binding_source_end)
if(binding_source_begin LESS 0 OR binding_source_end LESS binding_source_begin)
    message(FATAL_ERROR "Native binding source selection boundaries changed; update the affected extraction")
endif()
math(EXPR binding_source_size "${binding_source_end}-${binding_source_begin}")
string(SUBSTRING "${binding_source_text}" ${binding_source_begin} ${binding_source_size} binding_source_body)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/snapshot_binding_source_body.inc" "${binding_source_body}")
target_include_directories(test_empty_world_bind PRIVATE "${MT2MP_SOURCE_ROOT}/src/game")

# The same target exercises the full identity collector and target resolvers.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../test/saved_scenery_batch_source_boundary.py"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/object_identity/remove_runtime/copy/native_origin.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/root_scenery_snapshot.inc"
    "${MT2MP_SOURCE_ROOT}/test/extract_identity_refund_split.py"
    "${MT2MP_SOURCE_ROOT}/src/game/root_scenery_metadata.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/service_building_census.h"
    "${MT2MP_SOURCE_ROOT}/test/identity_refund_split_cases.inc"
    "${MT2MP_SOURCE_ROOT}/test/building_identity_cases.inc"
    "${MT2MP_SOURCE_ROOT}/test/party_owner_cases.inc"
    "${MT2MP_SOURCE_ROOT}/test/construction_target_cases.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/construction_native.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/party_frame_native.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/building_identity_census.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/service_live_native.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/toon_thought_frame.inc")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_identity_refund_split.py"
    --root "${MT2MP_SOURCE_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}"
    COMMAND_ERROR_IS_FATAL ANY)

# The actual NativeEngine bind join also publishes into the real World table.
target_sources(test_empty_world_bind PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/multiplayer_world/world_state.cpp")
