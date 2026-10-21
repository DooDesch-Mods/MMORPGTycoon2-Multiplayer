set(MT2MP_PLACEMENT_INPUT_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/placement-input-order")
add_custom_command(
    OUTPUT "${MT2MP_PLACEMENT_INPUT_GENERATED}/placement_input_order.inc"
        "${MT2MP_PLACEMENT_INPUT_GENERATED}/placement_observer_order.inc"
    COMMAND "${Python3_EXECUTABLE}" "${MT2MP_SOURCE_ROOT}/test/extract_placement_input_order.py"
        --source "${MT2MP_SOURCE_ROOT}" --output "${MT2MP_PLACEMENT_INPUT_GENERATED}"
    DEPENDS
        "${MT2MP_SOURCE_ROOT}/test/extract_placement_input_order.py"
        "${MT2MP_SOURCE_ROOT}/src/game/placement_ui_probe/ui_probe.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/placement_ui_diagnostics/placement_ui_diagnostics.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/placement_ui_diagnostics/admission.cpp"
        "${MT2MP_SOURCE_ROOT}/src/game/ui_build_runtime.cpp"
    VERBATIM)
add_executable(mt2mp_test_placement_input_order
    "${MT2MP_SOURCE_ROOT}/test/test_placement_input_order.cpp"
    "${MT2MP_SOURCE_ROOT}/src/game/placement_ui_diagnostics/admission.cpp"
    "${MT2MP_PLACEMENT_INPUT_GENERATED}/placement_input_order.inc"
    "${MT2MP_PLACEMENT_INPUT_GENERATED}/placement_observer_order.inc")
target_include_directories(mt2mp_test_placement_input_order PRIVATE
    "${MT2MP_SOURCE_ROOT}/src/game" "${MT2MP_SOURCE_ROOT}/include" "${MT2MP_PLACEMENT_INPUT_GENERATED}")
target_compile_features(mt2mp_test_placement_input_order PRIVATE cxx_std_20)
add_test(NAME placement_input_order COMMAND mt2mp_test_placement_input_order)
