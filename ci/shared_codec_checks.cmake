# These state and codec sources are used unchanged by several source-only test binaries.
# Keep one compiled copy while retaining every executable and test registration.
set(shared_codec_sources
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/network/node_state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/day_rollover/day.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/day_rollover/economy/codec.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/day_rollover/economy/monetary.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/additions.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/payload.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/record_graph.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/reference_map.cpp")
if(WIN32)
    list(APPEND shared_codec_sources "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/record_decode.cpp")
endif()
get_property(shared_codec_consumers DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
foreach(archive source_check_codecs source_check_codecs_windows)
    add_library(${archive} STATIC ${shared_codec_sources})
    target_include_directories(${archive} PRIVATE
        "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${archive} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${archive} PRIVATE /W4 /WX /utf-8 /EHsc /Gy)
        target_precompile_headers(${archive} PRIVATE
            "${MT2MP_SOURCE_ROOT}/include/vendor/nlohmann/json.hpp")
    endif()
endforeach()
target_compile_definitions(source_check_codecs_windows PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
foreach(target IN LISTS shared_codec_consumers)
    get_target_property(sources ${target} SOURCES)
    set(uses_shared_codec FALSE)
    foreach(source IN LISTS shared_codec_sources)
        if(source IN_LIST sources)
            list(REMOVE_ITEM sources "${source}")
            set(uses_shared_codec TRUE)
        endif()
    endforeach()
    if(uses_shared_codec)
        get_target_property(definitions ${target} COMPILE_DEFINITIONS)
        set(archive source_check_codecs)
        if(definitions)
            list(SORT definitions)
            if(NOT definitions STREQUAL "NOMINMAX;WIN32_LEAN_AND_MEAN")
                message(FATAL_ERROR "Shared codec compile definitions changed for ${target}: ${definitions}")
            endif()
            set(archive source_check_codecs_windows)
        endif()
        set_property(TARGET ${target} PROPERTY SOURCES "${sources}")
        target_link_libraries(${target} PRIVATE ${archive})
    endif()
endforeach()
