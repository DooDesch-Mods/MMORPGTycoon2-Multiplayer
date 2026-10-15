cmake_minimum_required(VERSION 3.20)
project(MT2MP_SourceChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(MT2MP_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." CACHE PATH "Canonical source root")
enable_testing()
foreach(name action_order software_snapshot snapshot_buffer)
    add_executable(test_${name} "${MT2MP_SOURCE_ROOT}/test/test_${name}.cpp")
    target_include_directories(test_${name} PRIVATE "${MT2MP_SOURCE_ROOT}/include")
    target_compile_definitions(test_${name} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    if(MSVC)
        target_compile_options(test_${name} PRIVATE /W4 /WX)
    endif()
    add_test(NAME ${name} COMMAND test_${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 60 WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
endforeach()
if(WIN32)
    target_link_libraries(test_snapshot_buffer PRIVATE bcrypt)
endif()

# Compile the actual native callback bodies against bounded test doubles.
set(icon_source "${MT2MP_SOURCE_ROOT}/src/game/native_lobby.cpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${icon_source}")
file(READ "${icon_source}" icon_text)
string(FIND "${icon_text}" "struct DesktopIconBinding" state_begin)
string(FIND "${icon_text}" "// The game owns the native icon" state_end)
string(FIND "${icon_text}" "void desktop_icon_launch(" callback_begin)
string(FIND "${icon_text}" "bool refresh_saved_worlds(" callback_end)
if(state_begin LESS 0 OR state_end LESS state_begin OR callback_begin LESS 0 OR callback_end LESS callback_begin)
    message(FATAL_ERROR "Native Desktop callback boundaries changed; update the affected test extraction")
endif()
math(EXPR state_size "${state_end}-${state_begin}")
math(EXPR callback_size "${callback_end}-${callback_begin}")
string(SUBSTRING "${icon_text}" ${state_begin} ${state_size} icon_state)
string(SUBSTRING "${icon_text}" ${callback_begin} ${callback_size} icon_callbacks)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/desktop_icon_behavior_body.inc" "${icon_state}\n${icon_callbacks}")
add_executable(test_desktop_icon_behavior "${MT2MP_SOURCE_ROOT}/test/test_desktop_icon_behavior.cpp")
target_include_directories(test_desktop_icon_behavior PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
    target_compile_options(test_desktop_icon_behavior PRIVATE /W4 /WX /utf-8)
endif()
add_test(NAME desktop_icon_behavior COMMAND test_desktop_icon_behavior)
set_tests_properties(desktop_icon_behavior PROPERTIES TIMEOUT 60)

string(FIND "${icon_text}" "bool select_world(" world_end)
if(callback_end LESS 0 OR world_end LESS callback_end)
    message(FATAL_ERROR "Native world selection boundaries changed; update the affected test extraction")
endif()
math(EXPR world_size "${world_end}-${callback_end}")
string(SUBSTRING "${icon_text}" ${callback_end} ${world_size} world_body)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/world_selection_body.inc" "${world_body}")
add_executable(test_world_selection "${MT2MP_SOURCE_ROOT}/test/test_world_selection.cpp")
target_include_directories(test_world_selection PRIVATE "${CMAKE_CURRENT_BINARY_DIR}" "${MT2MP_SOURCE_ROOT}")
if(MSVC)
    target_compile_options(test_world_selection PRIVATE /W4 /WX /utf-8)
endif()
add_test(NAME world_selection COMMAND test_world_selection)
set_tests_properties(world_selection PROPERTIES TIMEOUT 60)

string(FIND "${icon_text}" "bool bind_desktop_icon_presentation(" presentation_begin)
string(FIND "${icon_text}" "bool desktop_entry(" presentation_end)
if(presentation_begin LESS 0 OR presentation_end LESS presentation_begin)
    message(FATAL_ERROR "Native presentation boundaries changed; update the affected test extraction")
endif()
math(EXPR presentation_size "${presentation_end}-${presentation_begin}")
string(SUBSTRING "${icon_text}" ${presentation_begin} ${presentation_size} presentation_body)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/desktop_icon_presentation_body.inc" "${presentation_body}")
add_executable(test_desktop_icon_presentation "${MT2MP_SOURCE_ROOT}/test/test_desktop_icon_presentation.cpp")
target_include_directories(test_desktop_icon_presentation PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
    target_compile_options(test_desktop_icon_presentation PRIVATE /W4 /WX /utf-8)
endif()
add_test(NAME desktop_icon_presentation COMMAND test_desktop_icon_presentation)
set_tests_properties(desktop_icon_presentation PROPERTIES TIMEOUT 60)

include("${CMAKE_CURRENT_LIST_DIR}/auto_world_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/invite_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/empty_world_bind_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/dropin_proof_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/dropin_write_dir_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/cold_time_retire_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/snapshot_world_path_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_observer_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/empty_network_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/client_leave_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/original_saves_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/private_save_parent_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/named_world_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/diagnostic_mailbox_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/tutorial_state_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/tutorial_load_gate_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/tutorial_development_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/tutorial_progress_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/tutorial_movement_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/tutorial_startingpoint_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_startingpoint_client_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_creation_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_commit_intent_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_client_commit_ui_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/tutorial_start_intent_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_staff_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_client_staff_ui_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/tutorial_staff_activity_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/tutorial_staff_display_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/tutorial_message_close_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/tutorial_completion_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/tutorial_network_tab_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/tutorial_load_start_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/npc_economy_difference_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/locked_lobby_leave_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/npc_state_guard_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/npc_pose_phase_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/monster_zone_probe_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/developer_staff_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/developer_staff_retirement_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/developer_job_census_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/developer_staff_gameplay_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/developer_construction_portable_checks.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/developer_construction_native_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/monster_zone_action_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/developer_job_target_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/developer_movement_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/snapshot_capture_policy_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/buildings_tab_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/building_remove_restore_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/bug_authority_portable_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/bug_authority_native_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/building_types_checks.cmake")

include("${CMAKE_CURRENT_LIST_DIR}/shared_codec_checks.cmake")
