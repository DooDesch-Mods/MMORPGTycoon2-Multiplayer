add_executable(test_tutorial_observer
    "${MT2MP_SOURCE_ROOT}/test/test_tutorial_observer.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_observer_read.cpp")
target_include_directories(test_tutorial_observer PRIVATE "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include")
if(MSVC)
    target_compile_options(test_tutorial_observer PRIVATE /W4 /WX /utf-8)
endif()
add_test(NAME tutorial_observer COMMAND test_tutorial_observer)
