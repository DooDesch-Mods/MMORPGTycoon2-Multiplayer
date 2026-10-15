find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(creation_extract "${CMAKE_CURRENT_BINARY_DIR}/tutorial_creation_extracted")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint_native.inc"
    "${MT2MP_SOURCE_ROOT}/test/test_tutorial_development.cpp")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_tutorial_creation.py"
    --native-inc "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint_native.inc"
    --reader-fixture "${MT2MP_SOURCE_ROOT}/test/test_tutorial_development.cpp"
    --output "${creation_extract}" COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_tutorial_creation_model "${MT2MP_SOURCE_ROOT}/test/test_tutorial_startingpoint_creation_model.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp")
add_executable(test_tutorial_creation_reader "${MT2MP_SOURCE_ROOT}/test/test_tutorial_startingpoint_creation_reader.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_development_read.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/admission_read.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/initial_release/startingpoint_read.cpp")
add_executable(test_tutorial_creation_calls "${MT2MP_SOURCE_ROOT}/test/test_tutorial_startingpoint_creation_calls.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp")
add_library(test_tutorial_creation_native_compile OBJECT "${MT2MP_SOURCE_ROOT}/test/test_tutorial_startingpoint_creation_native_compile.cpp")
foreach(target test_tutorial_creation_model test_tutorial_creation_reader test_tutorial_creation_calls test_tutorial_creation_native_compile)
    target_include_directories(${target} PRIVATE "${creation_extract}" "${MT2MP_SOURCE_ROOT}/src/game"
        "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
endforeach()
add_test(NAME tutorial_startingpoint_creation_model COMMAND test_tutorial_creation_model)
add_test(NAME tutorial_startingpoint_creation_reader COMMAND test_tutorial_creation_reader)
add_test(NAME tutorial_startingpoint_creation_calls COMMAND test_tutorial_creation_calls)
set_tests_properties(tutorial_startingpoint_creation_model tutorial_startingpoint_creation_reader tutorial_startingpoint_creation_calls PROPERTIES TIMEOUT 60)
