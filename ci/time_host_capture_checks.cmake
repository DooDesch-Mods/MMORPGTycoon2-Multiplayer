# Reuse the existing fixtures, replacing only their historical staging includes
# with the current production bodies in the build directory.
set(time_fixture_root "${MT2MP_SOURCE_ROOT}/tools/engine_analysis/calendar_authority")
set(time_fixture_build "${CMAKE_CURRENT_BINARY_DIR}/time_host_capture_fixture")
file(MAKE_DIRECTORY "${time_fixture_build}/host_time/staging" "${time_fixture_build}/time_runtime/staging")
foreach(name test_host_time.cpp test_client_regression.cpp)
    configure_file("${time_fixture_root}/host_time/${name}" "${time_fixture_build}/host_time/${name}" COPYONLY)
endforeach()
configure_file("${time_fixture_root}/time_runtime/test_time_runtime.cpp" "${time_fixture_build}/time_runtime/test_time_runtime.cpp" COPYONLY)
foreach(name shared_time.cpp shared_time.h)
    configure_file("${MT2MP_SOURCE_ROOT}/src/game/${name}" "${time_fixture_build}/host_time/staging/${name}" COPYONLY)
endforeach()
foreach(name time_runtime.h time_session.h)
    configure_file("${MT2MP_SOURCE_ROOT}/src/game/${name}" "${time_fixture_build}/time_runtime/staging/${name}" COPYONLY)
endforeach()
# The Runtime fixture includes the frozen Bridge fixture in the same translation
# unit. Both must reach one physical header for pragma once to apply.
file(WRITE "${time_fixture_build}/time_runtime/staging/shared_time.h"
    "#pragma once\n#include \"../../host_time/staging/shared_time.h\"\n")
add_executable(test_time_host_capture "${time_fixture_build}/host_time/test_host_time.cpp")
target_compile_definitions(test_time_host_capture PRIVATE MT2MP_TIME_HOST_CAPTURE)
add_executable(test_time_client_regression "${time_fixture_build}/host_time/test_client_regression.cpp")
add_executable(test_time_runtime "${time_fixture_build}/time_runtime/test_time_runtime.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/time_runtime.cpp" "${MT2MP_SOURCE_ROOT}/src/game/time_session.cpp")
foreach(name time_host_capture time_client_regression time_runtime)
    if(MSVC)
        target_compile_options(test_${name} PRIVATE /W4 /WX /utf-8)
    endif()
    add_test(NAME ${name} COMMAND test_${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endforeach()

add_executable(test_time_client_noop "${MT2MP_SOURCE_ROOT}/test/test_time_client_noop.cpp")
target_include_directories(test_time_client_noop PRIVATE "${time_fixture_build}")
if(MSVC)
    target_compile_options(test_time_client_noop PRIVATE /W4 /WX /utf-8)
endif()
add_test(NAME time_client_noop COMMAND test_time_client_noop)
set_tests_properties(time_client_noop PROPERTIES TIMEOUT 60)
