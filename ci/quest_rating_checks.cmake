find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(QUEST_RATING_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/quest_rating")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/npc_quest_rating_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/day_rollover/economy/native_quest_runtime.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/economic_fence.h"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/bridge.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action_runtime.cpp"
    "${MT2MP_SOURCE_ROOT}/test/extract_quest_rating.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_quest_rating.py"
    --source-root "${MT2MP_SOURCE_ROOT}" --output "${QUEST_RATING_GENERATED}" COMMAND_ERROR_IS_FATAL ANY)
set(quest_rating_frame_sources
    tutorial_replication/state.cpp tutorial_replication/startingpoint.cpp developer_staff/shared_state.cpp
    live_host_state/state.cpp live_host_state/network/node_state.cpp live_host_state/day_rollover/day.cpp
    live_host_state/day_rollover/economy/codec.cpp live_host_state/day_rollover/economy/monetary.cpp
    subscriber_lifecycle/additions.cpp subscriber_lifecycle/payload.cpp subscriber_lifecycle/record_decode.cpp
    subscriber_lifecycle/record_graph.cpp subscriber_lifecycle/reference_map.cpp)
foreach(name apply pending frame)
    set(target test_quest_rating_${name})
    add_executable(${target} "${MT2MP_SOURCE_ROOT}/test/${target}.cpp")
    if(name STREQUAL "frame")
        foreach(source IN LISTS quest_rating_frame_sources)
            target_sources(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/${source}")
        endforeach()
    endif()
    target_include_directories(${target} PRIVATE "${QUEST_RATING_GENERATED}" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc /Gy)
        target_link_options(${target} PRIVATE /OPT:REF /INCREMENTAL:NO)
    endif()
    add_test(NAME quest_rating_${name} COMMAND ${target})
    set_tests_properties(quest_rating_${name} PROPERTIES TIMEOUT 60)
endforeach()
