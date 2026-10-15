set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/native.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint_native.inc"
    "${MT2MP_SOURCE_ROOT}/test/test_tutorial_development.cpp")
add_executable(test_startingpoint_client "${MT2MP_SOURCE_ROOT}/test/test_tutorial_startingpoint_client.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/state.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/live_host_state/network/node_state.cpp")
target_include_directories(test_startingpoint_client PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
target_compile_features(test_startingpoint_client PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_startingpoint_client PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME startingpoint_client COMMAND test_startingpoint_client)
set_tests_properties(startingpoint_client PROPERTIES TIMEOUT 60)
add_test(NAME startingpoint_client_guards COMMAND test_startingpoint_client guards)
set_tests_properties(startingpoint_client_guards PROPERTIES TIMEOUT 60)
add_test(NAME startingpoint_commit_receipts COMMAND test_startingpoint_client receipts)
set_tests_properties(startingpoint_commit_receipts PROPERTIES TIMEOUT 60)
add_test(NAME startingpoint_initial_release_guard COMMAND test_startingpoint_client first_release)
set_tests_properties(startingpoint_initial_release_guard PROPERTIES TIMEOUT 60)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_tutorial_startingpoint_native.py"
    --native "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/native.cpp"
    --reader-fixture "${MT2MP_SOURCE_ROOT}/test/test_tutorial_development.cpp"
    --output "${CMAKE_CURRENT_BINARY_DIR}/startingpoint_client_extracted"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_startingpoint_scope "${MT2MP_SOURCE_ROOT}/test/test_tutorial_startingpoint_scope.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp" "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/state.cpp")
add_executable(test_startingpoint_reader "${MT2MP_SOURCE_ROOT}/test/test_tutorial_startingpoint_reader.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_development_read.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/admission_read.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/initial_release/startingpoint_read.cpp")
add_executable(test_startingpoint_commit_guard "${MT2MP_SOURCE_ROOT}/test/test_tutorial_commit_guard.cpp")
add_executable(test_startingpoint_commit_lifetime "${MT2MP_SOURCE_ROOT}/test/test_tutorial_commit_lifetime.cpp")
foreach(target test_startingpoint_scope test_startingpoint_reader test_startingpoint_commit_guard test_startingpoint_commit_lifetime)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/startingpoint_client_extracted" "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
endforeach()
add_test(NAME startingpoint_native_scope COMMAND test_startingpoint_scope)
add_test(NAME startingpoint_snapshot_reader COMMAND test_startingpoint_reader)
add_test(NAME startingpoint_owned_commit_guard COMMAND test_startingpoint_commit_guard)
add_test(NAME startingpoint_commit_lifetime COMMAND test_startingpoint_commit_lifetime)
set_tests_properties(startingpoint_native_scope startingpoint_snapshot_reader PROPERTIES TIMEOUT 60)
