# The plugin already requires this exact SDK. Do not replace its ABI with doubles.
if(WIN32 AND MT2_FRAMEWORK_SDK)
    foreach(header include/mt2fw/plugin.h adapters/mt2/sdk/native_services.h adapters/mt2/sdk/native_routes.h adapters/mt2/sdk/native_ownership.h)
        if(NOT EXISTS "${MT2_FRAMEWORK_SDK}/${header}")
            message(FATAL_ERROR "Monster hook admission requires Framework SDK header: ${header}")
        endif()
    endforeach()
    add_executable(test_monster_hook_admission
        "${MT2MP_SOURCE_ROOT}/test/test_monster_hook_admission.cpp"
        "${MT2MP_SOURCE_ROOT}/src/hooks/framework_backend.cpp")
    target_include_directories(test_monster_hook_admission PRIVATE
        "${MT2MP_SOURCE_ROOT}/include" "${MT2MP_SOURCE_ROOT}/include/vendor/minhook/include"
        "${MT2MP_SOURCE_ROOT}/src/hooks"
        "${MT2_FRAMEWORK_SDK}/include" "${MT2_FRAMEWORK_SDK}/adapters/mt2/sdk")
    target_compile_features(test_monster_hook_admission PRIVATE cxx_std_20)
    target_compile_definitions(test_monster_hook_admission PRIVATE MT2MP_FRAMEWORK_PLUGIN NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(test_monster_hook_admission PRIVATE /W4 /WX /EHsc /utf-8)
    add_test(NAME monster_hook_admission COMMAND test_monster_hook_admission)
    set_tests_properties(monster_hook_admission PROPERTIES TIMEOUT 60)
else()
    message(STATUS "Monster hook admission requires Windows and MT2_FRAMEWORK_SDK; not configured in standalone source checks")
endif()
