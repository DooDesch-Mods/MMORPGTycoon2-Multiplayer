#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace mt2mp::action_session {
struct DungeonInstanceRecord {
    int32_t index=-1;bool started=false;
    bool operator==(const DungeonInstanceRecord&)const=default;
};
struct DungeonConfigIntent {
    std::string key;int32_t before=0,desired=0;
    bool operator==(const DungeonConfigIntent&)const=default;
};
struct DungeonConfigState {
    int32_t desired=0,price=0;bool sandbox=false;
    std::vector<DungeonInstanceRecord> instances;
    std::array<uint64_t,4> global_random{};uint32_t undo_count=0;
    bool operator==(const DungeonConfigState&)const=default;
};
inline bool dungeon_config_transition(const DungeonConfigIntent& intent,const DungeonConfigState& before,const DungeonConfigState& after){
    return before.desired==intent.before&&after.desired==intent.desired&&before.instances==after.instances&&before.price==after.price&&before.sandbox==after.sandbox&&before.undo_count==after.undo_count;
}
}
