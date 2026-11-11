# Management actions use the same serialized Controller as existing world actions.
add_executable(test_management_actions
    "${MT2MP_SOURCE_ROOT}/test/test_management_actions.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/network/node_state.cpp")
target_include_directories(test_management_actions PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_management_actions PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_management_actions PRIVATE /utf-8 /EHsc)
endif()
include("${CMAKE_CURRENT_LIST_DIR}/service_live_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/general_project_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/release_graph_checks.cmake")
target_compile_definitions(test_management_actions PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME management_actions COMMAND test_management_actions)
set_tests_properties(management_actions PROPERTIES TIMEOUT 60)
foreach(name IN ITEMS management_service_graph management_service_sources)
    add_executable(test_${name} "${MT2MP_SOURCE_ROOT}/test/test_${name}.cpp")
    target_include_directories(test_${name} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${MT2MP_SOURCE_ROOT}/test")
    target_compile_features(test_${name} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(test_${name} PRIVATE /utf-8 /EHsc)
    endif()
    add_test(NAME ${name} COMMAND test_${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endforeach()
include("${CMAKE_CURRENT_LIST_DIR}/release_cycle_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/gm_ticket_checks.cmake")
