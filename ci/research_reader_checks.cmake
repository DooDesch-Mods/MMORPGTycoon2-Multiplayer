add_executable(test_research_reader "${MT2MP_SOURCE_ROOT}/test/test_research_reader.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_development_read.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/initial_release/startingpoint_read.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/admission_read.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/tutorial_replication/startingpoint.cpp")
target_include_directories(test_research_reader PRIVATE "${MT2MP_SOURCE_ROOT}/src/game"
    "${MT2MP_SOURCE_ROOT}/include" "${CMAKE_CURRENT_BINARY_DIR}/startingpoint_client_extracted")
target_compile_features(test_research_reader PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(test_research_reader PRIVATE /W4 /WX /utf-8 /EHsc)
endif()
add_test(NAME research_reader COMMAND test_research_reader)
set_tests_properties(research_reader PROPERTIES TIMEOUT 60)
