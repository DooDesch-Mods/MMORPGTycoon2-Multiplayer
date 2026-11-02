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
include("${CMAKE_CURRENT_LIST_DIR}/release_edit_checks.cmake")
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
if(WIN32)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(management_generated "${CMAKE_CURRENT_BINARY_DIR}/management-generated")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_service_ui_children.py"
        --source-root "${MT2MP_SOURCE_ROOT}" --output "${management_generated}"
        RESULT_VARIABLE service_ui_children_extract)
    if(NOT service_ui_children_extract EQUAL 0)
        message(FATAL_ERROR "Service UI child traversal extraction failed")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${MT2MP_SOURCE_ROOT}/src/game/management_service_ui.inc"
        "${MT2MP_SOURCE_ROOT}/test/extract_service_ui_children.py")
    add_executable(test_service_ui_children "${MT2MP_SOURCE_ROOT}/test/test_service_ui_children.cpp")
    target_include_directories(test_service_ui_children PRIVATE "${management_generated}")
    target_compile_features(test_service_ui_children PRIVATE cxx_std_20)
    add_test(NAME service_ui_children COMMAND test_service_ui_children)
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_lookup_code_batch.py"
        --source-root "${MT2MP_SOURCE_ROOT}" --output "${management_generated}"
        RESULT_VARIABLE lookup_code_batch_extract)
    if(NOT lookup_code_batch_extract EQUAL 0)
        message(FATAL_ERROR "Lookup code batch extraction failed")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${MT2MP_SOURCE_ROOT}/src/game/object_identity/remove_runtime/copy/world_prop_reader.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/object_identity/remove_runtime/copy/saved_scenery_batch_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/test/extract_lookup_code_batch.py")
    add_executable(test_lookup_code_batch "${MT2MP_SOURCE_ROOT}/test/test_lookup_code_batch.cpp")
    target_include_directories(test_lookup_code_batch PRIVATE "${management_generated}")
    target_compile_features(test_lookup_code_batch PRIVATE cxx_std_20)
    add_test(NAME lookup_code_batch COMMAND test_lookup_code_batch)
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_management_native.py"
        --source-root "${MT2MP_SOURCE_ROOT}" --output "${management_generated}"
        RESULT_VARIABLE management_extract)
    if(NOT management_extract EQUAL 0)
        message(FATAL_ERROR "Management actual native scalar extraction failed")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${MT2MP_SOURCE_ROOT}/src/game/management_runtime.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/management_campaign_ui.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/management_live_port.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/dungeon_gizmo_live_native.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/typed_action_runtime.cpp"
        "${MT2MP_SOURCE_ROOT}/test/extract_management_native.py"
        "${MT2MP_SOURCE_ROOT}/src/game/analytics_native.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/analytics_state.h"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_demand_native.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_demand_state.h"
        "${MT2MP_SOURCE_ROOT}/src/game/toon_thought_lifetime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/toon_thought_lifetime.h"
        "${MT2MP_SOURCE_ROOT}/src/game/toon_thought_state.h"
        "${MT2MP_SOURCE_ROOT}/src/game/toon_thought_frame.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/toon_thought_codes.inc")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MT2MP_SOURCE_ROOT}/src/game/building_uptime_native.inc")
    add_executable(test_management_native "${MT2MP_SOURCE_ROOT}/test/test_management_native.cpp" "${MT2MP_SOURCE_ROOT}/src/game/typed_action/tick_authority.cpp")
    target_link_libraries(test_management_native PRIVATE source_check_codecs_windows)
    target_include_directories(test_management_native PRIVATE "${management_generated}" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(test_management_native PRIVATE cxx_std_20)
    target_compile_definitions(test_management_native PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(test_management_native PRIVATE /utf-8 /EHsc)
    endif()
    add_test(NAME management_native COMMAND test_management_native)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/release_cycle_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/gm_ticket_checks.cmake")
