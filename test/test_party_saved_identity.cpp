#include "party_saved_identity.h"
#include "party_identity.h"
#include "party_snapshot_alias.h"
#include <functional>
#include <iostream>
#include <stdexcept>

namespace saved = mt2mp::game::party_saved_identity;
namespace identity = mt2mp::game::party_identity;
namespace keys = mt2mp::game::snapshot_party_key;
namespace aliases = mt2mp::game::party_snapshot_alias;
namespace {
constexpr uintptr_t fixture_image = 0x140000000, fixture_manager = 0x20000;
constexpr uintptr_t fixture_party = 0x30000, fixture_other = 0x40000, fixture_data = 0x50000;
constexpr uintptr_t fixture_reader = 0x60000, fixture_writer = 0x70000, fixture_context = 0x80000;
constexpr uintptr_t fixture_property = fixture_image + 0x13c0140;
const std::string snapshot = "11111111-2222-3333-4444-555555555555", sha(64, 'a');
int comparisons = 0;
void require(bool value, const char* cause) { ++comparisons; if (!value) throw std::runtime_error(cause); }
struct Memory {
    std::map<uintptr_t, uint8_t> bytes;
    template<class T> void put(uintptr_t address, const T& value) {
        const auto* p = reinterpret_cast<const uint8_t*>(&value);
        for (size_t i = 0; i < sizeof value; ++i) bytes[address + i] = p[i];
    }
    bool read(uintptr_t address, void* out, size_t size, std::string& error) {
        auto* p = static_cast<uint8_t*>(out);
        for (size_t i = 0; i < size; ++i) {
            const auto found = bytes.find(address + i);
            if (found == bytes.end()) { error = "fixture unreadable original object"; return false; }
            p[i] = found->second;
        }
        return true;
    }
};
struct Fixture {
    Memory memory;
    std::string error;
    Fixture(int32_t count = 1) {
        memory.put(fixture_property, fixture_image + 0x170e670);
        memory.put(fixture_property + 0x38, uint64_t{0x18});
        memory.put(fixture_manager, fixture_image + 0x17018d0);
        memory.put(fixture_manager + 0x18, fixture_image + 0x16f0180);
        memory.put(fixture_manager + 0x20, fixture_data);
        memory.put(fixture_manager + 0x28, count);
        memory.put(fixture_manager + 0x2c, int32_t{2});
        memory.put(fixture_data, fixture_party);
        memory.put(fixture_data + 8, fixture_other);
        for (const auto object : {fixture_party, fixture_other}) {
            memory.put(object, fixture_image + 0x174a370);
            memory.put(object + 8, fixture_context);
            memory.put(object + 0x18, uint64_t{0x1122334400000000} + object);
        }
    }
    bool links(uintptr_t object, int32_t& value, std::string& cause) {
        if (object != fixture_party && object != fixture_other) { cause = "unknown original fixture object"; return false; }
        value = object == fixture_party ? 7 : 0;
        return true;
    }
};
void streams() {
    {
        Fixture f;
        auto links = [&](uintptr_t p, int32_t& id, std::string& e) { return f.links(p, id, e); };
        saved::SaveFrame frame;
        require(saved::begin_save(f.memory, fixture_image, fixture_property, fixture_manager, fixture_writer,
                                 fixture_context, links, frame, f.error), "original save receipt rejected");
        saved::Pool pool;
        require(saved::finish_save(f.memory, fixture_image, frame, fixture_property, fixture_manager, fixture_writer,
                                  fixture_context, true, links, pool, f.error), "unchanged original save rejected");
        require(pool.records.size() == 1 && pool.records[0].link_id == 7 && pool.records[0].uid > UINT32_MAX,
                "original save link/full UID was replaced");
        require(!saved::finish_save(f.memory, fixture_image, frame, fixture_property, fixture_manager, fixture_writer,
                                   fixture_context, false, links, pool, f.error), "false original save became success");
        f.memory.put(fixture_party + 0x18, uint64_t{9});
        require(!saved::finish_save(f.memory, fixture_image, frame, fixture_property, fixture_manager, fixture_writer,
                                   fixture_context, true, links, pool, f.error), "changed original save identity accepted");
    }
    {
        Fixture f;
        f.memory.put(fixture_property + 0x38, uint64_t{0x38});
        require(!saved::property(f.memory, fixture_image, fixture_property, fixture_manager, f.error),
                "property field address confused with actual array member offset");
    }
    {
        Fixture f(0);
        saved::LoadFrame frame;
        require(saved::begin_load(f.memory, fixture_image, fixture_property, fixture_manager, fixture_reader,
                                 fixture_context, frame, f.error), "fresh original load receiver rejected");
        require(!frame.children(saved::load_children_return_rva, fixture_reader, "boss", 2, 2, f.error),
                "foreign serialized label accepted");
        require(frame.children(saved::load_children_return_rva, fixture_reader, "party", 2, 2, f.error),
                "original Party child count rejected");
        require(saved::registered(f.memory, fixture_image, frame, saved::load_link_return_rva, fixture_context,
                                  7, fixture_party, f.error), "original linked Party registration rejected");
        require(saved::registered(f.memory, fixture_image, frame, saved::load_inline_return_rva, fixture_context,
                                  0, fixture_other, f.error), "original inline Party registration rejected");
        f.memory.put(fixture_manager + 0x28, int32_t{2});
        saved::Pool pool;
        require(frame.finish(f.memory, fixture_image, pool, f.error), "complete original Party load failed");
        require(pool.records.size() == 2 && pool.records[0].pointer == fixture_party && pool.records[1].link_id == 0,
                "original native registration order changed");
        f.memory.put(fixture_data, fixture_other);
        f.memory.put(fixture_data + 8, fixture_party);
        require(!frame.finish(f.memory, fixture_image, pool, f.error), "unobserved native reordering accepted");
    }
    {
        Fixture f(0);
        saved::LoadFrame frame;
        require(saved::begin_load(f.memory, fixture_image, fixture_property, fixture_manager, fixture_reader,
                                 fixture_context, frame, f.error), "empty load frame failed");
        saved::Pool pool;
        require(!frame.finish(f.memory, fixture_image, pool, f.error), "missing original children became empty success");
        require(frame.children(saved::load_children_return_rva, fixture_reader, "party", 0, 0, f.error) &&
                frame.finish(f.memory, fixture_image, pool, f.error), "actual empty serialized Party array rejected");
    }
    {
        saved::LoadFrame frame;
        frame.reader = fixture_reader;frame.context = fixture_context;
        std::string error;
        require(!frame.children(saved::load_children_return_rva + 1, fixture_reader, "party", 1, 1, error), "foreign call return accepted");
        require(frame.children(saved::load_children_return_rva, fixture_reader, "party", 1, 1, error), "exact child callback rejected");
        require(!frame.registered(saved::load_inline_return_rva, fixture_context, 8, fixture_party, error), "inline callback accepted invented link ID");
        require(!frame.registered(saved::load_link_return_rva, fixture_context, 0, fixture_party, error), "linked callback accepted missing link ID");
        require(frame.registered(saved::load_link_return_rva, fixture_context, 8, fixture_party, error), "exact linked registration failed");
        require(!frame.registered(saved::load_link_return_rva, fixture_context, 8, fixture_other, error), "duplicate link/count accepted");
    }
}
void canonical() {
    const identity::Scope world{snapshot, 3, 7, 0x90000};
    const std::string record = sha + ":world:party:7";
    const std::string root_inline = sha + ":world:party:" + keys::root_suffix(3, 4);
    const std::string template_inline = sha + ":world:party:" + keys::scene_suffix(3, 4, -1, 5);
    const std::string instance_inline = sha + ":world:party:" + keys::scene_suffix(3, 4, 8, 5);
    require(keys::valid(record) && keys::valid(root_inline) && keys::valid(template_inline) && keys::valid(instance_inline),
            "actual saved Party canonical paths rejected");
    require(!keys::valid(sha + ":world:party:0") && !keys::valid(sha + ":world:boss:7"), "invalid Party domain/link accepted");
    const std::string born = identity::birth(snapshot, 0);
    require(identity::valid_key(born) && !identity::valid_key("action:" + snapshot + ":party:"), "original birth grammar differs");
    identity::Registry host_registry, client_registry;
    const identity::Entry host{world, fixture_party, 0x1122334400000001, false};
    const identity::Entry client{world, fixture_other, 0x1122334400000002, false};
    std::string error, key;
    require(host_registry.restore(world, {{record, host}}, error) && client_registry.restore(world, {{record, client}}, error),
            "same original saved record could not bind different local native full UIDs");
    require(host_registry.identify(world, host.pointer, host.uid, key, error) && key == record, "Host canonical resolution failed");
    require(client_registry.identify(world, client.pointer, client.uid, key, error) && key == record, "Client canonical resolution differs");
    auto foreign = world;foreign.epoch++;
    require(!host_registry.identify(foreign, host.pointer, host.uid, key, error), "old completed-world lifetime accepted");
    require(!host_registry.restore(world, {{record, client}}, error), "snapshot seal replaced a current local binding");
    const identity::Entry new_party{world, 0xa0000, 0x1122334400000003, false};
    require(host_registry.append(born, new_party, error), "confirmed original Party birth rejected");
    require(!host_registry.append(identity::birth(snapshot, 1), new_party, error), "original birth reused existing full UID");
    require(host_registry.retire(born, new_party, error), "original Party retirement failed");
    identity::Entry found;
    require(!host_registry.lookup(world, born, found, error), "retired Party remained resolvable");
    std::string capture;
    require(host_registry.capture_key(world, host.pointer, host.uid, root_inline, capture, error) && capture == record,
            "later native save renamed original canonical Party");
    aliases::Manifest manifest;manifest.present = true;manifest.rows.push_back({root_inline, record});
    require(aliases::valid(manifest, sha, error), "Party resnapshot aliases rejected");
    aliases::Manifest decoded;
    require(aliases::decode(aliases::encode(manifest), sha, decoded, error) && decoded == manifest, "Party alias attachment roundtrip failed");
    manifest.rows.push_back({root_inline, record});
    require(!aliases::valid(manifest, sha, error), "repeated Party saved-record alias accepted");
}
}
int main() {
    try { streams(); canonical(); std::cout << "PASS party_saved_identity " << comparisons << " comparisons\n"; return 0; }
    catch (const std::exception& error) { std::cerr << "FAIL party_saved_identity: " << error.what() << '\n'; return 1; }
}
