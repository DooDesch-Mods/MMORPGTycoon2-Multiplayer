#pragma once
#include "typed_action/dungeon_gizmo_action.h"
#include "vendor/nlohmann/json.hpp"
namespace mt2mp::game::dungeon_gizmo_portable {
inline nlohmann::json row(const action_session::DungeonGizmoRow& v){const auto& r=v.value;
    return {{"key",v.key},{"name",r.name},{"type",r.type},{"variant",r.variant},{"parent",r.parent},{"rotation",r.rotation},{"position",r.position},{"scale",r.scale},
        {"triggered",r.triggered},{"state",r.state},{"gold",r.gold},{"loot",r.loot},{"vendor_trash",r.vendor_trash},{"keys",r.keys},{"boss_keys",r.boss_keys},
        {"lock_id",r.lock_id},{"edge_a",r.edge_a},{"edge_b",r.edge_b}};
}
inline nlohmann::json state(const action_session::DungeonGizmoState& v,bool random=true){
    auto scenes=nlohmann::json::array();for(const auto& s:v.scenes){auto rows=nlohmann::json::array();for(const auto& r:s.rows)rows.push_back(row(r));scenes.push_back({{"key",s.key},{"rows",rows}});}
    return {{"protected_sha256",v.protected_sha256},{"global_random",random?v.global_random:std::array<uint64_t,4>{}},{"scenes",scenes}};
}
}
