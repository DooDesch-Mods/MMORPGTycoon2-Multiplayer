# Production queue/controller sources and configure-time bootstrap extraction.
set(ordered_test_root "${CMAKE_CURRENT_LIST_DIR}/../test/ordered")
set(ordered_generated "${CMAKE_CURRENT_BINARY_DIR}/ordered-generated")
foreach(name queue wire controller snapshot_barrier)
    add_executable(test_ordered_${name} "${ordered_test_root}/test_${name}.cpp")
    target_link_libraries(test_ordered_${name} PRIVATE source_check_codecs_windows)
    target_include_directories(test_ordered_${name} PRIVATE
        "${MT2MP_SOURCE_ROOT}/src/game/typed_action" "${MT2MP_SOURCE_ROOT}/src/game"
        "${MT2MP_SOURCE_ROOT}/src" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(test_ordered_${name} PRIVATE cxx_std_20)
    target_compile_definitions(test_ordered_${name} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(test_ordered_${name} PRIVATE /W4 /WX /utf-8 /EHsc /bigobj)
        target_link_options(test_ordered_${name} PRIVATE /STACK:8388608)
    endif()
    add_test(NAME ordered_${name} COMMAND test_ordered_${name})
    set_tests_properties(ordered_${name} PROPERTIES TIMEOUT 60 LABELS ordered)
endforeach()

find_package(Python3 COMPONENTS Interpreter REQUIRED)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_movement/camera_native.inc" "${ordered_test_root}/extract_tutorial_camera_native.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" -X utf8 "${ordered_test_root}/extract_tutorial_camera_native.py"
    --source "${MT2MP_SOURCE_ROOT}" --output "${ordered_generated}/ordered_tutorial_camera_native_body.inc" COMMAND_ERROR_IS_FATAL ANY)
foreach(name tutorial_camera tutorial_camera_native)
    add_executable(test_ordered_${name} "${ordered_test_root}/test_${name}.cpp")
    target_include_directories(test_ordered_${name} PRIVATE "${ordered_generated}" "${MT2MP_SOURCE_ROOT}/src" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(test_ordered_${name} PRIVATE cxx_std_20)
    target_compile_definitions(test_ordered_${name} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(test_ordered_${name} PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME ordered_${name} COMMAND test_ordered_${name})
    set_tests_properties(ordered_${name} PROPERTIES TIMEOUT 60 LABELS ordered)
endforeach()

target_sources(test_ordered_queue PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/placement_ui_diagnostics/admission.cpp")

if(WIN32)
    set(MT2MP_ORDERED_FRAMEWORK_SDK "${MT2_FRAMEWORK_SDK}" CACHE PATH "Framework checkout for ordered source checks")
    foreach(header include/mt2fw/plugin.h adapters/mt2/sdk/native_steps_draft.h)
        if(NOT EXISTS "${MT2MP_ORDERED_FRAMEWORK_SDK}/${header}")
            message(FATAL_ERROR "Ordered source checks require the pinned Framework SDK: ${header}; set MT2MP_ORDERED_FRAMEWORK_SDK")
        endif()
    endforeach()
    # Network identity includes the public Runtime declarations with Step ABI.
    foreach(target test_empty_network test_building_remove_restore_history test_bug_authority_runtime test_move_history developer_staff_native_compile)
        if(TARGET ${target})
            target_include_directories(${target} SYSTEM PRIVATE
                "${MT2MP_ORDERED_FRAMEWORK_SDK}/include" "${MT2MP_ORDERED_FRAMEWORK_SDK}/adapters/mt2/sdk")
        endif()
    endforeach()
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${MT2MP_SOURCE_ROOT}/src/net/ordered_snapshot_load.inc"
        "${ordered_test_root}/extract_bootstrap.py")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -X utf8 "${ordered_test_root}/extract_bootstrap.py"
        --source "${MT2MP_SOURCE_ROOT}" --output "${ordered_generated}/ordered_bootstrap_body.inc"
        COMMAND_ERROR_IS_FATAL ANY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${MT2MP_SOURCE_ROOT}/src/game/typed_action/ordered_save_request.inc"
        "${ordered_test_root}/extract_periodic_service.py")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -X utf8 "${ordered_test_root}/extract_periodic_service.py"
        --source "${MT2MP_SOURCE_ROOT}" --output "${ordered_generated}/ordered_periodic_service_body.inc"
        COMMAND_ERROR_IS_FATAL ANY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${MT2MP_SOURCE_ROOT}/src/game/native_save_request.inc" "${ordered_test_root}/extract_periodic_countdown.py")
    execute_process(COMMAND "${Python3_EXECUTABLE}" -X utf8 "${ordered_test_root}/extract_periodic_countdown.py"
        --source "${MT2MP_SOURCE_ROOT}" --output "${ordered_generated}/ordered_periodic_countdown_body.inc"
        COMMAND_ERROR_IS_FATAL ANY)
    foreach(name bootstrap lifecycle periodic_service periodic_countdown)
        add_executable(test_ordered_${name} "${ordered_test_root}/test_${name}.cpp")
        target_include_directories(test_ordered_${name} PRIVATE "${ordered_generated}"
            "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include"
            "${MT2MP_ORDERED_FRAMEWORK_SDK}/include" "${MT2MP_ORDERED_FRAMEWORK_SDK}/adapters/mt2/sdk")
        target_compile_features(test_ordered_${name} PRIVATE cxx_std_20)
        target_compile_definitions(test_ordered_${name} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        if(MSVC)
            # The bootstrap assertions must also run in the CI Release build.
            target_compile_options(test_ordered_${name} PRIVATE /W4 /WX /utf-8 /EHsc /UNDEBUG)
        else()
            target_compile_options(test_ordered_${name} PRIVATE -UNDEBUG)
        endif()
        add_test(NAME ordered_${name} COMMAND test_ordered_${name})
        set_tests_properties(ordered_${name} PROPERTIES TIMEOUT 60 LABELS ordered)
    endforeach()
endif()

# Header-only native tutorial readback checks run in Release as well as Debug.
add_executable(test_ordered_tutorial_receipts "${ordered_test_root}/test_tutorial_receipts.cpp")
target_include_directories(test_ordered_tutorial_receipts PRIVATE
    "${MT2MP_SOURCE_ROOT}/src" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_ordered_tutorial_receipts PRIVATE cxx_std_20)
target_compile_definitions(test_ordered_tutorial_receipts PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_ordered_tutorial_receipts PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME ordered_tutorial_receipts COMMAND test_ordered_tutorial_receipts)
set_tests_properties(ordered_tutorial_receipts PROPERTIES TIMEOUT 60 LABELS ordered)

# Exercise the actual specialized retained owner used by Start, Workers and Commit.
find_package(Python3 COMPONENTS Interpreter REQUIRED)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action_runtime.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/typed_action/action_session.cpp"
    "${ordered_test_root}/extract_startingpoint_receipts.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" -X utf8 "${ordered_test_root}/extract_startingpoint_receipts.py"
    --source "${MT2MP_SOURCE_ROOT}" --output "${ordered_generated}"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_ordered_startingpoint_receipts "${ordered_test_root}/test_startingpoint_receipts.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp")
target_include_directories(test_ordered_startingpoint_receipts PRIVATE
    "${ordered_generated}" "${MT2MP_SOURCE_ROOT}/src" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_ordered_startingpoint_receipts PRIVATE cxx_std_20)
target_compile_definitions(test_ordered_startingpoint_receipts PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_ordered_startingpoint_receipts PRIVATE /W4 /WX /utf-8 /EHsc)
    target_link_options(test_ordered_startingpoint_receipts PRIVATE /STACK:8388608)
endif()
add_test(NAME ordered_startingpoint_receipts COMMAND test_ordered_startingpoint_receipts)
set_tests_properties(ordered_startingpoint_receipts PROPERTIES TIMEOUT 60 LABELS ordered)

# Run the actual native placement operation and wrappers against controlled ports.
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/native_engine.cpp"
    "${ordered_test_root}/extract_placement_probe.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" -X utf8 "${ordered_test_root}/extract_placement_probe.py"
    --source "${MT2MP_SOURCE_ROOT}" --output "${ordered_generated}"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_ordered_placement_probe "${ordered_test_root}/test_placement_probe.cpp")
target_include_directories(test_ordered_placement_probe PRIVATE
    "${ordered_generated}" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_ordered_placement_probe PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_ordered_placement_probe PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME ordered_placement_probe COMMAND test_ordered_placement_probe)
set_tests_properties(ordered_placement_probe PROPERTIES TIMEOUT 60 LABELS ordered)

# Keep real periodic rotation and exclusive fresh publication covered together.
add_executable(test_ordered_periodic_companion_rotation "${ordered_test_root}/test_periodic_companion_rotation.cpp")
target_include_directories(test_ordered_periodic_companion_rotation PRIVATE
    "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_ordered_periodic_companion_rotation PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_ordered_periodic_companion_rotation PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME ordered_periodic_companion_rotation COMMAND test_ordered_periodic_companion_rotation
    "${CMAKE_CURRENT_BINARY_DIR}/periodic-companion-files")
set_tests_properties(ordered_periodic_companion_rotation PROPERTIES TIMEOUT 60 LABELS ordered)
