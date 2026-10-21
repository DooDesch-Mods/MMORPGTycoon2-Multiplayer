find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(MONSTER_HISTORY_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/monster_history")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/monster_history_runtime.inc")
execute_process(COMMAND "${Python3_EXECUTABLE}" -X utf8 "${MT2MP_SOURCE_ROOT}/test/extract_monster_history.py"
    "${MT2MP_SOURCE_ROOT}/src/game/monster_history_runtime.inc" "${MONSTER_HISTORY_GENERATED}"
    RESULT_VARIABLE MONSTER_HISTORY_EXTRACT_RESULT)
if(NOT MONSTER_HISTORY_EXTRACT_RESULT EQUAL 0)
    message(FATAL_ERROR "Monster history actual-body extraction failed")
endif()
foreach(name target_scope edit_protocol history_references history_graph destination)
    set(target "test_monster_${name}")
    add_executable(${target} "${MT2MP_SOURCE_ROOT}/test/${target}.cpp")
    if(name STREQUAL "edit_protocol")
        target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
    endif()
    target_include_directories(${target} PRIVATE "${MONSTER_HISTORY_GENERATED}" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    add_test(NAME monster_${name} COMMAND ${target})
    set_tests_properties(monster_${name} PROPERTIES TIMEOUT 60)
endforeach()
