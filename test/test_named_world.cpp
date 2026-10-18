#include <Windows.h>
#include "mt2mp/game/native_engine.h"
#include "mt2mp/net/snapshot_world_path.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <type_traits>

#define LOG_INFO(...) ((void)0)
#define LOG_ERROR(...) ((void)0)

namespace mt2mp::game::native {
namespace {
std::array<unsigned char, 0x600> fake_game{}, fake_title{}, fake_mode{}, fake_software{}, other_world{}, other_software{};
constexpr uintptr_t game_slot = 1, mode_slot = 2, software_slot = 3;
constexpr uintptr_t title_command = 0x4b9be0;
std::atomic<uintptr_t> image{0};
std::atomic<DWORD> game_thread{0};
bool world_active = false, folder_readable = true, tick_slot_valid = true, memory_writable = true;
bool native_world_readable = true;
int saves = 0;
std::string current_folder;
std::string current_stem;
void* current_world = nullptr;
void* current_software = nullptr;
std::map<const void*, uintptr_t> vtables;
unsigned cache_stores = 0;
unsigned native_load_calls = 0;
unsigned passed = 0;

bool fail(std::string& error, const std::string& cause) { error = cause; return false; }
bool readable(const void* pointer, size_t size) {
    const auto address = reinterpret_cast<uintptr_t>(pointer);
    for (const auto* bytes : {&fake_game, &fake_title, &fake_mode, &fake_software, &other_world, &other_software}) {
        const auto start = reinterpret_cast<uintptr_t>(bytes->data());
        if (address >= start && address - start <= bytes->size() && size <= bytes->size() - (address - start)) return true;
    }
    return false;
}
template<class T> bool read_at(const void* object, size_t offset, T& result) {
    if constexpr (std::is_same_v<T, uintptr_t>) {
        if (object == reinterpret_cast<void*>(image.load()) && (offset == 0x17154f8 || offset == 0x1715520)) {
            result = image.load() + (offset == 0x17154f8 ? 0x2de7f0 : 0x2de820) + (tick_slot_valid ? 0 : 1);
            return true;
        }
    }
    const auto* source = static_cast<const unsigned char*>(object) + offset;
    if (!readable(source, sizeof(result))) return false;
    std::memcpy(&result, source, sizeof(result));
    return true;
}
template<class T> void write_at(void* object, size_t offset, const T& value) {
    auto* target = static_cast<unsigned char*>(object) + offset;
    if (!readable(target, sizeof(value))) throw std::runtime_error("fixture write escapes owned bytes");
    std::memcpy(target, &value, sizeof(value));
}
void* singleton(uintptr_t slot) {
    if (slot == game_slot) return fake_game.data();
    if (slot == mode_slot) return fake_mode.data();
    if (slot == software_slot) return current_software;
    return nullptr;
}
bool has_vtable(const void* object, uintptr_t expected) {
    const auto found = vtables.find(object);
    return found != vtables.end() && found->second == expected;
}
bool read_game_string(const void* object, size_t offset, std::string& result) {
    if (object != fake_game.data() || offset != 0x470 || !folder_readable) return false;
    result = current_folder; return true;
}
bool game_string_equals(const void* object, size_t offset, const std::string& expected) {
    return object == fake_game.data() && ((offset == 0x450 && current_stem == expected) || (offset == 0x470 && current_folder == expected));
}
DWORD call_string(uintptr_t rva, void* receiver, const char* text) {
    if (rva != title_command || receiver != fake_title.data() || std::string(text) != "Load \"snapshot-new\" \"Worlds of Titans\"")
        throw std::runtime_error("unexpected native Title call in fixture");
    ++native_load_calls;
    write_at(fake_title.data(), 0xd0, static_cast<unsigned char>(1));
    current_stem = "snapshot-new"; current_folder = "Worlds of Titans";
    return 0;
}
DWORD store_software_cache(void* object, const SoftwareCache& value) {
    ++cache_stores;
    write_at(object, 0x314, value.rating); write_at(object, 0x318, value.fun);
    return 0;
}
}
bool ready() { return image.load() != 0; }
bool in_game() { return world_active; }
bool read_world_status(WorldStatus& out, std::string& error) {
    if (!native_world_readable) { error = "fixture native world unreadable"; return false; }
    out = {}; out.active_mode_index = world_active ? 3 : 2; out.saves_in_progress = saves;
    unsigned char pending = 1;
    if (!read_at(fake_title.data(), 0xd0, pending)) return false;
    out.title_transition_pending = pending != 0;
    return true;
}
}

namespace mt2mp::game::live_host_state::apply_timing {
enum class Part { QueryEngine };
SIZE_T virtual_query(LPCVOID, PMEMORY_BASIC_INFORMATION memory, SIZE_T size, Part) {
    if (size != sizeof(*memory)) return 0;
    memory->Protect = mt2mp::game::native::memory_writable ? PAGE_READWRITE : PAGE_READONLY;
    return sizeof(*memory);
}
}

namespace mt2mp::game::native {
#include "named_world_bodies.inc"

void require(bool value, const char* cause) { if (!value) throw std::runtime_error(cause); }
void pass(const char* name) { ++passed; std::cout << "PASS " << name << '\n'; }
void reset() {
    software_replica = {};
    fake_game.fill(0); fake_title.fill(0); fake_mode.fill(0); fake_software.fill(0);
    image.store(0x100000); game_thread.store(GetCurrentThreadId());
    world_active = false; folder_readable = true; tick_slot_valid = true; memory_writable = true; native_world_readable = true; saves = 0;
    current_folder = "Worlds of Titans"; current_world = other_world.data(); current_software = fake_software.data();
    cache_stores = native_load_calls = 0; current_stem.clear();
    vtables = {{fake_game.data(), 0x1743730}, {fake_title.data(), 0x16dfba0},
               {fake_software.data(), 0x1715440}, {fake_software.data() + 0x18, 0x1715500},
               {other_software.data(), 0x1715440}, {other_software.data() + 0x18, 0x1715500}};
    write_at(fake_game.data(), 0x350, static_cast<void*>(fake_title.data()));
    write_at(fake_game.data(), 0x28, static_cast<void*>(fake_title.data()));
    write_at(fake_title.data(), 0x88, static_cast<void*>(other_world.data()));
    write_at(fake_title.data(), 0xd8, static_cast<void*>(other_software.data()));
    write_at(fake_mode.data(), 0x58, current_world);
    write_at(fake_software.data(), 0x314, 100.0f); write_at(fake_software.data(), 0x318, 2.0f);
}
void arm(const std::string& name = "Worlds of Titans") {
    std::string error;
    require(arm_software_replica(17, error, name), error.c_str());
    world_active = true;
}
bool apply_initial(std::string& error, uint64_t epoch = 17, uint64_t revision = 1, SoftwareCache host = {120, 3}) {
    SoftwareCacheReceipt receipt;
    return apply_software_cache(epoch, revision, {100, 2}, host, receipt, error);
}
void tests() {
    reset(); arm();
    SoftwareState state; std::string error;
    require(read_software_state(state, error) && state.replica_bound && state.replica_error.empty(), "actual state-read latched named world as a diagnostic failure");
    require(!software_calculation_should_forward(current_software, SoftwareCalcLeaf::Rating), "named-world rating calculation was not suppressed");
    SoftwareCacheReceipt receipt;
    require(apply_software_cache(17, 1, {100, 2}, {120, 3}, receipt, error) && receipt.written && cache_stores == 1, "named snapshot did not apply its initial host caches once");
    require(apply_software_cache(17, 1, {100, 2}, {120, 3}, receipt, error) && receipt.already_applied && !receipt.written && cache_stores == 1, "identical confirmed cache revision was replayed");
    pass("named Host snapshot read/bind/calculate/apply and duplicate without a second store");

    reset(); current_folder = "Überwald 世界"; arm(current_folder);
    require(apply_initial(error), error.c_str()); pass("exact UTF-8 named snapshot accepted");

    reset(); arm(); current_folder = "Worlds of Titans 2";
    require(!apply_initial(error) && cache_stores == 0 && error.find("armed snapshot world") != std::string::npos, "mismatched named world was written");
    pass("different actual folder rejects before cache store");

    reset(); arm("");
    require(!apply_initial(error) && cache_stores == 0, "legacy no-proof route admitted a normal named world");
    pass("legacy route retains named-world restriction");
    reset(); current_folder = "MPTestSession"; arm(""); require(apply_initial(error), error.c_str());
    pass("legacy MPTest route remains accepted");

    reset(); arm(); require(bind_software_replica(current_software), "first world bind failed");
    write_at(fake_mode.data(), 0x58, static_cast<void*>(fake_game.data()));
    require(!apply_initial(error) && cache_stores == 0 && error.find("identity changed") != std::string::npos, "changed world bypassed pin");
    pass("world pointer stays pinned");
    reset(); arm(); require(bind_software_replica(current_software), "first software bind failed"); current_software = other_software.data();
    require(!apply_initial(error) && cache_stores == 0 && error.find("identity changed") != std::string::npos, "changed software bypassed pin");
    pass("software pointer stays pinned");

    for (const std::string& bad : std::array<std::string, 8>{"../World", "save/World", "C:\\World", "CON", "World.", "World ", std::string(201, 'x'), std::string("Bad\0Name", 8)}) {
        reset(); require(!arm_software_replica(17, error, bad) && !software_replica.armed && cache_stores == 0, "malformed explicit folder armed native policy");
    }
    pass("malformed explicit folder preflight rejects before arming");
    reset(); require(!arm_software_replica(0, error, "Worlds of Titans") && !software_replica.armed, "zero epoch armed");
    pass("zero epoch rejects");
    reset(); game_thread.store(GetCurrentThreadId() + 1); require(!arm_software_replica(17, error, current_folder) && !software_replica.armed, "wrong-thread arm succeeded");
    pass("wrong-thread arm rejects");
    reset(); write_at(fake_title.data(), 0xd0, static_cast<unsigned char>(1));
    require(!arm_software_replica(17, error, current_folder) && !software_replica.armed, "pending Title armed");
    pass("pending original Title rejects");
    reset(); vtables[fake_game.data()] = 0;
    require(!arm_software_replica(17, error, current_folder) && !software_replica.armed, "wrong game AP armed");
    pass("exact native game AP required");
    reset(); arm(); require(!arm_software_replica(17, error, "Worlds of Titans") && software_replica.world_folder == "Worlds of Titans", "same epoch rearmed");
    pass("already armed epoch rejects");

    reset(); arm(); game_thread.store(GetCurrentThreadId() + 1);
    require(!apply_initial(error) && cache_stores == 0, "wrong-thread apply wrote"); pass("wrong-thread apply rejects");
    reset(); arm(); vtables[current_software] = 0;
    require(!apply_initial(error) && cache_stores == 0 && error.find("primary_vtable_mismatch") != std::string::npos, "wrong software AP wrote");
    pass("software receiver AP guard remains");
    reset(); arm(); tick_slot_valid = false;
    require(!apply_initial(error) && cache_stores == 0 && error.find("tick_slot_mismatch") != std::string::npos, "wrong native tick slot wrote");
    pass("exact native tick slots remain required");
    reset(); arm(); folder_readable = false;
    require(!apply_initial(error) && cache_stores == 0 && error.find("folder is unreadable") != std::string::npos, "unreadable native folder wrote");
    pass("unreadable current folder retains distinct cause");
    reset(); arm(); write_at(fake_software.data(), 0x314, std::numeric_limits<float>::quiet_NaN());
    require(!apply_initial(error) && cache_stores == 0, "nonfinite initial cache wrote"); pass("nonfinite initial cache rejects");
    reset(); arm(); require(!apply_initial(error, 18) && cache_stores == 0, "wrong epoch wrote"); pass("wrong bound epoch rejects");
    reset(); arm(); require(!apply_initial(error, 17, 0) && cache_stores == 0, "zero revision wrote"); pass("zero revision rejects");
    reset(); arm(); require(!apply_initial(error, 17, 1, {std::numeric_limits<float>::infinity(), 3}) && cache_stores == 0, "nonfinite host cache wrote");
    pass("nonfinite authoritative cache rejects");
    reset(); arm(); memory_writable = false;
    require(!apply_initial(error) && cache_stores == 0 && error.find("not writable") != std::string::npos, "readonly cache wrote");
    pass("cache write permission remains required");
    reset(); arm(); require(apply_initial(error), error.c_str());
    require(!apply_initial(error, 17, 1, {121, 3}) && cache_stores == 1 && error.find("stale or conflicting") != std::string::npos, "conflicting revision replayed");
    pass("conflicting confirmed revision rejects without replay");

    reset(); image.store(0);
    require(!arm_software_replica(17, error, current_folder) && !software_replica.armed, "unresolved build armed policy");
    pass("exact-build resolver remains required");

    reset(); arm(); current_folder = "wrong actual world";
    require(!apply_initial(error) && software_replica.armed && !software_replica.error.empty(), "failed bind fixture did not retain original epoch");
    world_active = false;
    require(!request_load("save/Worlds of Titans/snapshot-new.vrz", error) && native_load_calls == 0, "existing replacement guard was bypassed before retirement");
    require(retire_software_replica(error) && !software_replica.armed && software_replica.epoch == 0 && software_replica.error.empty(), "failed bind did not retire exact internal epoch at Title");
    require(request_load("save/Worlds of Titans/snapshot-new.vrz", error) && native_load_calls == 1, "actual request_load could not reach original Title call after retirement");
    pass("failed Client replica blocks reload until Title retirement, then actual Host request_load is admitted once");

    for (unsigned condition = 0; condition < 3; ++condition) {
        reset(); arm();
        if (condition == 1) { world_active = false; write_at(fake_title.data(), 0xd0, static_cast<unsigned char>(1)); }
        if (condition == 2) { world_active = false; saves = 1; }
        require(!retire_software_replica(error) && software_replica.armed && software_replica.epoch == 17, "active/pending/saving World lost armed epoch");
    }
    pass("active World, pending Title and native save do not clear replica");
    reset(); arm(); world_active = false; native_world_readable = false;
    require(!retire_software_replica(error) && error == "fixture native world unreadable" && software_replica.armed, "unreadable Title retired or lost cause");
    pass("retirement preserves original native world read failure");
    reset(); arm(); world_active = false; game_thread.store(GetCurrentThreadId() + 1);
    require(!retire_software_replica(error) && software_replica.armed, "foreign thread retired software replica");
    pass("retirement requires native main thread");
    reset(); saves = -1;
    require(retire_software_replica(error) && !software_replica.armed && error.empty(), "clean idle Title noop with no owner failed");
    pass("cold idle Title with native no-save sentinel is a clean noop");
    std::cout << passed << " focused scenarios passed\n";
}
}

int main() {
    try { mt2mp::game::native::tests(); return 0; }
    catch (const std::exception& cause) { std::cerr << "error: named-world regression: " << cause.what() << '\n'; return 1; }
}
