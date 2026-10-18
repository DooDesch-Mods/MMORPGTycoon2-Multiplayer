if(WIN32)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(_disconnect_generated "${CMAKE_CURRENT_BINARY_DIR}/vip_disconnect_generated")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${MT2MP_SOURCE_ROOT}/src/game/scenario_vip_authority/host_witness.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/scenario_vip_authority/alert_client_native.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/scenario_vip_authority/disconnect_invoke.h"
        "${MT2MP_SOURCE_ROOT}/test/test_vip_alert_lifecycle.cpp"
        "${MT2MP_SOURCE_ROOT}/test/test_vip_alert_client_reader.cpp"
        "${MT2MP_SOURCE_ROOT}/test/test_vip_host_witness.cpp"
        "${MT2MP_SOURCE_ROOT}/test/extract_vip_disconnect.py")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_vip_disconnect.py"
        --source-root "${MT2MP_SOURCE_ROOT}" --output "${_disconnect_generated}" COMMAND_ERROR_IS_FATAL ANY)
    add_executable(test_vip_disconnect "${MT2MP_SOURCE_ROOT}/test/test_vip_disconnect.cpp" ${vip_frame_sources})
    add_executable(test_vip_disconnect_host "${MT2MP_SOURCE_ROOT}/test/test_vip_disconnect_host.cpp")
    foreach(target test_vip_disconnect test_vip_disconnect_host)
        target_include_directories(${target} PRIVATE "${_disconnect_generated}" "${MT2MP_SOURCE_ROOT}/test"
            "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${MT2MP_SOURCE_ROOT}/include/vendor/minhook/include")
        target_compile_features(${target} PRIVATE cxx_std_20)
        target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc /Gy)
        target_link_options(${target} PRIVATE /OPT:REF /INCREMENTAL:NO)
        add_test(NAME ${target} COMMAND ${target})
        set_tests_properties(${target} PROPERTIES TIMEOUT 60)
    endforeach()
endif()
