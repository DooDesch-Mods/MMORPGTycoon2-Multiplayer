#pragma once
#include "developer_staff/job_census.h"
#include "remove_diagnostics.h"

namespace mt2mp::game::developer_staff::job_census {
// Read-only diagnostic extension. This proves the existing Building binding,
// not the identity or lifetime of the Job that happens to reference it.
inline bool observe_building_targets(const Read& read, const Capture& capture,
                                    const Census& census, nlohmann::json& out,
                                    std::string& error) {
    out = {{"observed", false}};
    error.clear();
    const auto fail = [&](const std::string& cause) { error = cause; return false; };
    const auto at = [&]<class T>(uintptr_t object, size_t offset, T& value) {
        if (!object || object > UINTPTR_MAX - offset ||
            object + offset > UINTPTR_MAX - sizeof(T))
            return fail("Job target observation field address overflow");
        return read(object + offset, &value, sizeof(T), error);
    };
    if (!read || !capture.image || !capture.local.lounge)
        return fail("Job target observation requires authorized Capture and reader");
    const auto once = [&](nlohmann::json& result) {
        uint8_t activate = 0, auto_assign = 0;
        if (!at(capture.local.lounge, 0x56, activate) ||
            !at(capture.local.lounge, 0x57, auto_assign)) return false;
        if (activate > 1 || auto_assign > 1)
            return fail("Job target observation policy flags are not Boolean");
        result = {{"activate_buildings", activate != 0},
                  {"auto_assign_project_developers", auto_assign != 0},
                  {"targets", nlohmann::json::array()},
                  {"expired_targets", nlohmann::json::array()}};
        uintptr_t selection = 0, ap = 0;
        for (size_t index = 0; index < census.jobs.size(); ++index) {
            const auto& job = census.jobs[index];
            if (!job.building) continue;
            // The weak proxy owns lifetime truth. Its cached receiver can still
            // contain a freed Building address until the original lazy cleanup.
            if (!job.building_proxy)
                return fail("Job target cached Building receiver has no weak proxy");
            uintptr_t weak_target = 0;
            if (!at(job.building_proxy, 0, weak_target)) return false;
            if (!weak_target) {
                result["expired_targets"].push_back({{"queue_index_diagnostic", index},
                    {"cached_receiver_present", true}, {"weak_proxy_present", true},
                    {"weak_target_alive", false}, {"building_binding_observed", false},
                    {"cause", "Original Building weak target expired; cached receiver was not resolved or read"}});
                continue;
            }
            if (job.building > UINTPTR_MAX - 0x20 || weak_target != job.building + 0x20)
                return fail("Job target weak proxy does not name the cached Building receiver");
            if (!selection) {
                if (!at(capture.image, 0x13df630, selection) || !at(selection, 0, ap)) return false;
                if (ap != capture.image + 0x17205d0)
                    return fail("Job target observation SelectionManager class differs");
            }
            remove_diagnostics::LocalTarget target;
            if (!remove_diagnostics::resolve_local_target(selection, job.building, target, error)) return false;
            const auto& origin = capture.pin.origin;
            if (!target.normal_building || target.object != job.building || target.manager != selection ||
                target.snapshot_id.empty() || target.world_epoch != origin.world_epoch ||
                target.world_generation != origin.generation || target.record_key.empty() || !target.uid)
                return fail("Job target existing Building binding differs from actual Origin/receiver");
            uintptr_t shard = 0, region = 0, dungeon = 0, instance = 0;
            int32_t state = 0;
            uint8_t field_7e1 = 0, field_a8e = 0;
            if (!at(job.building, 0x170, shard) || !at(job.building, 0x178, region) ||
                !at(shard, 0x18, dungeon) || !at(shard, 0x20, instance) ||
                !at(job.building, 0x7a8, state) || !at(job.building, 0x7e1, field_7e1) ||
                !at(job.building, 0xa8e, field_a8e)) return false;
            if (field_7e1 > 1 || field_a8e > 1)
                return fail("Job target observation Building flags are not Boolean");
            result["targets"].push_back({{"queue_index_diagnostic", index},
                {"building_key", target.record_key}, {"local_uid_diagnostic", target.uid},
                {"binding_snapshot_uuid", target.snapshot_id}, {"native_origin_uuid", origin.uuid},
                {"weak_target_alive", true},
                {"type", target.type}, {"variant", target.variant},
                {"state_7a8", state}, {"field_7e1", field_7e1 != 0}, {"field_a8e", field_a8e != 0},
                {"region_matches_job", region == job.region},
                {"shard_has_dungeon", dungeon != 0}, {"shard_has_instance", instance != 0}});
        }
        return true;
    };
    nlohmann::json first, second;
    if (!once(first) || !once(second)) return false;
    if (first != second) return fail("Job target observation changed between reads");
    out = std::move(first);
    out["observed"] = true;
    out["job_identity_proven"] = false;
    out["building_binding_observed"] = !out.at("targets").empty();
    out["building_binding_cause"] = out.at("targets").empty()
        ? (out.at("expired_targets").empty()
            ? "Queue contains no Building target; no shared Building binding was observed"
            : "Queue contains expired Building weak targets; no live shared Building binding was observed") : "";
    return true;
}
}
