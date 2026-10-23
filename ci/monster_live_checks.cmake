find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(_monster_live_generated "${CMAKE_CURRENT_BINARY_DIR}/monster_live_generated")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/bridge.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/bridge.h"
    "${MT2MP_SOURCE_ROOT}/src/game/monster_live_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/hooks/framework_catalog.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action_runtime.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/npc_action_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/test/extract_monster_live.py"
    "${MT2MP_SOURCE_ROOT}/test/party_timing_source_guard.json"
    "${MT2MP_SOURCE_ROOT}/src/game/party_live_native.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/party_frame_native.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/scene_clone/runtime.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/object_identity/remove_runtime/copy/world_prop_reader.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/save_scope.cpp")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_monster_live.py"
    --source-root "${MT2MP_SOURCE_ROOT}" --output "${_monster_live_generated}" COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_monster_live_state "${MT2MP_SOURCE_ROOT}/test/test_monster_live_state.cpp" ${vip_frame_sources})
add_executable(test_monster_live_bridge "${MT2MP_SOURCE_ROOT}/test/test_monster_live_bridge.cpp")
add_executable(test_monster_live_callbacks "${MT2MP_SOURCE_ROOT}/test/test_monster_live_callbacks.cpp")
add_executable(test_monster_live_pending "${MT2MP_SOURCE_ROOT}/test/test_monster_live_pending.cpp" ${vip_frame_sources})
add_executable(test_monster_live_presentation "${MT2MP_SOURCE_ROOT}/test/test_monster_live_presentation.cpp")
add_executable(test_monster_live_identity "${MT2MP_SOURCE_ROOT}/test/test_monster_live_identity.cpp" ${vip_frame_sources})
target_link_libraries(test_monster_live_identity PRIVATE source_check_codecs_windows bcrypt)
add_executable(test_monster_live_animation "${MT2MP_SOURCE_ROOT}/test/test_monster_live_animation.cpp")
add_executable(test_monster_live_save_scope "${MT2MP_SOURCE_ROOT}/test/test_monster_live_save_scope.cpp")
add_executable(test_monster_live_lifecycle "${MT2MP_SOURCE_ROOT}/test/test_monster_live_lifecycle.cpp")
foreach(_target test_monster_live_lifecycle test_monster_live_save_scope test_monster_live_animation test_monster_live_identity test_monster_live_presentation test_monster_live_state test_monster_live_bridge test_monster_live_callbacks test_monster_live_pending)
    target_include_directories(${_target} PRIVATE "${_monster_live_generated}" "${MT2MP_SOURCE_ROOT}/src/game/live_host_state" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${_target} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${_target} PRIVATE /W4 /WX /utf-8 /EHsc /Gy)
        target_link_options(${_target} PRIVATE /OPT:REF /INCREMENTAL:NO)
    endif()
    add_test(NAME ${_target} COMMAND ${_target})
    set_tests_properties(${_target} PROPERTIES TIMEOUT 60)
endforeach()

target_link_libraries(test_monster_live_state PRIVATE source_check_codecs)

target_link_libraries(test_monster_live_pending PRIVATE source_check_codecs)
