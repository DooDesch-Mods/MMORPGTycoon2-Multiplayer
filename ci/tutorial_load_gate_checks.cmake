file(READ "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/native.cpp" tutorial_native_text)
string(FIND "${tutorial_native_text}" "bool finish_client_load(" gate_begin)
string(FIND "${tutorial_native_text}" "\nvoid invalidate(" gate_end)
if(gate_begin LESS 0 OR gate_end LESS gate_begin)
    message(FATAL_ERROR "Tutorial load gate boundaries changed; update the affected test extraction")
endif()
math(EXPR gate_size "${gate_end}-${gate_begin}")
string(SUBSTRING "${tutorial_native_text}" ${gate_begin} ${gate_size} gate_body)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/load_gate_body.inc" "${gate_body}")
add_executable(test_tutorial_load_gate "${MT2MP_SOURCE_ROOT}/test/test_tutorial_load_gate.cpp")
target_include_directories(test_tutorial_load_gate PRIVATE
    "${MT2MP_SOURCE_ROOT}/src"
    "${MT2MP_SOURCE_ROOT}/include"
    "${CMAKE_CURRENT_BINARY_DIR}")
if(MSVC)
    target_compile_options(test_tutorial_load_gate PRIVATE /W4 /WX /utf-8)
endif()
add_test(NAME tutorial_load_gate COMMAND test_tutorial_load_gate)
set_tests_properties(tutorial_load_gate PROPERTIES TIMEOUT 30)
