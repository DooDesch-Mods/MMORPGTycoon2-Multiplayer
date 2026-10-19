if(WIN32 AND MSVC)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(native_regression_root "${MT2MP_SOURCE_ROOT}/test/native_state_regression")
    set(native_regression_generated "${CMAKE_CURRENT_BINARY_DIR}/native_state_regression")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${MT2MP_SOURCE_ROOT}/src/core/dll_main.cpp"
        "${MT2MP_SOURCE_ROOT}/src/core/framework_plugin.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/control_input/control_runtime.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/control_input/control_route_initializers.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/monster_action_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/monster_action_codes.h"
        "${MT2MP_SOURCE_ROOT}/src/game/release_runtime.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/release_cycle_pins.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/placement_ui_probe/ui_probe.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/npc_quest_list_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/dungeon_palette_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/gm_policy_route_pins.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/npc_quest_authoring_codes.h"
        "${MT2MP_SOURCE_ROOT}/src/game/npc_quest_list_codes.h"
        "${MT2MP_SOURCE_ROOT}/src/game/npc_quest_list_ui_codes.h"
        "${MT2MP_SOURCE_ROOT}/src/game/dungeon_palette_codes.h"
        "${MT2MP_SOURCE_ROOT}/src/hooks/framework_backend.cpp"
        "${native_regression_root}/core/check.py"
        "${native_regression_root}/core/check_extended_v2.py"
        "${native_regression_root}/shared_itemactivate/baseline/src/game/monster_action_runtime.inc"
        "${native_regression_root}/shared_clock/baseline/src/game/control_input/control_runtime.cpp"
        "${native_regression_root}/shared_clock/baseline/src/game/release_runtime.cpp"
        "${native_regression_root}/shared_clock/baseline/src/game/placement_ui_probe/ui_probe.cpp"
        "${native_regression_root}/shared_quest/baseline/src/game/npc_quest_list_runtime.inc"
        "${native_regression_root}/shared_quest/baseline/src/game/dungeon_palette_runtime.inc")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${MT2MP_SOURCE_ROOT}/include/mt2mp/hooks/original_body.h"
        "${MT2MP_SOURCE_ROOT}/src/game/npc_probe.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/character_membership_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/dungeon_cells_runtime.inc"
        "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/bug_policy_process.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/duplicate_codeguards.h"
        "${MT2MP_SOURCE_ROOT}/src/game/scenery_kiss/lifecycle_native.cpp"
        "${native_regression_root}/original_body/baseline.json")
    set(native_core_generated "${native_regression_generated}/core")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -X utf8
        "${native_regression_root}/core/check_extended_v2.py"
        --source "${MT2MP_SOURCE_ROOT}" --output "${native_core_generated}"
        COMMAND_ERROR_IS_FATAL ANY)
    foreach(name instance_init_thread instance_runtime_ready)
        add_executable(test_${name} "${native_core_generated}/test_${name}.cpp")
        target_compile_features(test_${name} PRIVATE cxx_std_20)
        target_compile_options(test_${name} PRIVATE /W4 /WX /utf-8 /EHsc)
        add_test(NAME ${name} COMMAND test_${name})
        set_tests_properties(${name} PROPERTIES TIMEOUT 30)
    endforeach()

    if(MT2_FRAMEWORK_SDK)
        foreach(header include/mt2fw/plugin.h adapters/mt2/sdk/native_services.h
                adapters/mt2/sdk/native_routes.h adapters/mt2/sdk/native_ownership.h)
            if(NOT EXISTS "${MT2_FRAMEWORK_SDK}/${header}")
                message(FATAL_ERROR "Native state regression requires Framework SDK header: ${header}")
            endif()
        endforeach()
        foreach(group shared_itemactivate shared_clock shared_quest original_body)
            set(generated "${native_regression_generated}/${group}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                "${native_regression_root}/${group}/check.py")
            execute_process(COMMAND "${Python3_EXECUTABLE}" -X utf8
                "${native_regression_root}/${group}/check.py"
                --source "${MT2MP_SOURCE_ROOT}" --output "${generated}"
                COMMAND_ERROR_IS_FATAL ANY)
        endforeach()
        foreach(name shared_itemactivate shared_clock probe_shared_item shared_quest original_body)
            if(name STREQUAL "probe_shared_item")
                set(group shared_clock)
            else()
                set(group ${name})
            endif()
            add_executable(test_${name} "${native_regression_root}/${group}/test_${name}.cpp")
            target_include_directories(test_${name} PRIVATE
                "${native_regression_generated}/${group}"
                "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/src/hooks"
                "${MT2MP_SOURCE_ROOT}/include" "${MT2MP_SOURCE_ROOT}/include/vendor/minhook/include"
                "${MT2_FRAMEWORK_SDK}/include" "${MT2_FRAMEWORK_SDK}/adapters/mt2/sdk")
            target_compile_features(test_${name} PRIVATE cxx_std_20)
            target_compile_definitions(test_${name} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
            if(name STREQUAL "shared_quest" OR name STREQUAL "original_body")
                target_compile_definitions(test_${name} PRIVATE MT2MP_FRAMEWORK_PLUGIN)
            endif()
            target_compile_options(test_${name} PRIVATE /W4 /WX /utf-8 /EHsc)
            add_test(NAME ${name} COMMAND test_${name})
            set_tests_properties(${name} PROPERTIES TIMEOUT 30)
        endforeach()
        set(dungeon_boundary_root "${native_regression_root}/dungeon_boundary")
        set(dungeon_boundary_generated "${native_regression_generated}/dungeon_boundary")
        set(dungeon_boundary_minhook "${MT2_FRAMEWORK_SDK}/third_party/vendor/minhook/src")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${dungeon_boundary_root}/check.py" "${dungeon_boundary_root}/baseline.json")
        foreach(name gizmo config sound light palette cells)
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                "${MT2MP_SOURCE_ROOT}/src/game/dungeon_${name}_runtime.inc")
        endforeach()
        foreach(name dungeon_cells_ui_codes.h dungeon_cell_graph_codes.h)
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                "${MT2MP_SOURCE_ROOT}/src/game/${name}")
        endforeach()
        execute_process(COMMAND "${Python3_EXECUTABLE}" -X utf8
            "${dungeon_boundary_root}/check.py" --source "${MT2MP_SOURCE_ROOT}"
            --framework "${MT2_FRAMEWORK_SDK}" --output "${dungeon_boundary_generated}"
            COMMAND_ERROR_IS_FATAL ANY)
        enable_language(C)
        add_library(dungeon_boundary_decoder STATIC
            "${dungeon_boundary_minhook}/trampoline.c"
            "${dungeon_boundary_minhook}/buffer.c"
            "${dungeon_boundary_minhook}/hde/hde64.c")
        add_executable(test_dungeon_boundary "${dungeon_boundary_root}/test_dungeon_boundary.cpp")
        target_link_libraries(test_dungeon_boundary PRIVATE dungeon_boundary_decoder)
        target_include_directories(test_dungeon_boundary PRIVATE
            "${dungeon_boundary_generated}" "${dungeon_boundary_minhook}")
        target_compile_features(test_dungeon_boundary PRIVATE cxx_std_20)
        target_compile_definitions(test_dungeon_boundary PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        target_compile_options(test_dungeon_boundary PRIVATE /W4 /WX /utf-8 /EHsc)
        add_test(NAME dungeon_boundary COMMAND test_dungeon_boundary)
        set_tests_properties(dungeon_boundary PROPERTIES TIMEOUT 30)
    else()
        message(STATUS "Shared native state regression requires MT2_FRAMEWORK_SDK")
    endif()
endif()
