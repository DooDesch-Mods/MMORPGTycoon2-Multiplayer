if(WIN32)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(construction_source "${MT2MP_SOURCE_ROOT}/src/game/typed_action_runtime.cpp")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${construction_source}"
        "${MT2MP_SOURCE_ROOT}/test/extract_developer_construction.py")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_developer_construction.py"
        --source "${construction_source}" --output "${CMAKE_CURRENT_BINARY_DIR}/construction_extracted"
        COMMAND_ERROR_IS_FATAL ANY)
    foreach(group pool placement)
        add_executable(test_developer_construction_${group} "${MT2MP_SOURCE_ROOT}/test/test_developer_construction_${group}.cpp")
        target_include_directories(test_developer_construction_${group} PRIVATE "${MT2MP_SOURCE_ROOT}/src/game"
            "${MT2MP_SOURCE_ROOT}/include" "${CMAKE_CURRENT_BINARY_DIR}/construction_extracted")
        target_compile_features(test_developer_construction_${group} PRIVATE cxx_std_20)
        if(MSVC)
            target_compile_options(test_developer_construction_${group} PRIVATE /W4 /WX /utf-8 /EHsc)
        endif()
        add_test(NAME test_developer_construction_${group} COMMAND test_developer_construction_${group})
    endforeach()
    target_sources(test_developer_construction_pool PRIVATE "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/shared_state.cpp")
endif()
