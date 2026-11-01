find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/live_host_state/native_port.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/live_host_state/native_port.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/live_host_state/state.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/live_host_state/state.h"
    "${CMAKE_CURRENT_LIST_DIR}/../src/game/native_engine.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/../include/mt2mp/game/native_engine.h"
    "${CMAKE_CURRENT_LIST_DIR}/../test/extract_native_owner.py")
set(native_owner_generated "${CMAKE_CURRENT_BINARY_DIR}/native_owner_generated")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../test/extract_native_owner.py"
    --root "${CMAKE_CURRENT_LIST_DIR}/.." --output "${native_owner_generated}"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_native_owner "${CMAKE_CURRENT_LIST_DIR}/../test/test_native_owner.cpp")
target_compile_features(test_native_owner PRIVATE cxx_std_20)
target_include_directories(test_native_owner PRIVATE "${native_owner_generated}")
if(MSVC)
    target_compile_options(test_native_owner PRIVATE /W4 /WX /utf-8)
else()
    target_compile_options(test_native_owner PRIVATE -Wall -Wextra -Werror)
endif()
add_test(NAME native_owner_only COMMAND test_native_owner)
