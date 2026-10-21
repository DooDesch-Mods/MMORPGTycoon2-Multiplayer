find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(NPC_HISTORY_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/npc_history")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/npc_history_runtime.inc")
execute_process(COMMAND "${Python3_EXECUTABLE}" -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_npc_history.py"
    --root "${MT2MP_SOURCE_ROOT}" --output "${NPC_HISTORY_GENERATED}"
    RESULT_VARIABLE NPC_HISTORY_EXTRACT_RESULT)
if(NOT NPC_HISTORY_EXTRACT_RESULT EQUAL 0)
    message(FATAL_ERROR "NPC history actual-body extraction failed")
endif()
foreach(name graph protocol quests)
    set(target "test_npc_history_${name}")
    add_executable(${target} "${MT2MP_SOURCE_ROOT}/test/${target}.cpp")
    if(name STREQUAL "protocol")
        target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
    endif()
    target_include_directories(${target} PRIVATE "${NPC_HISTORY_GENERATED}" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    target_link_libraries(${target} PRIVATE source_check_codecs_windows)
    add_test(NAME "npc_history_${name}" COMMAND ${target})
    set_tests_properties("npc_history_${name}" PROPERTIES TIMEOUT 60)
endforeach()
