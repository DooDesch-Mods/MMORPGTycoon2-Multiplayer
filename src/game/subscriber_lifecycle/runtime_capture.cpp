#include "mt2mp/core/runtime_paths.h"
#include "runtime_capture.h"
#include "building_reference_pins.h"
#include "npc_reference_pins.h"
#include "capture.h"
#include "record_graph.h"
#include "reference_map.h"
#include "save_scope.h"
#include "native_open_runtime.h"
#include "session_provider.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/core/config.h"
#include "mt2mp/core/hash.h"
#include <atomic>
#include <set>
#include <fstream>
#include <functional>
#include <Windows.h>

namespace mt2mp::game::subscriber_lifecycle_capture {
namespace {
namespace local = mt2mp::subscriber_lifecycle;
using Json = nlohmann::json;
using World = building_mixed_identity::opened_file::CurrentWorld;
constexpr const char* exe_sha = "bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
std::atomic_flag busy = ATOMIC_FLAG_INIT;
std::atomic<uint64_t> sequence{0};
uint64_t blocked_epoch = 0, blocked_generation = 0;
std::string blocked_cause;
bool fail(std::string& e, const std::string& cause) { e = cause; return false; }
class ProcessMemory final : public local::Memory {
public:
    bool read(uintptr_t p, void* out, size_t size, std::string& e) override {
        if (!p || p > UINTPTR_MAX - size) return fail(e, "subscriber capture memory extent invalid");
        SIZE_T copied = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(p), out, size, &copied))
            return fail(e, "subscriber capture ReadProcessMemory error=" + std::to_string(GetLastError()));
        if (copied != size) return fail(e, "subscriber capture ReadProcessMemory short read");
        return true;
    }
};
ProcessMemory memory;
template<class T> bool at(uintptr_t p, size_t offset, T& out, std::string& e) {
    if (!p || p > UINTPTR_MAX - offset) return fail(e, "subscriber capture field address invalid");
    return memory.read(p + offset, &out, sizeof out, e);
}
bool ap(uintptr_t object, uintptr_t image, uintptr_t expected, const char* label, std::string& e) {
    uintptr_t actual = 0;
    if (!at(object, 0, actual, e)) return false;
    return actual == image + expected || fail(e, std::string("subscriber capture ") + label + " addresspoint mismatch");
}
bool uuid(const std::string& s) {
    if (s.size() != 36) return false;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) { if (s[i] != '-') return false; }
        else if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f'))) return false;
    }
    return true;
}
struct Member {
    uintptr_t object = 0;
    int32_t id = -1;
    bool operator==(const Member&) const = default;
};
struct Pin {
    World world;
    uintptr_t image = 0, manager = 0, data = 0, subscriber = 0, toon = 0, brain = 0;
    uint32_t thread = 0, image_size = 0;
    int32_t capacity = 0, next_id = 0;
    bool online = false;
    std::vector<Member> members;
};
bool same(const Pin& a, const Pin& b) {
    const auto& x = a.world; const auto& y = b.world;
    return x.game == y.game && x.mode == y.mode && x.state == y.state && x.world == y.world && x.map == y.map &&
        x.generation == y.generation && x.world_epoch == y.world_epoch && x.uuid == y.uuid && x.sha256 == y.sha256 &&
        a.image == b.image && a.thread == b.thread && a.manager == b.manager && a.data == b.data && a.capacity == b.capacity &&
        a.next_id == b.next_id && a.members == b.members && a.subscriber == b.subscriber && a.toon == b.toon && a.brain == b.brain && a.online == b.online;
}
bool read_pin(int32_t id, const std::string& origin, Pin& out, std::string& e, int32_t expected_saves = 0, bool census = true, bool require_paused = true, const action_session::Context* expected = nullptr) {
    out = {}; std::string hash;
    if(expected){
        action_session::Context current;
        if(!action_session::selected_session().current(current,e))return false;
        if(current!=*expected||current.role!=action_session::Role::Host||current.host_peer!=1||current.local_peer!=1||
           !current.session||current.snapshot_uuid.empty()||(current.peer_mask&0xf0)||!(current.peer_mask&1u))
            return fail(e,"subscriber capture production Session/Host slot differs");
    }else{
        const auto& config = Config::instance();
        if (!config.local_test || !config.is_host || config.peer_id != 1) return fail(e, "subscriber capture requires actual owned test Host1");
    }
    if (!native::read_resolver_receipt(out.image, out.thread, hash, e)) return false;
    if (hash != exe_sha || out.thread != GetCurrentThreadId()) return fail(e, "subscriber capture exact build/mainthread differs");
    if (!native_open_runtime::read_current_world(out.world, e)) return false;
    if (out.world.uuid != origin || !out.world.generation || !out.world.world_epoch) return fail(e, "subscriber capture native origin UUID/epoch differs");
    native::WorldStatus status;
    if (!native::read_world_status(status, e)) return false;
    if (status.active_mode_index != 3 || status.in_game_state != 4 || !status.world_present || status.quitting ||
        status.title_transition_pending || status.map_task_pending || status.edit_operations != 0 || status.saves_in_progress != expected_saves)
        return fail(e, "subscriber capture active World/effects guard failed: mode=" + std::to_string(status.active_mode_index) +
            " state=" + std::to_string(status.in_game_state) + " edits=" + std::to_string(status.edit_operations) + " saves=" + std::to_string(status.saves_in_progress));
    uint32_t pe_offset = 0;
    if (!at(out.image, 0x3c, pe_offset, e) || !at(out.image, size_t(pe_offset) + 24 + 56, out.image_size, e) || out.image_size < 0x1744e90)
        return fail(e, e.empty() ? "subscriber capture image extent invalid" : e);
    const auto state = reinterpret_cast<uintptr_t>(out.world.state);
    uintptr_t singleton = 0;
    if (!ap(state, out.image, 0x16de920, "GameState", e) || !at(state, 0x1b8, out.manager, e) ||
        !at(out.image, 0x13df690, singleton, e) || singleton != out.manager || !ap(out.manager, out.image, 0x1725f40, "Manager", e))
        return fail(e, e.empty() ? "subscriber capture GameState/Singleton Manager ownership differs" : e);
    native::ClockState clock;
    uintptr_t clock_object = 0;
    if (!at(out.image, 0x13df9a0, clock_object, e) || !ap(clock_object, out.image, 0x17499d0, "Clock", e)) return false;
    if (!native::read_clock(clock, e)) return false;
    if (require_paused && (clock.time_scale != 0 || !clock.paused)) return fail(e, "subscriber capture requires selected native Clock0 before original SaveScope");
    if (!census) return true;
    int32_t count = -1;
    if (!at(out.manager, 0x40, out.data, e) || !at(out.manager, 0x48, count, e) || !at(out.manager, 0x4c, out.capacity, e) ||
        !at(out.manager, 0x68, out.next_id, e)) return false;
    if (count < 0 || out.capacity < count || out.next_id < 0 || (count && !out.data)) return fail(e, "subscriber capture actual Manager array bounds invalid");
    std::set<int32_t> ids; std::set<uintptr_t> objects;
    for (int32_t i = 0; i < count; ++i) {
        Member m;
        if (!at(out.data, size_t(i) * 8, m.object, e) || !ap(m.object, out.image, 0x16e75f0, "Subscriber", e) || !at(m.object, 0x20, m.id, e)) return false;
        if (m.id < 0 || m.id >= out.next_id || !ids.insert(m.id).second || !objects.insert(m.object).second)
            return fail(e, "subscriber capture actual serialized IDs/objects not unique");
        out.members.push_back(m);
        if (m.id == id) out.subscriber = m.object;
    }
    if (!out.subscriber) return fail(e, "subscriber capture requested serialized ID is absent");
    uintptr_t owner = 0, reverse = 0; uint8_t online = 0;
    if (!at(out.subscriber, 0x80, out.toon, e) || !at(out.subscriber, 0x88, out.brain, e) ||
        !ap(out.toon, out.image, 0x1744e90, "Toon", e) || !ap(out.brain, out.image, 0x16fdd60, "Brain", e) ||
        !at(out.brain, 0x28, owner, e) || !at(out.toon, 0x940, reverse, e) || !at(out.toon, 0x825, online, e)) return false;
    if (owner != out.subscriber || reverse != out.subscriber || online > 1) return fail(e, "subscriber capture actual Toon/Brain reciprocal ownership differs");
    out.online = online != 0; return true;
}
bool pin(int32_t id, const std::string& origin, Pin& out, std::string& e, int32_t expected_saves = 0, bool require_paused = true, const action_session::Context* expected = nullptr) {
    Pin first, second;
    if (!read_pin(id, origin, first, e, expected_saves, true, require_paused, expected) || !read_pin(id, origin, second, e, expected_saves, true, require_paused, expected)) return false;
    if (!same(first, second)) return fail(e, "subscriber capture native double-read census changed");
    out = std::move(second); return true;
}
bool bytes(uintptr_t image, uintptr_t rva, const char* expected, size_t count, std::string& e) {
    std::vector<char> actual(count);
    if (!memory.read(image + rva, actual.data(), actual.size(), e)) return false;
    return std::equal(actual.begin(), actual.end(), expected) || fail(e, "subscriber capture native entry bytes differ RVA=" + std::to_string(rva));
}
bool resolve(uintptr_t image, local::CaptureApi& api, std::string& e) {
    if (!bytes(image, 0x9667c0, "\x57\x56\x53\x48\x83\xec\x20\x48\x89\xcb\xe8\xb1\x66\x00\x00\x48", 16, e) ||
        !bytes(image, 0x9676a0, "\x57\x56\x53\x48\x83\xec\x20\x48\x89\xce\xe8\x61\xed\xff\xff\x48", 16, e) ||
        !bytes(image, 0x89e580, "\x53\x48\x83\xec\x20\x8b\x05\x25\xe9\x19\x01\x8d\x50\x01\x89\x15", 16, e) ||
        !bytes(image, 0x89e5f0, "\x56\x53\x48\x83\xec\x28\x48\x8b\x31\x48\x85\xf6\x0f\x84\x7e\x00", 16, e) ||
        !bytes(image, 0xeb85f0, "\x53\x48\x83\xec\x30\x48\x8b\x01\x48\x8b\x40\x30\x49\x89\xd0\x48", 16, e) ||
        !bytes(image, 0xeb8650, "\x41\x55\x41\x54\x55\x57\x56\x53\x48\x83\xec\x58\x48\x8d\x2d\x5d", 16, e)) return false;
    api = {reinterpret_cast<local::Construct>(image + 0x9667c0), reinterpret_cast<local::Construct>(image + 0x9676a0),
        reinterpret_cast<local::Construct>(image + 0x89e580), reinterpret_cast<local::Construct>(image + 0x89e5f0),
        reinterpret_cast<local::PrepareSave>(image + 0xeb85f0), reinterpret_cast<local::SaveRecord>(image + 0xeb8650)};
    return true;
}
struct Schedulers { uintptr_t task = 0, tick = 0, update = 0; bool operator==(const Schedulers&) const = default; };
bool schedulers(const Pin& pin, bool held, Schedulers& out, std::string& e) {
    const auto image = pin.image;
    if (!at(image, 0x13df340, out.task, e) || !ap(out.task, image, 0x16fdaf0, "TaskManager", e) ||
        !at(image, 0x13df350, out.tick, e) || !ap(out.tick, image, 0x16fdc40, "TickManager", e) ||
        !at(image, 0x13df4a0, out.update, e) || !ap(out.update, image, 0x1709650, "UpdateManager", e)) return false;
    uintptr_t task = 0, tick = 0, update = 0;
    const auto game = reinterpret_cast<uintptr_t>(pin.world.game);
    if (!at(game, 0x68, task, e) || !at(game, 0x70, update, e) || !at(game, 0x150, tick, e)) return false;
    if (task != out.task || tick != out.tick || update != out.update) return fail(e, "subscriber capture Game scheduler ownership differs");
    uint8_t task_paused = 0, tick_paused = 0, update_paused = 0;
    if (!at(task, 0xf8, task_paused, e) || !at(tick, 0x28, tick_paused, e) || !at(update, 0x68, update_paused, e)) return false;
    const auto expected = static_cast<uint8_t>(held);
    if (task_paused != expected || tick_paused != expected || update_paused != expected)
        return fail(e, "subscriber capture native scheduler pause state differs: task=" + std::to_string(task_paused) +
            " tick=" + std::to_string(tick_paused) + " update=" + std::to_string(update_paused) + " required=" + std::to_string(expected));
    if (held) {
        int32_t running = -1, waiting = -1;
        if (!at(task, 0xa8, running, e) || !at(task, 0x90, waiting, e)) return false;
        if (running || waiting) return fail(e, "subscriber capture original TaskManager.Pause did not drain: a8=" + std::to_string(running) + " 90=" + std::to_string(waiting));
    }
    return true;
}
bool same_owner(const Pin& a, const Pin& b) {
    const auto& x = a.world; const auto& y = b.world;
    return x.game == y.game && x.mode == y.mode && x.state == y.state && x.world == y.world && x.map == y.map &&
        x.generation == y.generation && x.world_epoch == y.world_epoch && x.uuid == y.uuid && x.sha256 == y.sha256 &&
        a.image == b.image && a.thread == b.thread && a.manager == b.manager;
}
struct ScopeCallbacks { std::function<bool(std::string&)> body, exit; };
bool scope_body(void* p, std::string& e) { return static_cast<ScopeCallbacks*>(p)->body(e); }
bool scope_exit(void* p, std::string& e) { return static_cast<ScopeCallbacks*>(p)->exit(e); }
struct Borrow { local::Graph graph; };
bool inspect(void* user, const void* record, const void* context, std::string& e) {
    // These are per-capture memory budgets. Rejection is explicit and never a
    // restriction on the world's native Subscriber population.
    const local::Budget budget{100000, 1000000, 16 * 1024 * 1024, 1000000};
    return local::read_graph(memory, reinterpret_cast<uintptr_t>(record), reinterpret_cast<uintptr_t>(context), budget, static_cast<Borrow*>(user)->graph, e);
}
Json token(const local::Token& value) {
    Json out{{"type", value.type}};
    if (value.type == 1 || value.type == 2) out["text"] = value.text;
    else if (value.type == 3 || value.type == 4) out["bits"] = value.bits;
    return out;
}
std::string digest(const std::string& value) { return mt2mp::sha256(std::vector<unsigned char>(value.begin(), value.end())); }
bool save_artifact(const std::filesystem::path& path, const std::string& content, std::string& e) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return fail(e, "subscriber capture artifact create error=" + std::to_string(GetLastError()));
    DWORD written = 0; const bool wrote = content.size() <= MAXDWORD && WriteFile(file, content.data(), static_cast<DWORD>(content.size()), &written, nullptr);
    const DWORD write_error = wrote ? 0 : GetLastError(); const bool flushed = wrote && written == content.size() && FlushFileBuffers(file);
    const DWORD flush_error = flushed ? 0 : GetLastError(); const bool closed = CloseHandle(file) != FALSE; const DWORD close_error = closed ? 0 : GetLastError();
    if (!wrote || written != content.size()) return fail(e, "subscriber capture artifact write error=" + std::to_string(write_error) + " written=" + std::to_string(written));
    if (!flushed) return fail(e, "subscriber capture artifact flush error=" + std::to_string(flush_error));
    if (!closed) return fail(e, "subscriber capture artifact close error=" + std::to_string(close_error));
    std::ifstream verify(path, std::ios::binary); const std::string actual((std::istreambuf_iterator<char>(verify)), {});
    if (verify.bad() || actual != content) return fail(e, "subscriber capture artifact readback differs");
    return true;
}
}
bool capture_impl(const std::filesystem::path& root, int32_t id, const std::string& origin, Json& result, std::string& cause, Json* wire, const action_session::Context* expected, local::first_append::PartyReferences* parties=nullptr) {
    result = {{"state", "rejected"}, {"serialized_subscriber_id", id}, {"native_origin_uuid", origin}, {"native_save_context_serial_changes", true}, {"subscriber_creation_requested", false}};
    if (busy.test_and_set()) return fail(cause, "subscriber capture reentrant operation");
    struct Release { ~Release() { busy.clear(); } } release;
    try {
        if (id < 0 || !uuid(origin) || (!wire && root.empty())) return fail(cause, "subscriber capture typed ID/origin/runtime root invalid");
        Pin admission;
        if (!read_pin(id, origin, admission, cause, 0, false, !wire, expected)) return false;
        if (blocked_epoch == admission.world.world_epoch && blocked_generation == admission.world.generation)
            return fail(cause, "subscriber capture previous native outcome unknown: " + blocked_cause);
        subscriber_building_references::Pins building_pins;
        subscriber_npc_references::Pins npc_pins;
        if(!building_pins.prepare(admission.image, admission.world, cause)||!npc_pins.prepare(admission.image,admission.world,cause))return false;
        local::CaptureApi api;
        if (!resolve(admission.image, api, cause) ||
            !bytes(admission.image, 0x4a03a0, "\x53\x48\x83\xec\x20\x48\x89\xcb\xe8\xd3\xb8\x56\x00\x84\xc0\x74", 16, cause) ||
            !bytes(admission.image, 0x49f6f0, "\x57\x56\x53\x48\x83\xec\x40\x48\x89\xcb\xe8\x81\xc5\x56\x00\x84", 16, cause)) return false;
        Schedulers original_schedulers;
        if (!schedulers(admission, false, original_schedulers, cause)) return false;
        const local::SaveScopeApi scope_api{reinterpret_cast<local::SaveScopeCall>(admission.image + 0x4a03a0),
            reinterpret_cast<local::SaveScopeCall>(admission.image + 0x49f6f0)};
        Pin before;
        local::CaptureReceipt receipt; Borrow borrow;
        Json rows = Json::array(), links = Json::array(), external_links = Json::array(); size_t external = 0;
        bool workers_drained = false;
        ScopeCallbacks callbacks;
        callbacks.body = [&](std::string& e) {
            Schedulers held;
            if (!pin(id, origin, before, e, 1, !wire, expected) || !same_owner(admission, before)) return fail(e, e.empty() ? "subscriber capture native owner changed entering SaveScope" : e);
            if (!schedulers(before, true, held, e) || held != original_schedulers) return fail(e, e.empty() ? "subscriber capture scheduler receivers changed" : e);
            workers_drained = true;
            if (!local::capture_original(api, reinterpret_cast<const void*>(before.subscriber), inspect, &borrow, receipt, e)) return false;
            Pin after;
            if (!pin(id, origin, after, e, 1, !wire, expected) || !same(before, after)) return fail(e, e.empty() ? "subscriber capture owner changed across native capture" : e);
            for (const auto& row : borrow.graph.records) {
                Json tokens = Json::array(); for (const auto& value : row.tokens) tokens.push_back(token(value));
                rows.push_back({{"parent", row.parent == SIZE_MAX ? Json(nullptr) : Json(row.parent)}, {"child_index", row.child_index}, {"label", token(row.label)}, {"tokens", std::move(tokens)}});
            }
            int32_t character_kind = -1, character_id = -1;
            if (!local::first_append::record_character_type(borrow.graph.records, character_kind, character_id, e)) return false;
            const local::first_append::ReferenceWorld reference_world{before.image,
                reinterpret_cast<uintptr_t>(before.world.state), reinterpret_cast<uintptr_t>(before.world.map), &building_pins, &npc_pins, parties};
            for (const auto& link : borrow.graph.private_links) {
                uintptr_t vptr = 0;
                if (!at(link.object, 0, vptr, e)) return false;
                if (vptr < before.image || vptr >= before.image + before.image_size) return fail(e, "subscriber capture referenced object addresspoint outside exact game image");
                Json row{{"saved_link_id", link.saved_link_id}, {"inline_definition", link.has_inline_definition}, {"class_addresspoint_rva", vptr - before.image}};
                if (!link.has_inline_definition) {
                    ++external;
                    local::first_append::ReferenceKey key;
                    uintptr_t resolved = 0;
                    const auto reference_failure = [&](const char* stage) {
                        return fail(e, std::string("subscriber external reference ")+stage+" subscriber_id="+std::to_string(id)+
                            " saved_link_id="+std::to_string(link.saved_link_id)+" class_addresspoint_rva="+std::to_string(vptr-before.image)+
                            " character_kind="+std::to_string(character_kind)+" character_id="+std::to_string(character_id)+" original="+e);
                    };
                    if (!local::first_append::identify_reference(memory, reference_world, link.object, character_kind, character_id, key, e))
                        return reference_failure("identify failed");
                    if (!local::first_append::resolve_reference(memory, reference_world, key, resolved, e))
                        return reference_failure("resolve failed");
                    if (resolved != link.object) {
                        e="subscriber external reference owner roundtrip differs";return reference_failure("roundtrip failed");
                    }
                    const char* kind = key.kind == local::first_append::ReferenceKind::RootShard ? "root_shard" :
                        key.kind == local::first_append::ReferenceKind::RootRegion ? "root_region" :
                        key.kind == local::first_append::ReferenceKind::SharedBuilding ? "shared_building" :
                        key.kind == local::first_append::ReferenceKind::SubscriberToon ? "subscriber_toon" :
                        key.kind == local::first_append::ReferenceKind::SharedNPC ? "shared_npc" :
                        key.kind == local::first_append::ReferenceKind::SharedQuest ? "shared_quest" :
                        key.kind == local::first_append::ReferenceKind::SharedParty ? "shared_party" : "character_ability";
                    row["portable_owner"] = {{"kind", kind}, {"region_id", key.region_id}, {"character_kind", key.character_kind},
                        {"character_id", key.character_id}, {"ability_index", key.ability_index}, {"source_snapshot_sha256", before.world.sha256}};
                    if(key.kind==local::first_append::ReferenceKind::SharedBuilding)row["portable_owner"]["building_key"]=key.building_key;
                    if(key.kind==local::first_append::ReferenceKind::SharedNPC)row["portable_owner"]["npc_key"]=key.npc_key;
                    if(key.kind==local::first_append::ReferenceKind::SharedQuest){
                        row["portable_owner"]["npc_key"]=key.npc_key;
                        row["portable_owner"]["sequence_id"]=key.sequence_id;
                    }
                    if(key.kind==local::first_append::ReferenceKind::SharedParty)row["portable_owner"]["party_key"]=key.party_key;
                    if(key.kind==local::first_append::ReferenceKind::SubscriberToon){
                        if(key.subscriber_id==id){e="Subscriber self Toon must have an inline Record definition";return reference_failure("identify failed");}
                        row["portable_owner"]["subscriber_id"]=key.subscriber_id;
                    }
                }
                if (link.object == reinterpret_cast<uintptr_t>(before.world.world)) row["anchor"] = "current_world";
                else if (link.object == reinterpret_cast<uintptr_t>(before.world.map)) row["anchor"] = "current_map";
                else if (link.object == reinterpret_cast<uintptr_t>(before.world.state)) row["anchor"] = "current_game_state";
                for (const auto& member : before.members) if (link.object == member.object) row["serialized_subscriber_id"] = member.id;
                if (!link.has_inline_definition) external_links.push_back(row);
                links.push_back(std::move(row));
            }
            return true;
        };
        callbacks.exit = [&](std::string& e) {
            Pin current; Schedulers held;
            if (!read_pin(id, origin, current, e, 1, false, !wire, expected) || !same_owner(admission, current)) return fail(e, e.empty() ? "subscriber SaveScope exit owner changed" : e);
            if (!schedulers(current, true, held, e) || held != original_schedulers) return fail(e, e.empty() ? "subscriber SaveScope exit scheduler changed" : e);
            return building_pins.verify_used(e)&&npc_pins.verify_used(e);
        };
        local::SaveScopeReceipt scope_receipt;
        const bool captured = local::with_save_scope(scope_api, admission.world.mode, scope_body, scope_exit, &callbacks, scope_receipt, cause);
        if (scope_receipt.unknown || receipt.call_exception || receipt.record_cleanup_exception || receipt.context_cleanup_exception) {
            blocked_epoch = admission.world.world_epoch; blocked_generation = admission.world.generation; blocked_cause = cause;
            result["state"] = "unknown";
        }
        result["save_scope"] = {{"enter_attempted", scope_receipt.enter_attempted}, {"enter_returned", scope_receipt.enter_returned},
            {"body_attempted", scope_receipt.body_attempted}, {"exit_guard_confirmed", scope_receipt.exit_guard_confirmed},
            {"leave_attempted", scope_receipt.leave_attempted}, {"leave_returned", scope_receipt.leave_returned},
            {"enter_exception", scope_receipt.enter_exception}, {"leave_exception", scope_receipt.leave_exception},
            {"unknown", scope_receipt.unknown}, {"workers_drained_confirmed", workers_drained}, {"closed_readback_confirmed", false},
            {"body_cause", scope_receipt.body_cause}, {"cleanup_cause", scope_receipt.cleanup_cause},
            {"original_side_effect_paths", {"shard_update_dt0", "task_pause_and_drain", "cursor_tick_update_pause", "title_logo_autounshow", "autosave_notify_if_present", "save_cache_rebuild"}}};
        result["capture"] = {{"record_constructed", receipt.record_constructed}, {"context_constructed", receipt.context_constructed},
            {"prepare_returned", receipt.prepare_returned}, {"save_returned", receipt.save_returned}, {"inspected", receipt.inspected},
            {"record_destroyed", receipt.record_destroyed}, {"context_destroyed", receipt.context_destroyed}, {"exception", receipt.call_exception},
            {"record_cleanup_exception", receipt.record_cleanup_exception}, {"context_cleanup_exception", receipt.context_cleanup_exception}};
        if (!captured) {
            if (scope_receipt.unknown || receipt.call_exception || receipt.record_cleanup_exception || receipt.context_cleanup_exception) {
                blocked_epoch = admission.world.world_epoch; blocked_generation = admission.world.generation; blocked_cause = cause; result["state"] = "unknown";
            }
            return false;
        }
        Pin closed; Schedulers resumed;
        if (!read_pin(id, origin, closed, cause, 0, false, !wire, expected) || !same_owner(admission, closed) ||
            !schedulers(closed, false, resumed, cause) || resumed != original_schedulers || !building_pins.verify_used(cause)||!npc_pins.verify_used(cause)) {
            if (cause.empty()) cause = "subscriber SaveScope closed readback differs";
            blocked_epoch = admission.world.world_epoch; blocked_generation = admission.world.generation; blocked_cause = cause; result["state"] = "unknown";
            return false;
        }
        result["save_scope"]["closed_readback_confirmed"] = true;
        const auto source = rows.dump(); const auto references = links.dump();
        const auto source_hash = digest(source), link_hash = digest(references), external_hash = digest(external_links.dump());
        Json artifact{{"schema", 1}, {"native_origin_uuid", origin}, {"origin_sha256", before.world.sha256}, {"origin_generation", before.world.generation},
            {"world_epoch", before.world.world_epoch}, {"serialized_subscriber_id", id}, {"next_subscriber_id", before.next_id}, {"online", before.online},
            {"source_record_sha256", source_hash}, {"references_sha256", link_hash}, {"external_references_sha256", external_hash}, {"records", std::move(rows)}, {"references", std::move(links)}};
        if(wire){*wire=artifact;result["state"]="captured";cause.clear();return true;}
        const auto directory = runtime_paths::diagnostics(root) / "subscriber-records";
        std::filesystem::create_directories(directory);
        const auto path = directory / (origin + "-" + std::to_string(id) + "-" + std::to_string(GetTickCount64()) + "-" + std::to_string(++sequence) + ".json");
        if (!save_artifact(path, artifact.dump(2), cause)) return false;
        result.update({{"state", "captured"}, {"origin_generation", before.world.generation}, {"world_epoch", before.world.world_epoch},
            {"subscriber_count", before.members.size()}, {"next_subscriber_id", before.next_id}, {"online", before.online},
            {"record_count", borrow.graph.records.size()}, {"reference_count", borrow.graph.private_links.size()}, {"external_reference_count", external},
            {"source_record_sha256", source_hash}, {"references_sha256", link_hash}, {"external_references_sha256", external_hash}, {"private_artifact", path.string()}, {"client_append_proven", false}});
        cause.clear(); return true;
    } catch (const std::exception& e) { return fail(cause, std::string("subscriber capture exception: ") + e.what()); }
    catch (...) { return fail(cause, "subscriber capture unknown C++ exception"); }
}
bool capture(const std::filesystem::path& root,int32_t id,const std::string& origin,Json& result,std::string& cause){return capture_impl(root,id,origin,result,cause,nullptr,nullptr);}
bool capture_frame(int32_t id,const std::string& origin,const action_session::Context& expected,Json& graph,Json& receipt,std::string& cause){graph={};return capture_impl({},id,origin,receipt,cause,&graph,&expected);}
bool capture_frame(int32_t id,const std::string& origin,const action_session::Context& expected,
                   local::first_append::PartyReferences& parties,Json& graph,Json& receipt,std::string& cause){
    graph={};return capture_impl({},id,origin,receipt,cause,&graph,&expected,&parties);
}
#ifdef MT2MP_SUBSCRIBER_CAPTURE_TEST
bool testing_pin(int32_t id, const std::string& origin, std::string& cause) {
    Pin value;
    return pin(id, origin, value, cause);
}
bool testing_schedulers(int32_t id, const std::string& origin, bool held, std::string& cause) {
    Pin value; Schedulers actual;
    return read_pin(id, origin, value, cause, held ? 1 : 0, false) && schedulers(value, held, actual, cause);
}
#endif
}
