#include "building_clearance_graph.h"
#include "scenery_alias_order.h"
#include "scenery_stroke_history.h"
#include "multiplayer_world/world_state.h"
#include <atomic>
#include <memory>
namespace sample_fixture {
namespace npc_quest_targets=mt2mp::game::npc_quest_targets;
namespace duplicate_edit=::sample_fixture;
#include "duplicate_sample_binding_actual.inc"
namespace scenery_make_history=mt2mp::game::scenery_make_history;
namespace scenery_stroke_history=mt2mp::game::scenery_stroke_history;
namespace scenery_alias_order=mt2mp::game::scenery_alias_order;
namespace scenery_stroke_state=mt2mp::game::scenery_stroke_state;
namespace building_clearance_graph=mt2mp::game::building_clearance_graph;
uint32_t GetCurrentThreadId(){return 7;}
std::atomic<Runtime*> published{nullptr};
uintptr_t clearance_graph=0;
std::vector<std::string> clearance_events;
std::string clearance_failure;
namespace scenery_stroke {
bool digest(const std::string& text,std::string& out,std::string&){out=mt2mp::sha256(std::vector<unsigned char>(text.begin(),text.end()));return out.size()==64;}
}
namespace witness {
struct GraphWitness {bool locked=true;int32_t pending_delete=0;};
bool read_graph(int&,uintptr_t image,uintptr_t owner,GraphWitness& out,std::string& error){
    if(image!=f::image||!owner||owner!=clearance_graph){error="fixture exact closed graph owner differs";return false;}
    out={};if(clearance_failure=="graph"){error="original graph read failed";return false;}return true;
}
}
namespace building_history_slice {
bool graph_for(action::Action direction,const std::string& original,uintptr_t& out,std::string& error){
    if((direction!=action::Action::UndoBuilding&&direction!=action::Action::RedoBuilding)||original.empty()||!clearance_graph){error="fixture history graph binding differs";return false;}
    out=clearance_graph;return true;
}
}
namespace scenery_kiss {
struct NativeCatalog {
    NativeCatalog(int&,uintptr_t image,uint32_t thread){f::need(image==f::image&&thread==7,"Native catalogue owner differs");}
    bool classify_directory(const std::string& type,bool& scenery,std::string& error){
        if(type!="SmallInn"&&type!="cactus"){error="actual catalogue type is absent";return false;}scenery=type=="cactus";return true;
    }
};
}
namespace remove_diagnostics {
struct LocalTarget {uintptr_t object=0;uint64_t uid=0;std::string record_key;bool normal_scenery=false;};
std::vector<LocalTarget> saved_extra;
bool identity_targets(std::vector<LocalTarget>& out,std::string& error){
    if(clearance_failure=="metadata"){error="original saved Root metadata proof failed";return false;}
    for(const auto& target:saved_extra){const auto found=uid_objects.find(target.uid);
        if(found==uid_objects.end()||found->second!=target.object){error="fixture saved Root full UID differs";return false;}}
    out=saved_extra;return true;
}
bool retire_region_scenery(const std::string& snapshot,const std::string& id,const std::vector<uint64_t>& uids,std::vector<std::string>& keys,std::string& error){
    clearance_events.push_back("retire");keys.clear();
    if(snapshot!=net::current_snapshot_id()||id.empty()){error="fixture retirement identity differs";return false;}
    if(clearance_failure=="retire"){error="original absence proof failed";return false;}
    for(auto uid:uids){if(uid_objects.contains(uid)){error="fixture retirement full UID still live";return false;}
        auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.at("local_uid").template get<uint64_t>()==uid;});
        if(found==rows.end()||found->at("revoked").template get<bool>()){error="fixture retirement canonical row missing or retired";return false;}
        (*found)["revoked"]=true;keys.push_back(found->at("record_key").template get<std::string>());
    }
    std::sort(keys.begin(),keys.end());return true;
}
bool restore_redone_building(const std::string& snapshot,const std::string& key,uint64_t uid,std::string& error){
    clearance_events.push_back("restore");
    if(snapshot!=net::current_snapshot_id()||!uid_objects.contains(uid)){error="fixture restored full UID not live";return false;}
    auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.at("local_uid").template get<uint64_t>()==uid&&row.at("record_key").template get<std::string>()==key;});
    if(found==rows.end()||!found->at("revoked").template get<bool>()){error="fixture restore exact tombstone differs";return false;}
    if(clearance_failure=="restore"){error="original restoration metadata failed";return false;}
    (*found)["revoked"]=false;return true;
}
bool resolve_identity_destination(const std::string& snapshot,const std::string& key,LocalTarget& out,std::string& error){
    if(snapshot!=net::current_snapshot_id()){error="fixture readback snapshot differs";return false;}
    for(const auto& row:rows)if(row.at("record_key")==key&&!row.at("revoked").template get<bool>()){
        const auto uid=row.at("local_uid").template get<uint64_t>();const auto found=uid_objects.find(uid);
        if(found==uid_objects.end()){error="fixture readback full UID absent";return false;}out={found->second,uid,key,true};return true;}
    error="fixture readback current alias absent";return false;
}
}
namespace world_state {
using Origin=mt2mp::world_state::Origin;using LocalBinding=mt2mp::world_state::LocalBinding;
bool bind_root_readback(const Origin& origin,const std::string& key,const remove_diagnostics::LocalTarget& target,const native::BuildingState& actual,LocalBinding& out,std::string& error){
    if(origin.snapshot_uuid!=net::current_snapshot_id()||target.record_key!=key||!target.uid||actual.prop.uid!=target.uid){error="fixture exact World readback differs";return false;}
    out={origin,key,target.uid,{}};return true;
}
}
namespace building_clearance {struct Local;struct Snapshot{std::shared_ptr<Local> local;};}
#include "building_clearance_runtime.inc"
}
