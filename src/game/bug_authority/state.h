#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace mt2mp::game::bug_authority {
enum class ParentKind { Building, Npc };
enum class BirthKind { Snapshot, Action, Publication };
struct Birth {
    BirthKind kind=BirthKind::Snapshot;
    std::string source;
    uint64_t sequence=0,occurrence=0;
    bool operator==(const Birth&)const=default;
};
struct Value {
    int32_t type=0,created=-1,discovered=-1,toons=0,variant=-1;
    float reproducibility=0;
    bool known=false,manual=false,debugged=false;
    bool operator==(const Value&)const=default;
};
struct Row {
    ParentKind parent_kind=ParentKind::Building;
    std::string parent_key;
    Birth birth;
    Value value;
    bool operator==(const Row&)const=default;
};
// The complete set of live Bugs in the exact bound world. Missing witnessed
// occurrences retire individually; an empty state is authoritative.
struct State {
    std::vector<Row> rows;
    bool operator==(const State&)const=default;
};
}
