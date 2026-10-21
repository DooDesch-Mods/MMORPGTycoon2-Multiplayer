if(WIN32)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(_building_types_body "${CMAKE_CURRENT_BINARY_DIR}/building_types_extracted")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/construction_native.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/developer_staff/read.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/remove_diagnostics.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/scenery_kiss/native_catalog.cpp"
        "${MT2MP_SOURCE_ROOT}/test/extract_building_types.py")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_building_types.py"
        --root "${MT2MP_SOURCE_ROOT}" --output "${_building_types_body}"
        COMMAND_ERROR_IS_FATAL ANY)
    add_executable(test_building_types_construction "${MT2MP_SOURCE_ROOT}/test/test_building_types_construction.cpp")
    target_compile_features(test_building_types_construction PRIVATE cxx_std_20)
    target_compile_options(test_building_types_construction PRIVATE /W4 /WX /utf-8 /EHsc)
    target_compile_definitions(test_building_types_construction PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(test_building_types_construction PRIVATE
        "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${_building_types_body}")
    add_test(NAME building_types_construction COMMAND test_building_types_construction)
endif()
