foreach(name protocol controller)
    set(target "test_npc_quest_list_${name}")
    add_executable(${target} "${MT2MP_SOURCE_ROOT}/test/${target}.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
    target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME npc_quest_list_${name} COMMAND ${target})
    set_tests_properties(npc_quest_list_${name} PROPERTIES TIMEOUT 60)
endforeach()
if(WIN32 AND MT2_FRAMEWORK_SDK)
    add_executable(test_npc_quest_list_ui_codes "${MT2MP_SOURCE_ROOT}/test/test_npc_quest_list_ui_codes.cpp")
    target_include_directories(test_npc_quest_list_ui_codes PRIVATE "${MT2MP_SOURCE_ROOT}/src/game"
        "${MT2_FRAMEWORK_SDK}/include" "${MT2_FRAMEWORK_SDK}/adapters/mt2/sdk")
    target_compile_features(test_npc_quest_list_ui_codes PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(test_npc_quest_list_ui_codes PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME npc_quest_list_ui_codes COMMAND test_npc_quest_list_ui_codes)
    set_tests_properties(npc_quest_list_ui_codes PROPERTIES TIMEOUT 60)
endif()
add_test(NAME npc_quest_list_mcp COMMAND "${Python3_EXECUTABLE}" -B -X utf8
    "${MT2MP_SOURCE_ROOT}/test/test_npc_quest_list_mcp.py" "${MT2MP_SOURCE_ROOT}/tools/mcp/server.py")
set_tests_properties(npc_quest_list_mcp PROPERTIES TIMEOUT 60)

include("${CMAKE_CURRENT_LIST_DIR}/character_dungeon_checks.cmake")
