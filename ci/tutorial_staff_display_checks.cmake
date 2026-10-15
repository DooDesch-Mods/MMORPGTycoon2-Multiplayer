find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/staff_display_owner.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/staff_display_ui.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/staff_display_values.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/staff_display_hook.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/native.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint_native.inc")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_tutorial_staff_display.py"
    --source-root "${MT2MP_SOURCE_ROOT}" --output "${CMAKE_CURRENT_BINARY_DIR}/staff_display_extracted"
    COMMAND_ERROR_IS_FATAL ANY)
foreach(target test_tutorial_staff_display test_tutorial_staff_display_native test_tutorial_staff_display_guard)
    add_executable(${target} "${MT2MP_SOURCE_ROOT}/test/${target}.cpp")
endforeach()
set(staff_frame_sources
    tutorial_replication/state.cpp tutorial_replication/startingpoint.cpp
    live_host_state/state.cpp live_host_state/network/node_state.cpp
    live_host_state/day_rollover/day.cpp live_host_state/day_rollover/economy/codec.cpp
    live_host_state/day_rollover/economy/monetary.cpp subscriber_lifecycle/additions.cpp
    subscriber_lifecycle/payload.cpp subscriber_lifecycle/record_decode.cpp subscriber_lifecycle/record_graph.cpp
    subscriber_lifecycle/reference_map.cpp)
list(TRANSFORM staff_frame_sources PREPEND "${MT2MP_SOURCE_ROOT}/src/game/")
add_executable(test_tutorial_staff_display_frame "${MT2MP_SOURCE_ROOT}/test/test_tutorial_staff_display_frame.cpp" ${staff_frame_sources})
foreach(target test_tutorial_staff_display test_tutorial_staff_display_native test_tutorial_staff_display_guard test_tutorial_staff_display_frame)
    target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include"
        "${CMAKE_CURRENT_BINARY_DIR}/staff_display_extracted")
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 60)
endforeach()
if(MSVC)
    target_compile_options(test_tutorial_staff_display_frame PRIVATE /Gy)
    target_link_options(test_tutorial_staff_display_frame PRIVATE /OPT:REF /INCREMENTAL:NO)
endif()
