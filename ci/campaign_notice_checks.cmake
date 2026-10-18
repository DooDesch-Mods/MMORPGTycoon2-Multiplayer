if(WIN32)
    set(_campaign_generated "${CMAKE_CURRENT_BINARY_DIR}/campaign_notice_generated")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${MT2MP_SOURCE_ROOT}/src/game/marketing_authority/notice_runtime.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/marketing_authority/notice_hook.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/marketing_authority/native_port.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/marketing_native_probe/runtime.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/scenario_vip_authority/disconnect_invoke.h"
        "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/bridge.cpp"
        "${MT2MP_SOURCE_ROOT}/test/test_vip_alert_client_reader.cpp"
        "${MT2MP_SOURCE_ROOT}/test/extract_campaign_notice.py")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_campaign_notice.py"
        --source-root "${MT2MP_SOURCE_ROOT}" --output "${_campaign_generated}" COMMAND_ERROR_IS_FATAL ANY)
    add_executable(test_campaign_notice "${MT2MP_SOURCE_ROOT}/test/test_campaign_notice.cpp" ${vip_frame_sources})
    add_executable(test_campaign_notice_witness "${MT2MP_SOURCE_ROOT}/test/test_campaign_notice_witness.cpp")
    foreach(_target test_campaign_notice test_campaign_notice_witness)
        target_include_directories(${_target} PRIVATE "${_campaign_generated}" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
        target_compile_features(${_target} PRIVATE cxx_std_20)
        target_compile_definitions(${_target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
        target_compile_options(${_target} PRIVATE /W4 /WX /utf-8 /EHsc /Gy)
        target_link_options(${_target} PRIVATE /OPT:REF /INCREMENTAL:NO)
        add_test(NAME ${_target} COMMAND ${_target})
        set_tests_properties(${_target} PROPERTIES TIMEOUT 60)
    endforeach()
endif()
