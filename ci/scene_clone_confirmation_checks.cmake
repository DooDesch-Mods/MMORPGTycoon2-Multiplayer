find_package(Python3 REQUIRED COMPONENTS Interpreter)
if(NOT DEFINED MT2MP_SOURCE_ROOT)
 get_filename_component(MT2MP_SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
set(SCENE_CONFIRMATION_FIXTURE "${MT2MP_SOURCE_ROOT}/test/scene_clone_confirmation")
set(SCENE_CONFIRMATION_SOURCE "${MT2MP_SOURCE_ROOT}/src/game/party_frame_native.cpp")
set(SCENE_CONFIRMATION_RUNTIME "${MT2MP_SOURCE_ROOT}/src/game/scene_clone/runtime.cpp")
set(SCENE_CONFIRMATION_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/scene_clone_confirmation_generated.cpp")
add_custom_command(OUTPUT "${SCENE_CONFIRMATION_GENERATED}"
 COMMAND "${Python3_EXECUTABLE}" -X utf8 "${SCENE_CONFIRMATION_FIXTURE}/extract_capture.py" --candidate "${SCENE_CONFIRMATION_SOURCE}" --output "${SCENE_CONFIRMATION_GENERATED}"
 DEPENDS "${SCENE_CONFIRMATION_FIXTURE}/extract_capture.py" "${SCENE_CONFIRMATION_FIXTURE}/capture_fixture.in" "${SCENE_CONFIRMATION_SOURCE}" "${SCENE_CONFIRMATION_RUNTIME}" "${SCENE_CONFIRMATION_FIXTURE}/baseline_party_frame_native.txt"
 VERBATIM)
add_executable(party_scene_capture "${SCENE_CONFIRMATION_GENERATED}")
target_compile_features(party_scene_capture PRIVATE cxx_std_20)
if(MSVC)
 target_compile_options(party_scene_capture PRIVATE /utf-8 /EHsc)
endif()
add_test(NAME party_scene_capture COMMAND party_scene_capture)
