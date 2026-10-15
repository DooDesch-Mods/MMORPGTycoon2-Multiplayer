add_executable(test_tutorial_development
    "${MT2MP_SOURCE_ROOT}/test/test_tutorial_development.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_development_read.cpp")
target_include_directories(test_tutorial_development PRIVATE
    "${MT2MP_SOURCE_ROOT}/src/game"
    "${MT2MP_SOURCE_ROOT}/include")
if(MSVC)
    target_compile_options(test_tutorial_development PRIVATE /W4 /WX /utf-8)
endif()
add_test(NAME tutorial_development COMMAND test_tutorial_development)
set_tests_properties(tutorial_development PROPERTIES TIMEOUT 30)
