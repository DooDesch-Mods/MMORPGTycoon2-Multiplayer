find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(_vip_wiring_generated "${CMAKE_CURRENT_BINARY_DIR}/vip_frame_wiring_generated")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/bridge.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/bridge.h"
    "${MT2MP_SOURCE_ROOT}/test/extract_vip_frame_wiring.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_vip_frame_wiring.py"
    --source-root "${MT2MP_SOURCE_ROOT}" --output "${_vip_wiring_generated}"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_vip_frame_wiring "${MT2MP_SOURCE_ROOT}/test/test_vip_frame_wiring.cpp")
target_include_directories(test_vip_frame_wiring PRIVATE "${_vip_wiring_generated}"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_vip_frame_wiring PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_vip_frame_wiring PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME vip_frame_wiring COMMAND test_vip_frame_wiring)
set_tests_properties(vip_frame_wiring PROPERTIES TIMEOUT 60)
