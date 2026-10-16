if(WIN32)
add_executable(test_render_pool_guard
    "${MT2MP_SOURCE_ROOT}/test/test_render_pool_guard.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/render_pool_guard/invoke.cpp")
target_include_directories(test_render_pool_guard PRIVATE "${MT2MP_SOURCE_ROOT}/src/game")
target_compile_features(test_render_pool_guard PRIVATE cxx_std_20)
target_compile_definitions(test_render_pool_guard PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(test_render_pool_guard PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME render_pool_guard COMMAND test_render_pool_guard)
set_tests_properties(render_pool_guard PROPERTIES TIMEOUT 45)
endif()
