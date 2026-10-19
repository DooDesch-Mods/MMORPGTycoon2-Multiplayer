#include "monster_snapshot_companion.h"
#include <iostream>
#include <stdexcept>
namespace companion = mt2mp::game::monster_snapshot_companion;
namespace {
int comparisons = 0;
void require(bool value, const char* cause) { ++comparisons; if (!value) throw std::runtime_error(cause); }
void run() {
    companion::Manifest source;
    source.native_sha256 = std::string(64, 'a');
    source.native_bytes = 500ull * 1024 * 1024;
    std::string bytes, error;
    companion::Manifest actual;
    require(companion::encode(source, bytes, error), "legacy empty Monster companion failed");
    require(companion::decode(bytes, source.native_sha256, source.native_bytes, actual, error) && actual == source,
            "legacy revision1 changed after Party extension");
    source.network.present = true;
    require(companion::encode(source, bytes, error) && companion::decode(bytes, source.native_sha256, source.native_bytes, actual, error) && actual == source,
            "legacy revision2 changed after Party extension");
    source.dungeon_gizmos.present = true;
    require(companion::encode(source, bytes, error) && companion::decode(bytes, source.native_sha256, source.native_bytes, actual, error) && actual == source,
            "legacy revision3 changed after Party extension");
    source.parties.present = true;
    source.parties.rows.push_back({source.native_sha256 + ":world:party:7",
        "action:11111111-2222-3333-4444-555555555555:party:0"});
    require(companion::encode(source, bytes, error), "Party companion serialization failed");
    auto wire = nlohmann::json::parse(bytes);
    require(wire.at("revision") == 5 && wire.contains("parties") && wire.contains("dungeon_gizmos") && wire.contains("network"),
            "Party companion dropped another alias family or used wrong revision");
    require(companion::decode(bytes, source.native_sha256, source.native_bytes, actual, error) && actual == source,
            "complete500MB native-file identity and Party canonical aliases changed");
    require(!companion::decode(bytes, source.native_sha256, source.native_bytes + 1, actual, error), "wrong native file length accepted");
    require(!companion::decode(bytes, std::string(64, 'b'), source.native_bytes, actual, error), "wrong native file checksum accepted");
    wire.erase("parties");
    require(!companion::decode(wire.dump(), source.native_sha256, source.native_bytes, actual, error), "revision5 without actual Party attachment accepted");
    wire = nlohmann::json::parse(bytes);
    wire["parties"].push_back(wire["parties"][0]);
    require(!companion::decode(wire.dump(), source.native_sha256, source.native_bytes, actual, error), "duplicate actual saved Party identity accepted");
    wire = nlohmann::json::parse(bytes);
    wire["unrecognized"] = true;
    require(!companion::decode(wire.dump(), source.native_sha256, source.native_bytes, actual, error), "unknown companion field accepted");
    source.network = {};
    source.dungeon_gizmos = {};
    require(companion::encode(source, bytes, error) && companion::decode(bytes, source.native_sha256, source.native_bytes, actual, error) && actual == source,
            "Party-only alias attachment requires unrelated families");
}
}
int main() {
    try { run(); std::cout << "PASS party_snapshot_companion " << comparisons << " comparisons\n"; return 0; }
    catch (const std::exception& error) { std::cerr << "FAIL party_snapshot_companion: " << error.what() << '\n'; return 1; }
}
