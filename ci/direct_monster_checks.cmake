foreach(name protocol controller mixed_identity)
    set(target "test_direct_monster_${name}")
    add_executable(${target} "${MT2MP_SOURCE_ROOT}/test/${target}.cpp")
    if(name STREQUAL "protocol" OR name STREQUAL "controller")
        target_sources(${target} PRIVATE
            "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
            "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
    endif()
    target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    add_test(NAME direct_monster_${name} COMMAND ${target})
    set_tests_properties(direct_monster_${name} PROPERTIES TIMEOUT 60)

endforeach()
include("${CMAKE_CURRENT_LIST_DIR}/monster_hook_admission_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/direct_monster_move_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/monster_remove_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/monster_live_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/npc_quest_authoring_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/saved_direct_monster_checks.cmake")
