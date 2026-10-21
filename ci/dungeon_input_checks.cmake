foreach(name dungeon_input_native dungeon_fill_record)
    add_executable(test_${name} "${MT2MP_SOURCE_ROOT}/test/test_${name}.cpp")
    target_include_directories(test_${name} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(test_${name} PRIVATE cxx_std_20)
    target_compile_definitions(test_${name} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(test_${name} PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME ${name} COMMAND test_${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endforeach()
if(WIN32)
    add_executable(test_dungeon_fill_decode "${MT2MP_SOURCE_ROOT}/test/test_dungeon_fill_decode.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/record_decode.cpp")
    target_include_directories(test_dungeon_fill_decode PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(test_dungeon_fill_decode PRIVATE cxx_std_20)
    target_compile_definitions(test_dungeon_fill_decode PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    if(MSVC)
        target_compile_options(test_dungeon_fill_decode PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
    add_test(NAME dungeon_fill_decode COMMAND test_dungeon_fill_decode)
    set_tests_properties(dungeon_fill_decode PROPERTIES TIMEOUT 60)
endif()

# Compile the actual current Scenery readers and result fence in the existing native fixture.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(scene_scenery_native_output "${CMAKE_CURRENT_BINARY_DIR}/scene_scenery_native")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/test/extract_scene_scenery_native.py"
    "${MT2MP_SOURCE_ROOT}/src/game/dungeon_input_sample.inc"
    "${MT2MP_SOURCE_ROOT}/src/game/dungeon_gizmo_control_capture.inc")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B -X utf8
    "${MT2MP_SOURCE_ROOT}/test/extract_scene_scenery_native.py"
    --source "${MT2MP_SOURCE_ROOT}" --output "${scene_scenery_native_output}"
    COMMAND_ERROR_IS_FATAL ANY)
target_include_directories(test_dungeon_input_native PRIVATE "${scene_scenery_native_output}")
target_sources(test_dungeon_input_native PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/scene_clone/state.cpp")
