if(WIN32)
    add_executable(test_steam_avatars ../test/test_steam_avatars.cpp ../src/net/steam_avatars.cpp)
    target_include_directories(test_steam_avatars PRIVATE ../include)
    target_compile_definitions(test_steam_avatars PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(test_steam_avatars PRIVATE /UNDEBUG /utf-8)
    target_link_libraries(test_steam_avatars PRIVATE bcrypt)
    add_test(NAME steam_avatar_provider COMMAND test_steam_avatars "${CMAKE_CURRENT_BINARY_DIR}/avatar-profile")
    set_tests_properties(steam_avatar_provider PROPERTIES FIXTURES_SETUP avatar_png)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(NAME steam_avatar_png_decode COMMAND "${Python3_EXECUTABLE}" -B -X utf8
        "${CMAKE_CURRENT_SOURCE_DIR}/../test/check_avatar_png.py" "${CMAKE_CURRENT_BINARY_DIR}/avatar-profile")
    set_tests_properties(steam_avatar_png_decode PROPERTIES FIXTURES_REQUIRED avatar_png)
    # The PNG consumer and its native provider share one runner and output directory.
    set_tests_properties(steam_avatar_provider steam_avatar_png_decode
        PROPERTIES LABELS "source-target:test_steam_avatars")
endif()
