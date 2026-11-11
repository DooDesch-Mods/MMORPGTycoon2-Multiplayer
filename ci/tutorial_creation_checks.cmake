add_executable(test_tutorial_creation_model "${MT2MP_SOURCE_ROOT}/test/test_tutorial_startingpoint_creation_model.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp")
add_library(test_tutorial_creation_native_compile OBJECT "${MT2MP_SOURCE_ROOT}/test/test_tutorial_startingpoint_creation_native_compile.cpp")
foreach(target test_tutorial_creation_model test_tutorial_creation_native_compile)
    target_include_directories(${target} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game"
        "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication" "${MT2MP_SOURCE_ROOT}/include")
    target_compile_features(${target} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    endif()
endforeach()
add_test(NAME tutorial_startingpoint_creation_model COMMAND test_tutorial_creation_model)
set_tests_properties(tutorial_startingpoint_creation_model PROPERTIES TIMEOUT 60)
