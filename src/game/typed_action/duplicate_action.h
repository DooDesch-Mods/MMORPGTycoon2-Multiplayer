#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
namespace mt2mp::action_session {
enum class DuplicateFamily:uint8_t {Building=1,Scenery=2,Group=3};
struct DuplicateCash {
    int32_t amount=0;std::array<uint32_t,3> position{};
    bool operator==(const DuplicateCash&)const=default;
};
struct DuplicateTarget {
    std::string key,record_sha;
    DuplicateFamily family=DuplicateFamily::Building;
    int32_t region=-1,cost=0;
    std::array<uint32_t,10> before{},after{};
    // Group children retain their complete native member order. Their stable
    // output UUIDs are supplied once in the intent, never allocated per peer.
    std::vector<std::string> members,created_keys;
    // Captured by the original Building Save. Other families and older
    // intents retain the strict complete-current-record precondition.
    std::optional<std::vector<int32_t>> captured_uptime;
    bool operator==(const DuplicateTarget&)const=default;
};
struct DuplicateIntent {
    std::vector<DuplicateTarget> rows;
    std::array<uint32_t,3> cash_position{};
    bool operator==(const DuplicateIntent&)const=default;
};
struct DuplicateObject {
    std::string key,parent,record_sha;
    DuplicateFamily family=DuplicateFamily::Building;
    int32_t region=-1;
    std::array<uint32_t,10> pose{};
    std::vector<std::string> members;
    bool operator==(const DuplicateObject&)const=default;
};
struct DuplicateHistoryEffects {
    std::string services_sha,quests_sha,related_sha;
    std::vector<std::string> related_keys;
    bool operator==(const DuplicateHistoryEffects&)const=default;
};
struct DuplicateState {
    std::vector<DuplicateObject> sources,created;
    std::string untouched_sha;
    std::array<uint64_t,4> random{};
    std::array<int32_t,8> release{};
    std::optional<DuplicateHistoryEffects> history;
    bool operator==(const DuplicateState&)const=default;
};
}
