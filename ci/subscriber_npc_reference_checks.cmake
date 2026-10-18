if(WIN32)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(_npc_ref_generated "${CMAKE_CURRENT_BINARY_DIR}/subscriber_npc_reference_generated")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_subscriber_npc_reference.py"
        --root "${MT2MP_SOURCE_ROOT}" --output "${_npc_ref_generated}" COMMAND_ERROR_IS_FATAL ANY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/npc_reference_pins.h"
        "${MT2MP_SOURCE_ROOT}/src/game/npc_host_state_binding.h"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/runtime_capture.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/runtime_append.cpp"
        "${MT2MP_SOURCE_ROOT}/test/extract_subscriber_npc_reference.py")
    add_executable(test_subscriber_npc_reference "${MT2MP_SOURCE_ROOT}/test/test_subscriber_npc_reference.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/reference_map.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/payload.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/subscriber_lifecycle/record_decode.cpp")
    target_include_directories(test_subscriber_npc_reference PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${_npc_ref_generated}")
    target_compile_features(test_subscriber_npc_reference PRIVATE cxx_std_20)
    target_compile_definitions(test_subscriber_npc_reference PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(test_subscriber_npc_reference PRIVATE /W4 /WX /utf-8 /EHsc)
    target_link_libraries(test_subscriber_npc_reference PRIVATE bcrypt)
    add_test(NAME subscriber_npc_reference COMMAND test_subscriber_npc_reference)
endif()
