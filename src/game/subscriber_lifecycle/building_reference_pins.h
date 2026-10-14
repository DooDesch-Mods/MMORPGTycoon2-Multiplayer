#pragma once
#include "reference_map.h"
#include "append_calls.h"
#include "remove_diagnostics.h"
#include "native_open_runtime.h"
#include "mt2mp/net/snapshot_sync.h"
#include "mt2mp/game/native_engine.h"
#include <map>
#include <set>
#include <Windows.h>

namespace mt2mp::game::subscriber_building_references {
namespace ref = mt2mp::subscriber_lifecycle::first_append;
using World = building_mixed_identity::opened_file::CurrentWorld;
// Resolve the shared binding while Save0 is required by its existing reader.
// The drained SaveScope uses only these private pins and fresh native readbacks.
class Pins final : public ref::BuildingReferences {
    struct Pin { remove_diagnostics::LocalTarget target; native::BuildingState state; };
    uintptr_t image_ = 0;
    World world_;
    std::string snapshot_;
    std::map<std::string, Pin> pins_;
    std::set<std::string> used_;
    static bool fail(std::string& e,const char* s) { e=s;return false; }
    bool current(std::string& e) const {
        World actual;
        if(!native_open_runtime::read_current_world(actual,e))return false;
        return (actual.uuid==world_.uuid&&actual.sha256==world_.sha256&&actual.world_epoch==world_.world_epoch&&
            actual.generation==world_.generation&&actual.world==world_.world&&actual.map==world_.map&&actual.state==world_.state&&
            net::current_snapshot_id()==snapshot_)||fail(e,"Subscriber Building reference current shared/local world differs");
    }
    bool addresspoint(uintptr_t object,uintptr_t& value,std::string& e) const {
        SIZE_T copied=0;
        if(!object||!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(object),&value,sizeof value,&copied)||copied!=sizeof value)
            return fail(e,"Subscriber Building reference addresspoint read failed");
        return true;
    }
    static bool same(const native::BuildingState& a,const native::BuildingState& b) {
        return a.prop.uid==b.prop.uid&&a.prop.x==b.prop.x&&a.prop.y==b.prop.y&&a.prop.z==b.prop.z&&
            a.type==b.type&&a.variant==b.variant&&a.rotation==b.rotation&&a.scale==b.scale&&
            a.dungeon_membership==b.dungeon_membership&&a.dungeon_instance_membership==b.dungeon_instance_membership&&a.exterior_uid==b.exterior_uid;
    }
    bool fresh(const Pin& pin,std::string& e) const {
        if(!current(e))return false;
        uintptr_t object=0,ap=0;native::BuildingState actual;
        if(!ref::registry_lookup(image_,pin.target.uid,object,e)||!addresspoint(object,ap,e))return false;
        if(object!=pin.target.object||ap!=image_+0x16d93c0)return fail(e,"Subscriber Building reference UID/addresspoint changed");
        if(!native::read_building(pin.target.uid,actual,e))return false;
        return (same(actual,pin.state)||fail(e,"Subscriber Building reference full native readback changed"))&&current(e);
    }
public:
    bool prepare(uintptr_t image,const World& world,std::string& e) {
        pins_.clear();used_.clear();image_=image;world_=world;snapshot_=net::current_snapshot_id();
        // Standalone captures can still describe the original non-Building keys.
        // A Building key requires an admitted shared snapshot and fails in identify.
        if(snapshot_.empty())return true;
        if(!current(e))return false;
        nlohmann::json rows;if(!remove_diagnostics::bindings(rows,e))return false;
        if(rows.at("world_epoch").get<uint64_t>()!=world.world_epoch||rows.at("world_generation").get<uint64_t>()!=world.generation)
            return fail(e,"Subscriber Building reference binding world differs");
        std::set<uintptr_t> objects;std::set<uint64_t> uids;
        for(const auto& row:rows.at("bindings")) {
            if(row.at("revoked").get<bool>()||row.at("unknown").get<bool>())continue;
            const auto key=row.at("record_key").get<std::string>();const auto uid=row.at("local_uid").get<uint64_t>();
            uintptr_t object=0,ap=0;
            if(!ref::registry_lookup(image,uid,object,e)||!addresspoint(object,ap,e))return false;
            if(ap!=image+0x16d93c0)continue;
            Pin pin;
            if(!ref::valid_building_key(key)||row.at("snapshot_id").get<std::string>()!=snapshot_)
                return fail(e,"Subscriber Building reference shared identity invalid");
            if(!remove_diagnostics::resolve_record_target(snapshot_,key,pin.target,e)||!native::read_building(uid,pin.state,e))return false;
            if(pin.target.object!=object||pin.target.uid!=uid||pin.target.world_epoch!=world.world_epoch||
                pin.target.world_generation!=world.generation||!objects.insert(object).second||!uids.insert(uid).second||pins_.contains(key))
                return fail(e,"Subscriber Building reference binding is not unique/current");
            if(!fresh(pin,e))return false;
            pins_.emplace(key,std::move(pin));
        }
        return current(e);
    }
    bool identify(uintptr_t object,std::string& key,std::string& e) override {
        key.clear();
        for(const auto& [name,pin]:pins_)if(pin.target.object==object){
            if(!fresh(pin,e))return false;key=name;used_.insert(name);return true;
        }
        return fail(e,"Subscriber Building reference has no current shared binding");
    }
    bool resolve(const std::string& key,uintptr_t& object,std::string& e) override {
        object=0;const auto found=pins_.find(key);
        if(found==pins_.end())return fail(e,"Subscriber Building reference shared key absent or retired");
        if(!fresh(found->second,e))return false;
        object=found->second.target.object;used_.insert(key);return true;
    }
    bool verify_used(std::string& e) const {
        for(const auto& key:used_)if(!fresh(pins_.at(key),e))return false;
        return true;
    }
};
}
