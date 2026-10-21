#include "job_target_observation.h"
#include <cstring>
#include <iostream>
#include <map>

using namespace mt2mp::game;
namespace {
std::map<uintptr_t, unsigned char> bytes;
remove_diagnostics::LocalTarget bound;
std::string resolver_error;
bool change_second = false;
bool reject_cached_read = false, change_weak_second = false;
int weak_reads = 0, cached_reads = 0;
int resolves = 0, checks = 0, failures = 0;
int modular_checks = 0;
bool modular_ok = true;
uint64_t modular_uid = 0;
template<class T> void put(uintptr_t p, const T& value) {
    const auto* data = reinterpret_cast<const unsigned char*>(&value);
    for (size_t n = 0; n < sizeof(T); ++n) bytes[p + n] = data[n];
}
bool read(uintptr_t p, void* target, size_t size, std::string& error) {
    if (p >= 0x40000000 && p < 0x40001000) {
        ++cached_reads;
        if (reject_cached_read) { error = "freed cached receiver was read"; return false; }
    }
    if (p == 0x41000000 && ++weak_reads == 2 && change_weak_second) put(p, uintptr_t(0));
    auto* data = static_cast<unsigned char*>(target);
    for (size_t n = 0; n < size; ++n) {
        const auto found = bytes.find(p + n);
        if (found == bytes.end()) { error = "original unreadable field"; return false; }
        data[n] = found->second;
    }
    return true;
}
void check(bool value, const char* name) {
    ++checks;
    if (!value) { ++failures; std::cerr << "FAIL " << name << '\n'; }
}
}
namespace mt2mp::game::remove_diagnostics {
bool resolve_local_target(uintptr_t, uintptr_t, LocalTarget& out, std::string& error) {
    ++resolves;
    if (!resolver_error.empty()) { error = resolver_error; return false; }
    out = bound;
    if (change_second && resolves == 2) out.record_key += "changed";
    return true;
}
bool is_modular_target(const LocalTarget& target, std::string& error) {
    ++modular_checks; modular_uid = target.uid;
    if (!modular_ok) { error = "original strict Modular proof failure"; return false; }
    return target.type == "Scenery" && target.variant == "base";
}
}
int main() {
    namespace j = developer_staff::job_census;
    developer_staff::Capture capture;
    capture.image = 0x10000000; capture.local.lounge = 0x20000000;
    capture.pin.origin.uuid = "test-snapshot";
    capture.pin.origin.world_epoch = 1; capture.pin.origin.generation = 2;
    j::Census census; j::Job job;
    job.pointer = 0x30000000; job.building = 0x40000000; job.region = 0x50000000;
    job.building_proxy = 0x41000000; put(job.building_proxy, job.building + 0x20);
    census.jobs.push_back(job);
    const uintptr_t selection = 0x60000000, shard = 0x70000000;
    put(capture.image + 0x13df630, selection); put(selection, capture.image + uintptr_t(0x17205d0));
    put(capture.local.lounge + 0x56, uint8_t(1)); put(capture.local.lounge + 0x57, uint8_t(0));
    put(job.building + 0x170, shard); put(job.building + 0x178, job.region);
    put(shard + 0x18, uintptr_t(0)); put(shard + 0x20, uintptr_t(0));
    put(job.building + 0x7a8, int32_t(1)); put(job.building + 0x7e1, uint8_t(0));
    put(job.building + 0xa8e, uint8_t(1));
    bound = {selection, job.building, 42, 1, 2, "test-snapshot", "action:test", "SmallInn", "base", true, false, {}};
    nlohmann::json out; std::string error;
    auto run = [&] { resolves = 0; weak_reads = 0; cached_reads = 0; modular_checks = 0; modular_uid = 0; return j::observe_building_targets(read, capture, census, out, error); };
    check(run() && out.at("observed").get<bool>() && !out.at("job_identity_proven").get<bool>() &&
          out.at("targets")[0].at("building_key") == "action:test", "exact target without Job identity claim");
    check(modular_checks == 0, "normal Building keeps its existing proof path");
    const auto normal = bound;
    bound.normal_building = false; bound.type = "Scenery";
    check(run() && modular_checks == 2 && modular_uid == 42 && out.at("targets")[0].at("type") == "Scenery" &&
          !out.at("targets")[0].at("field_7e1").get<bool>() && !out.at("job_identity_proven").get<bool>(),
          "admitted Modular uses strict full UID proof on both reads");
    modular_ok = false;
    check(!run() && modular_checks == 1 && error == "original strict Modular proof failure", "Modular proof failure remains visible");
    modular_ok = true; bound.variant = "other";
    check(!run() && modular_checks == 0, "Scenery family alone is not Modular proof");
    bound.variant = "base"; bound.normal_scenery = true;
    check(!run() && modular_checks == 0, "normal Scenery cannot enter Modular proof");
    bound.normal_scenery = false; bound.uid = 0;
    check(!run() && modular_checks == 0 && error.find("Origin/receiver") != std::string::npos, "Modular zero full UID is rejected before proof");
    bound.uid = 42; bound.world_epoch = 3;
    check(!run() && modular_checks == 0 && error.find("Origin/receiver") != std::string::npos, "Modular stale epoch is rejected before proof");
    bound.world_epoch = 1; bound.manager += 8;
    check(!run() && modular_checks == 0, "Modular wrong manager is rejected before proof");
    bound.manager -= 8; bound.object += 8;
    check(!run() && modular_checks == 0, "Modular wrong receiver is rejected before proof");
    bound.object -= 8; change_second = true;
    check(!run() && modular_checks == 2 && error.find("changed between reads") != std::string::npos, "Modular binding drift is rejected");
    change_second = false; bound = normal;
    bound.snapshot_id = "shared-snapshot";
    check(run() && out.at("targets")[0].at("binding_snapshot_uuid") == "shared-snapshot" &&
          out.at("targets")[0].at("native_origin_uuid") == "test-snapshot",
          "shared binding snapshot may differ from native load Origin");
    bound.snapshot_id.clear();
    check(!run() && error.find("Origin/receiver") != std::string::npos, "empty binding snapshot rejected");
    bound.snapshot_id = "test-snapshot";
    bound.world_generation = 3;
    check(!run() && error.find("Origin/receiver") != std::string::npos, "stale target generation");
    bound.world_generation = 2; bound.object += 8;
    check(!run(), "wrong bound receiver"); bound.object -= 8;
    resolver_error = "original binding failure";
    check(!run() && error == resolver_error, "preserve resolver cause"); resolver_error.clear();
    change_second = true;
    check(!run() && error.find("changed between reads") != std::string::npos, "changed target binding");
    change_second = false;
    bytes.erase(job.building + 0x7a8);
    check(!run() && error == "original unreadable field", "preserve read cause");
    put(job.building + 0x7a8, int32_t(1)); put(capture.local.lounge + 0x56, uint8_t(2));
    check(!run() && error.find("not Boolean") != std::string::npos, "invalid policy flag");
    put(capture.local.lounge + 0x56, uint8_t(1));
    put(job.building_proxy, uintptr_t(0)); reject_cached_read = true;
    resolver_error = "destroyed receiver must not enter resolver";
    check(run() && resolves == 0 && cached_reads == 0 && out.at("targets").empty() &&
          out.at("expired_targets").size() == 1 && !out.at("building_binding_observed").get<bool>() &&
          out.at("building_binding_cause").get<std::string>().find("expired") != std::string::npos,
          "null weak target precedes cached receiver resolution and every field read");
    reject_cached_read = false; resolver_error.clear();
    bytes.erase(job.building_proxy);
    check(!run() && error == "original unreadable field" && resolves == 0, "unreadable weak proxy preserves cause");
    put(job.building_proxy, job.building + 0x28);
    check(!run() && error.find("weak proxy") != std::string::npos && resolves == 0, "mismatched weak target rejects before cached receiver");
    put(job.building_proxy, job.building + 0x20); change_weak_second = true;
    check(!run() && error.find("changed between reads") != std::string::npos, "weak target expiration during observation is not stable");
    change_weak_second = false; put(job.building_proxy, job.building + 0x20);
    census.jobs[0].building_proxy = 0;
    check(!run() && error.find("no weak proxy") != std::string::npos && resolves == 0, "cached receiver without proxy is not live");
    census.jobs.clear();
    check(run() && resolves == 0 && out.at("targets").empty() &&
          !out.at("building_binding_observed").get<bool>() &&
          out.at("building_binding_cause").get<std::string>().find("no Building target") != std::string::npos,
          "empty queue states why no binding was observed");
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
