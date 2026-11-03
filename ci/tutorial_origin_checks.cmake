if(WIN32 AND MSVC)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(tutorial_origin_generated "${CMAKE_CURRENT_BINARY_DIR}/tutorial_origin_extracted")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/native.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/intro_native.inc"
    "${MT2MP_SOURCE_ROOT}/test/extract_tutorial_origin.py")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_tutorial_origin.py"
    --source "${MT2MP_SOURCE_ROOT}" --output "${tutorial_origin_generated}" COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_tutorial_ordered_origin "${MT2MP_SOURCE_ROOT}/test/test_tutorial_ordered_origin.cpp")
target_compile_features(test_tutorial_ordered_origin PRIVATE cxx_std_20)
target_include_directories(test_tutorial_ordered_origin PRIVATE "${tutorial_origin_generated}" "${MT2MP_SOURCE_ROOT}/include")
target_compile_options(test_tutorial_ordered_origin PRIVATE /W4 /WX /EHsc /utf-8
    "/external:I${MT2MP_SOURCE_ROOT}/include/vendor" /external:W0)
add_test(NAME tutorial_ordered_origin COMMAND test_tutorial_ordered_origin)
set_tests_properties(tutorial_ordered_origin PROPERTIES TIMEOUT 60)
endif()
