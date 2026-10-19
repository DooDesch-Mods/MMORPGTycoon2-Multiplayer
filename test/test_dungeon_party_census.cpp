#include "dungeon_party_census.h"
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>

namespace census = mt2mp::game::dungeon_party_census;
namespace {
constexpr uintptr_t image = 0x140000000, region = 0x20000, shard = 0x30000;
constexpr uintptr_t manager = 0x40000, party = 0x50000, other = 0x60000, slots = 0x70000;
constexpr uint64_t uid = 0x1122334455667788;
int checks = 0;
void require(bool ok, const char* cause) {
    ++checks;
    if (!ok) throw std::runtime_error(cause);
}
struct Memory {
    std::map<uintptr_t, uint8_t> bytes;
    std::map<uintptr_t, unsigned> reads;
    std::function<void(uintptr_t, unsigned)> before_read;
    template<class T> void put(uintptr_t address, const T& value) {
        const auto* raw = reinterpret_cast<const uint8_t*>(&value);
        for (size_t i = 0; i < sizeof value; ++i) bytes[address + i] = raw[i];
    }
    bool read(uintptr_t address, void* out, size_t count, std::string& error) {
        const unsigned read_count = ++reads[address];
        if (before_read) before_read(address, read_count);
        auto* raw = static_cast<uint8_t*>(out);
        for (size_t i = 0; i < count; ++i) {
            auto it = bytes.find(address + i);
            if (it == bytes.end()) { error = "fixture unreadable native field"; return false; }
            raw[i] = it->second;
        }
        return true;
    }
};
struct Fixture {
    Memory memory;
    census::Pool pool;
    std::string error = "previous unrelated error";
    uintptr_t registry = party;
    Fixture() {
        memory.put(region, image + census::region_ap);
        memory.put(region + 0x70, shard);
        memory.put(region + 0x9d0, manager);
        memory.put(manager, image + census::manager_ap);
        memory.put(manager + 0xa8, region);
        memory.put(manager + 0xb0, uint8_t{0});
        array(0x18, census::parties_ap, slots, 1);
        array(0x58, census::pending_ap, 0, 0);
        array(0x80, census::pending_ap, 0, 0);
        memory.put(slots, party);
        member(party, uid);
    }
    void array(size_t offset, uintptr_t ap, uintptr_t data, int32_t count) {
        memory.put(manager + offset, image + ap);
        memory.put(manager + offset + 8, data);
        memory.put(manager + offset + 0x10, count);
        memory.put(manager + offset + 0x14, count);
    }
    void member(uintptr_t pointer, uint64_t value) {
        memory.put(pointer, image + census::party_ap);
        memory.put(pointer + 0x18, value);
        memory.put(pointer + 0x170, shard);
        memory.put(pointer + 0x178, region);
        memory.put(pointer + 0x620, region);
    }
    bool read() {
        auto find = [this](uint64_t value, uintptr_t& out, std::string& cause) {
            if (value != uid) { cause = "fixture unknown full UID"; return false; }
            out = registry;
            return true;
        };
        return census::read(memory, find, image, shard, region, pool, error);
    }
    void rejected(const char* cause) {
        require(!read(), cause);
        require(pool == census::Pool{}, "failed read leaked a partial census");
        require(!error.empty() && error != "previous unrelated error", "failed read lost its current cause");
    }
};
void run() {
    {
        Fixture f;
        require(f.read(), "normal current Party census failed");
        require(f.error.empty(), "successful read kept stale error");
        require(f.pool.members.size() == 1 && f.pool.members[0].uid == uid, "full UID was truncated");
        require(census::settled(f.pool), "normal settled manager rejected");
    }
    {
        Fixture f;
        f.array(0x18, census::parties_ap, 0, 0);
        require(f.read() && f.pool.members.empty(), "empty original manager failed");
    }
    {
        Fixture f;
        f.memory.put(region, image + census::region_ap + 8);
        f.rejected("foreign Region class accepted");
        require(f.error == "Party census Region class/Shard differs", "stale cause masked failed Region guard");
    }
    { Fixture f; f.memory.bytes.erase(party + 0x178); f.rejected("unreadable member accepted");
      require(f.error == "fixture unreadable native field", "original read failure replaced"); }
    { Fixture f; f.memory.put(manager + 0xa8, other); f.rejected("foreign reciprocal manager accepted"); }
    { Fixture f; f.memory.put(party, image + census::party_ap + 8); f.rejected("foreign Party class accepted"); }
    { Fixture f; f.registry = other; f.rejected("UID registry points to different native Party"); }
    { Fixture f; f.memory.put(party + 0x18, uint64_t{0}); f.rejected("zero native UID accepted"); }
    {
        Fixture f;
        f.array(0x18, census::parties_ap, slots, 2);
        f.memory.put(slots + 8, party);
        f.rejected("duplicate native pointer accepted");
    }
    {
        Fixture f;
        f.array(0x18, census::parties_ap, slots, 2);
        f.memory.put(slots + 8, other);
        f.member(other, uid);
        f.rejected("duplicate full UID accepted");
    }
    { Fixture f; f.memory.put(manager + 0x28, int32_t{-1}); f.rejected("negative native count accepted"); }
    { Fixture f; f.memory.put(manager + 0x2c, int32_t{0}); f.rejected("count beyond capacity accepted"); }
    { Fixture f; f.memory.put(manager + 0x20, UINTPTR_MAX - 3); f.rejected("overflowing native span accepted"); }
    {
        Fixture f;
        f.array(0x58, census::pending_ap, slots, 1);
        require(f.read(), "native deferred deletion queue was not retained");
        require(f.pool.removals.members == std::vector<uintptr_t>{party} &&
                f.pool.releases.members.empty() && !census::settled(f.pool), "deletion queue mislabeled or hidden");
    }
    {
        Fixture f;
        f.array(0x80, census::pending_ap, slots, 1);
        f.memory.put(party + 0x620, other);
        f.memory.put(party + 0x170, other);
        f.memory.put(party + 0x178, other);
        require(f.read(), "native pending manager transfer was rejected");
        require(f.pool.members[0].manager_region == other &&
                f.pool.releases.members == std::vector<uintptr_t>{party} && !census::settled(f.pool),
                "manager transfer owner or deferred release was rewritten");
    }
    {
        Fixture f;
        f.memory.put(party + 0x620, uintptr_t{0});
        require(f.read() && f.pool.members[0].manager_region == 0, "native loaded null activity owner was invented");
    }
    {
        Fixture f;
        f.memory.put(manager + 0xb0, uint8_t{1});
        require(f.read() && !census::settled(f.pool), "manager mid-update was presented as settled");
    }
    {
        Fixture f;
        f.memory.before_read = [&f](uintptr_t address, unsigned n) {
            if (address == manager + 0x28 && n == 2) f.memory.put(address, int32_t{0});
        };
        f.rejected("changed native main array accepted");
    }
    {
        Fixture f;
        f.memory.before_read = [&f](uintptr_t address, unsigned n) {
            if (address == party + 0x620 && n == 2) f.memory.put(address, other);
        };
        f.rejected("changed native transfer owner accepted");
    }
    {
        Fixture f;
        f.memory.before_read = [&f](uintptr_t address, unsigned n) {
            if (address == manager + 0xb0 && n == 2) f.memory.put(address, uint8_t{1});
        };
        f.rejected("changed native update boundary accepted");
    }
}
}
int main() {
    try { run(); std::cout << "PASS dungeon_party_census " << checks << " comparisons\n"; return 0; }
    catch (const std::exception& error) { std::cerr << "FAIL dungeon_party_census: " << error.what() << '\n'; return 1; }
}
