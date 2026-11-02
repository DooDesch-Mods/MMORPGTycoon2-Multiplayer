struct SelectionMoveRow {
    std::string key;
    BuildingState before,after;
    bool operator==(const SelectionMoveRow&) const = default;
};
struct SelectionMoveIntent {
    std::vector<SelectionMoveRow> rows;
    bool operator==(const SelectionMoveIntent&) const = default;
};
struct SelectionMoveState {
    std::vector<BuildingState> rows;
    bool operator==(const SelectionMoveState&) const = default;
};
struct SelectionMoveSettlement {
    uint32_t index=0;std::string key;uint32_t immediate_y_bits=0,ground_y_bits=0;
    bool operator==(const SelectionMoveSettlement&) const = default;
};
