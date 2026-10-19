#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace mt2mp::action_session {
enum class GroupKind:uint8_t {Group=1,Ungroup=2};
struct GroupIntent {GroupKind kind=GroupKind::Group;std::vector<std::string> selected;bool operator==(const GroupIntent&)const=default;};
struct GroupMember {std::string key;std::array<uint32_t,10> relative{};bool operator==(const GroupMember&)const=default;};
struct GroupRow {std::string key;int32_t region=-1;std::array<uint32_t,10> pose{};std::vector<GroupMember> members;bool operator==(const GroupRow&)const=default;};
struct GroupProp {std::string key,parent;int32_t region=-1;std::array<uint32_t,10> pose{};bool operator==(const GroupProp&)const=default;};
struct GroupState {std::vector<GroupRow> groups;std::vector<GroupProp> props;std::array<uint64_t,4> random{};bool operator==(const GroupState&)const=default;};
// A saved Group can seed its key from the original first member. Runtime
// identity retains that key when later native edits change or empty the list.
inline std::string group_key(const GroupRow& row){return row.members.empty()?std::string{}:"group:"+row.members.front().key;}
}
