#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <mutex>

namespace mt2mp::subscriber_lifecycle {
struct Member {
    int32_t id = -1;
    uintptr_t subscriber = 0, toon = 0, brain = 0;
    bool online = false;
    bool operator==(const Member&) const = default;
};
// Addresses remain private. Portable records use serialized IDs and named world keys.
struct Census {
    uint64_t epoch = 0, origin_generation = 0;
    uintptr_t world = 0, manager = 0;
    int32_t next_id = 0;
    std::vector<Member> members;
    std::vector<int32_t> online, offline;
};
struct Addition { int32_t id = -1; bool online = false; };
struct Plan {
    Census before;
    std::vector<Addition> additions;
    int32_t next_id = 0;
};
bool plan_additions(const Census& before, const std::vector<Addition>& additions,
                    int32_t host_next_id, Plan&, std::string&);
bool confirm_additions(const Plan&, const Census& after, std::string&);

struct ExternalLink {
    int32_t saved_link_id = 0;
    std::string portable_key;
    uintptr_t object = 0;
};
using Lookup = void*(*)(void*, int32_t);
using FreshLink = bool(*)(void*, const ExternalLink&, std::string&);
struct LookupReceipt {
    uint64_t original_calls = 0, external_hits = 0;
    bool failed = false;
    std::string cause;
};
// Only a dedicated new-object context may be armed. Existing world objects must
// not enter its AddObject list: Resolve would revisit them and alter ownership.
class LookupScope {
public:
    bool arm(uint32_t thread, void* context, const std::vector<int32_t>& local_ids,
             std::vector<ExternalLink>, FreshLink, void* reader, std::string&);
    void clear();
    void* lookup(uint32_t thread, void* context, int32_t id, Lookup original);
    LookupReceipt receipt() const { std::lock_guard lock(mutex_); return receipt_; }
private:
    mutable std::recursive_mutex mutex_;
    bool busy_ = false;
    uint32_t thread_ = 0;
    void* context_ = nullptr;
    void* reader_ = nullptr;
    FreshLink fresh_ = nullptr;
    std::vector<int32_t> local_ids_;
    std::vector<ExternalLink> links_;
    LookupReceipt receipt_;
};
}
