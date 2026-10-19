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
