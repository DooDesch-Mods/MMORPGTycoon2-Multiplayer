#include "cohort.h"
#include <algorithm>
#include <set>
#include <utility>

namespace mt2mp::subscriber_lifecycle {
namespace {
bool fail(std::string& e, const char* cause) { e = cause; return false; }
bool valid(const Census& c, std::string& e) {
    if (!c.epoch || !c.origin_generation || !c.world || !c.manager || c.next_id < 0)
        return fail(e, "subscriber census world/epoch/manager/nextID invalid");
    std::set<int32_t> ids, on, off;
    std::set<uintptr_t> objects;
    for (const auto& m : c.members) {
        if (m.id < 0 || m.id >= c.next_id || !m.subscriber || (m.online && !m.toon) ||
            !ids.insert(m.id).second || !objects.insert(m.subscriber).second)
            return fail(e, "subscriber census ID/owned object graph invalid");
    }
    for (auto id : c.online) if (!ids.contains(id) || !on.insert(id).second)
        return fail(e, "subscriber online membership missing/duplicate");
    for (auto id : c.offline) if (!ids.contains(id) || !off.insert(id).second || on.contains(id))
        return fail(e, "subscriber offline membership missing/duplicate/overlap");
    for (const auto& m : c.members) if (m.online ? !on.contains(m.id) : !off.contains(m.id))
        return fail(e, "subscriber login readback differs from manager membership");
    return true;
}
}
bool plan_additions(const Census& before, const std::vector<Addition>& additions,
                    int32_t next, Plan& out, std::string& e) {
    out = {};
    if (!valid(before, e)) return false;
    if (next < before.next_id) return fail(e, "host subscriber nextID moved backwards");
    std::set<int32_t> ids;
    for (const auto& m : before.members) ids.insert(m.id);
    for (const auto& m : additions) {
        if (m.id < 0 || m.id >= next || !ids.insert(m.id).second)
            return fail(e, "new subscriber serialized ID reused/duplicate/outside host nextID");
    }
    out = {before, additions, next}; e.clear(); return true;
}
bool confirm_additions(const Plan& p, const Census& after, std::string& e) {
    if (!valid(after, e)) return false;
    const auto& b = p.before;
    if (after.epoch != b.epoch || after.origin_generation != b.origin_generation ||
        after.world != b.world || after.manager != b.manager || after.next_id != p.next_id)
        return fail(e, "subscriber world/manager/nextID changed outside admitted cohort");
    if (after.members.size() != b.members.size() + p.additions.size())
        return fail(e, "subscriber native append count differs");
    for (size_t i = 0; i < b.members.size(); ++i)
        if (after.members[i] != b.members[i]) return fail(e, "existing subscriber identity/order/state changed");
    for (size_t i = 0; i < p.additions.size(); ++i) {
        const auto& m = after.members[b.members.size() + i];
        if (m.id != p.additions[i].id || m.online != p.additions[i].online)
            return fail(e, "new subscriber ID/login differs from host cohort");
        if (!m.brain) return fail(e, "new subscriber resolved Brain is missing");
    }
    e.clear(); return true;
}
bool LookupScope::arm(uint32_t thread, void* context, const std::vector<int32_t>& local,
                       std::vector<ExternalLink> links, FreshLink fresh, void* reader, std::string& e) {
    std::lock_guard lock(mutex_);
    if (busy_) return fail(e, "subscriber context lookup reentrant arm");
    if (context_) return fail(e, "subscriber context lookup already armed");
    if (!thread || !context || !fresh) return fail(e, "subscriber context lookup binding incomplete");
    std::set<int32_t> ids;
    for (auto id : local) if (id <= 0 || !ids.insert(id).second)
        return fail(e, "subscriber record local link IDs invalid/duplicate");
    std::set<std::string> keys;
    struct ArmBusy { bool& value; explicit ArmBusy(bool& v) : value(v) { value = true; } ~ArmBusy() { value = false; } } arm_busy(busy_);
    for (const auto& link : links) {
        if (link.saved_link_id <= 0 || !link.object || link.portable_key.empty() ||
            !ids.insert(link.saved_link_id).second || !keys.insert(link.portable_key).second)
            return fail(e, "subscriber external link missing/duplicate/local collision");
        if (!fresh(reader, link, e)) return false;
    }
    thread_ = thread; context_ = context; reader_ = reader; fresh_ = fresh;
    local_ids_ = local; links_ = std::move(links); receipt_ = {}; e.clear(); return true;
}
void LookupScope::clear() {
    std::lock_guard lock(mutex_);
    if (busy_) { receipt_.failed = true; receipt_.cause = "subscriber context lookup reentrant clear"; return; }
    context_ = nullptr; thread_ = 0; reader_ = nullptr; fresh_ = nullptr;
    local_ids_.clear(); links_.clear();
}
void* LookupScope::lookup(uint32_t thread, void* context, int32_t id, Lookup original) {
    std::lock_guard lock(mutex_);
    if (!original) { receipt_.failed = true; receipt_.cause = "subscriber original context lookup missing"; return nullptr; }
    ++receipt_.original_calls;
    void* value = original(context, id);
    if (!context_ || context != context_) return value;
    if (thread != thread_) {
        receipt_.failed = true; receipt_.cause = "subscriber private context lookup on wrong thread"; return value;
    }
    if (busy_) { receipt_.failed = true; receipt_.cause = "subscriber context lookup reentered"; return value; }
    if (receipt_.failed) return value;
    auto link = std::find_if(links_.begin(), links_.end(), [id](const auto& item) { return item.saved_link_id == id; });
    if (link == links_.end()) {
        if (id != 0 && id != -1 && !value) {
            receipt_.failed = true;
            receipt_.cause = std::find(local_ids_.begin(), local_ids_.end(), id) == local_ids_.end()
                ? "subscriber record requested undeclared external link" : "subscriber declared local link was not constructed";
        }
        return value;
    }
    if (value) {
        receipt_.failed = true; receipt_.cause = "subscriber external link collides with private native context"; return value;
    }
    struct Busy { bool& value; explicit Busy(bool& v) : value(v) { value = true; } ~Busy() { value = false; } } busy(busy_);
    if (!fresh_(reader_, *link, receipt_.cause)) { receipt_.failed = true; return nullptr; }
    if (receipt_.failed) return nullptr;
    ++receipt_.external_hits;
    return reinterpret_cast<void*>(link->object);
}
}
