set(_construction_root "${MT2MP_SOURCE_ROOT}")
if(NOT _construction_root)
    set(_construction_root "${CMAKE_SOURCE_DIR}")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_construction_root}/src/game/developer_staff/construction_native.cpp"
    "${_construction_root}/test/extract_developer_construction_transition.py")
execute_process(COMMAND python "${_construction_root}/test/extract_developer_construction_retire.py"
    --source "${_construction_root}/src/game/developer_staff/construction_native.cpp"
    --output "${CMAKE_CURRENT_BINARY_DIR}/construction_retire_body.inc"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_developer_construction_plan "${_construction_root}/test/test_developer_construction_plan.cpp")
add_executable(test_developer_construction_retire "${_construction_root}/test/test_developer_construction_retire.cpp")
add_executable(test_developer_construction_links "${_construction_root}/test/test_developer_construction_links.cpp"
    "${_construction_root}/src/game/subscriber_lifecycle/cohort.cpp")
execute_process(COMMAND python "${_construction_root}/test/extract_developer_construction_transition.py"
    --source "${_construction_root}/src/game/developer_staff/construction_native.cpp"
    --output "${CMAKE_CURRENT_BINARY_DIR}/construction_transition"
    COMMAND_ERROR_IS_FATAL ANY)
add_executable(test_developer_construction_transition "${_construction_root}/test/test_developer_construction_transition.cpp"
    "${_construction_root}/src/game/developer_staff/shared_state.cpp")
target_include_directories(test_developer_construction_transition PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/construction_transition")
foreach(target test_developer_construction_plan test_developer_construction_retire test_developer_construction_links test_developer_construction_transition)
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_compile_options(${target} PRIVATE /W4 /WX /utf-8 /EHsc)
    target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
    target_include_directories(${target} PRIVATE "${_construction_root}/src/game" "${_construction_root}/include" "${CMAKE_CURRENT_BINARY_DIR}")
endforeach()
add_test(NAME developer_construction_plan COMMAND test_developer_construction_plan)
add_test(NAME developer_construction_retire COMMAND test_developer_construction_retire)
add_test(NAME developer_construction_links COMMAND test_developer_construction_links)
add_test(NAME developer_construction_transition COMMAND test_developer_construction_transition)
