#include "runtime.h"
#include "../live_host_state/apply_timing.h"
#include "raw.h"
#include "quest_preview.h"
#include "../npc_quest_target_runtime.h"
#include "../direct_monster_identity.h"
#include "../building_scene_identity.h"
#include "../scenery_scene_identity.h"
#include "../building_history_slice/native_bridge.h"
#include "../session_provider.h"
#include "../typed_action_runtime.h"
#include "mt2mp/game/native_engine.h"
#include <Windows.h>
namespace mt2mp::game::scene_clone {
namespace {
namespace gizmo=dungeon_scene_identity_runtime;
namespace gi=dungeon_gizmo_identity;
namespace direct=direct_monster_identity;
namespace buildings=remove_diagnostics::scene_buildings;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}

gi::Member gizmo_member(const Pool& pool,const Member& member){return {pool.scene.owner.key,member.object,pool.managers[3],pool.scene.owner.shard,pool.scene.owner.region,member.uid};}
buildings::Row building_member(const Pool& pool,const Member& member){
    return {member.key,pool.scene.owner.key,member.object,pool.scene.owner.shard,pool.scene.owner.region,member.family==Family::Scenery?pool.managers[0]:pool.building_manager,member.uid,member.family==Family::Scenery};
}
const Pool* pool_named(const Raw& raw,const std::string& name){const auto found=std::find_if(raw.pools.begin(),raw.pools.end(),[&](const auto& pool){return pool.scene.owner.key==name;});return found==raw.pools.end()?nullptr:&*found;}
const Member* assigned(const Pool* pool,const Assignment& assignment){if(!pool)return nullptr;const auto found=std::find_if(pool->members.begin(),pool->members.end(),[&](const auto& member){return member.family==assignment.family&&member.child==assignment.child;});return found==pool->members.end()?nullptr:&*found;}
bool identities(Raw& raw,const direct::Registry& registry,std::string& e,const Raw* before=nullptr){
    for(auto& pool:raw.pools){if(before&&!pool_named(*before,pool.scene.owner.key))continue;for(auto& member:pool.members){
        if(member.family==Family::Gizmo){if(!gizmo::resolve(member.object,member.uid,member.key,e))return false;}
        else if(member.family==Family::MonsterZone||zone_type(member.family)>=0){if(!npc_quest_target_runtime::clone_alias(raw,pool,member,member.key,e))return false;}
        else if(member.family==Family::Building){if(!buildings::resolve(raw.world,member.object,member.uid,member.key,e))return false;}
        else if(member.family==Family::Scenery){if(!remove_diagnostics::scene_scenery::resolve(raw.world,member.object,member.uid,member.key,e))return false;}
        else if(!registry.resolve(direct_scope(raw,pool),member.object,member.uid,member.key,e))return false;
    }}return true;
}
bool verify_direct(const Raw& raw,const direct::Registry& registry,std::string& e){
    std::vector<std::pair<std::string,direct::Entry>> entries;
    if(!registry.enumerate(raw.world.uuid,raw.world.world_epoch,raw.world.generation,uintptr_t(raw.world.map)+0x1c0,entries,e))return false;
    for(const auto& [key,entry]:entries){(void)key;if(entry.retired)continue;
        const auto pool=std::find_if(raw.pools.begin(),raw.pools.end(),[&](const auto& p){return p.managers[2]==entry.scope.manager;});
        if(pool==raw.pools.end())continue;
        if(entry.scope!=direct_scope(raw,*pool)||std::none_of(pool->members.begin(),pool->members.end(),[&](const auto& member){return member.family==Family::DirectMonster&&member.object==entry.pointer&&member.uid==entry.uid;}))return fail(e,"Scene clone surviving direct Monster registry contains a missing/replaced full UID");
    }
    for(const auto& pool:raw.pools)for(const auto& member:pool.members)if(member.family==Family::DirectMonster){std::string key;if(!registry.resolve(direct_scope(raw,pool),member.object,member.uid,key,e))return false;}
    return true;
}
bool same_raw(const Raw& a,const Raw& b){
    if(!same_world(a.world,b.world)||a.pools.size()!=b.pools.size())return false;
    for(size_t n=0;n<a.pools.size();++n){const auto& x=a.pools[n];const auto& y=b.pools[n];if(!same_owner(x,y)||x.members.size()!=y.members.size()||x.zones!=y.zones||x.scenery!=y.scenery)return false;
        for(size_t i=0;i<x.members.size();++i){const auto& u=x.members[i];const auto& v=y.members[i];if(u.family!=v.family||u.child!=v.child||u.object!=v.object||u.uid!=v.uid)return false;}}
    return true;
}
}
struct Runtime::State {
    action_session::Context context;action_session::Binding binding;DWORD thread=GetCurrentThreadId();
    struct TemplateHistory {action_session::Action kind;std::string exterior;Raw before,after;uintptr_t held_node=0;};
    Raw baseline,prepared_raw;Mapping mapping;bool prepared=false,locked=false;std::string cause;std::set<std::string> operations;
    std::map<std::string,TemplateHistory> template_history;
    std::map<std::string,Pool> retired_pools;
};
Runtime::Runtime()=default;
Runtime::~Runtime()=default;
namespace {
bool current(const action_session::Context& context,const action_session::Binding& binding,std::string& e){action_session::Context observed;action_session::Binding actual;
    return action_session::selected_session().current(observed,e)&&observed==context&&typed_action_runtime::read_binding(context.snapshot_uuid,actual,e)&&actual==binding||fail(e,e.empty()?"Scene clone exact current Context/Binding differs":e);
}
}
bool Runtime::bind(const action_session::Context& context,const action_session::Binding& binding,std::string& e){
    if(!current(context,binding,e))return false;
    if(state_){if(state_->locked)return fail(e,state_->cause);if(state_->thread!=GetCurrentThreadId()||state_->context!=context||state_->binding!=binding)return fail(e,"Scene clone changed world binding requires explicit reset");e.clear();return true;}
    auto ready=std::make_unique<State>();ready->context=context;ready->binding=binding;
    gizmo::Census census;if(!read_raw(ready->baseline,e))return fail(e,e.empty()?"Scene clone initial confirmed raw world/registry differs":e);
    if(ready->baseline.world.uuid.empty()||ready->baseline.world.world_epoch!=binding.native_epoch||ready->baseline.world.generation!=binding.native_generation)
        return fail(e,"Scene clone initial native binding differs: session_snapshot="+context.snapshot_uuid+" native_snapshot="+ready->baseline.world.uuid+
            " native_epoch="+std::to_string(ready->baseline.world.world_epoch)+" binding_epoch="+std::to_string(binding.native_epoch)+
            " native_generation="+std::to_string(ready->baseline.world.generation)+" binding_generation="+std::to_string(binding.native_generation));
    if(!gizmo::read_current(census,e)||!same_world(census.world,ready->baseline.world)||!verify_direct(ready->baseline,direct::registry,e)||!identities(ready->baseline,direct::registry,e))return fail(e,e.empty()?"Scene clone initial confirmed raw world/registry differs":e);
    state_=std::move(ready);e.clear();return true;
}
bool Runtime::prepare(const action_session::Context& context,const action_session::Binding& binding,std::string& e){
    if(!state_||state_->locked||state_->thread!=GetCurrentThreadId()||state_->context!=context||state_->binding!=binding||!current(context,binding,e))return fail(e,e.empty()?"Scene clone preparation lacks a healthy bound Mainthread":e);
    // Natural Host births may already exist. Keep the last confirmed baseline.
    Raw actual;if(!read_raw(actual,e)||!same_world(actual.world,state_->baseline.world))return fail(e,e.empty()?"Scene clone preparation world changed":e);
    state_->prepared_raw=std::move(actual);state_->prepared=true;e.clear();return true;
}
bool Runtime::confirm_impl(const action_session::Context& context,const action_session::Binding& binding,const std::string& operation,
    const Mapping* authority,Mapping& out,gizmo::Census& admitted,bool& attempted,std::string& e,const std::string& template_exterior,const Raw* restoration,uintptr_t held_node){
    out={};admitted={};attempted=false;
    if(!state_||!state_->prepared||state_->locked||state_->thread!=GetCurrentThreadId()||state_->context!=context||state_->binding!=binding||!current(context,binding,e)||(authority!=nullptr)!=(context.role==action_session::Role::Client)||state_->operations.contains(operation))return fail(e,e.empty()?"Scene clone confirmation token/current bound preparation differs":e);
    Raw actual;Mapping mapping;Changes changes;
    if(![&](){std::optional<live_host_state::apply_timing::Scope> measured;if(live_host_state::apply_timing::client_party_prepare_depth)measured.emplace(live_host_state::apply_timing::Part::ClientPartySceneRawActual);return read_raw(actual,e);}()||!identities(actual,direct::registry,e,&state_->baseline)||!plan(state_->baseline,actual,operation,state_->mapping,authority,mapping,changes,e,template_exterior,restoration))return false;
    Raw held;const Raw* retained=nullptr;
    const bool departed_template=std::any_of(state_->baseline.pools.begin(),state_->baseline.pools.end(),[&](const auto& pool){
        return pool.scene.owner.type==1&&!pool_named(actual,pool.scene.owner.key);
    });
    if(departed_template){
        if(!held_node||template_exterior.empty()||
           !read_held_template(state_->baseline,template_exterior,held_node,held,e))return fail(e,e.empty()?"Scene clone departed template lacks its original inverse owner":e);
        retained=&held;
    }else if(held_node)return fail(e,"Scene clone supplied an inverse Dungeon owner without a departed template");
    Mapping departed;departed.retired=changes.retired;
    if(!retirements_ready(state_->baseline,departed,find_uid,e,retained))return false;
    std::vector<std::pair<std::string,gi::Member>> gizmo_births,gizmo_retired;
    auto direct_before=direct::registry,direct_after=direct_before;
    std::vector<buildings::Row> building_births,building_retired;
    std::vector<buildings::Restore> building_restored;
    for(const auto& row:changes.retired){const auto* pool=pool_named(state_->baseline,row.scene);const auto* member=assigned(pool,row);uintptr_t found{};
        if(!member||member->key!=row.key||!find_uid(member->uid,found,e)||
           (held_member(retained,row.scene,member->object,member->uid)?found!=member->object:found!=0))
            return fail(e,e.empty()?"Scene clone original member retirement is incomplete":e);
        if(row.family==Family::Gizmo)gizmo_retired.emplace_back(row.key,gizmo_member(*pool,*member));
        if(row.family==Family::Building||row.family==Family::Scenery)building_retired.push_back(building_member(*pool,*member));
        if(row.family==Family::DirectMonster){direct::Entry entry;if(!direct_after.lookup(monster_target_key::argument(row.key),direct_scope(state_->baseline,*pool),entry,e)||entry.pointer!=member->object||entry.uid!=member->uid||!direct_after.retire(monster_target_key::argument(row.key),entry,e))return fail(e,e.empty()?"Scene clone direct Monster retirement exact UID differs":e);}
    }
    for(const auto& row:changes.births){const auto* pool=pool_named(actual,row.scene);const auto* member=assigned(pool,row);if(!member)return fail(e,"Scene clone birth raw scene/member absent");
        if(row.family==Family::Gizmo)gizmo_births.emplace_back(row.key,gizmo_member(*pool,*member));
        if(row.family==Family::Building||row.family==Family::Scenery){auto actual_row=building_member(*pool,*member);actual_row.key=row.key;building_births.push_back(std::move(actual_row));}
        if(row.family==Family::DirectMonster){const auto id=monster_target_key::argument(row.key);if(!direct_after.stage(id,direct_scope(actual,*pool),member->object,member->uid,e)||!direct_after.confirm(id,e))return false;}
    }
    std::vector<gizmo::GizmoRestore> gizmo_restored;
    for(const auto& row:changes.restored){
        const auto old=state_->retired_pools.find(row.scene);const auto* pool=pool_named(actual,row.scene);
        const auto* member=assigned(pool,row);const auto* prior=old==state_->retired_pools.end()?nullptr:assigned(&old->second,row);
        if(!member||!prior||prior->key!=row.key)return fail(e,"Scene clone original restoration lacks its exact latest retired member");
        if(row.family==Family::Gizmo)gizmo_restored.push_back({row.key,gizmo_member(old->second,*prior),gizmo_member(*pool,*member)});
        if(row.family==Family::Building||row.family==Family::Scenery){auto fresh=building_member(*pool,*member);fresh.key=row.key;
            building_restored.push_back({building_member(old->second,*prior),std::move(fresh)});}
        if(row.family==Family::DirectMonster){
            const direct::Entry expected{direct_scope(actual,old->second),prior->object,prior->uid,true,true};
            const direct::Entry fresh{direct_scope(actual,*pool),member->object,member->uid,true,false};
            if(!direct_after.restore_scene(monster_target_key::argument(row.key),expected,fresh,template_exterior,e))return false;
        }
    }
    gizmo::ClonePreview gizmos;npc_quest_target_runtime::ClonePreview zones;buildings::Preview building_preview;
    if(![&](){std::optional<live_host_state::apply_timing::Scope> measured;if(live_host_state::apply_timing::client_party_prepare_depth)measured.emplace(live_host_state::apply_timing::Part::ClientPartySceneGizmoPreview);return gizmo::preview_clones(actual.world,gizmo_births,gizmo_retired,gizmos,admitted,e,gizmo_restored,template_exterior,retained);}()||![&](){std::optional<live_host_state::apply_timing::Scope> measured;if(live_host_state::apply_timing::client_party_prepare_depth)measured.emplace(live_host_state::apply_timing::Part::ClientPartySceneZonePreview);return npc_quest_target_runtime::preview_clones(actual,mapping,zones,e,template_exterior,restoration,retained);}()||!verify_direct(actual,direct_after,e)||![&](){std::optional<live_host_state::apply_timing::Scope> measured;if(live_host_state::apply_timing::client_party_prepare_depth)measured.emplace(live_host_state::apply_timing::Part::ClientPartySceneBuildingPreview);return buildings::preview(actual.world,building_births,building_retired,building_restored,building_preview,e,retained);}())return false;
    Raw final;if(![&](){std::optional<live_host_state::apply_timing::Scope> measured;if(live_host_state::apply_timing::client_party_prepare_depth)measured.emplace(live_host_state::apply_timing::Part::ClientPartySceneRawFinal);return read_raw(final,e);}()||!same_raw(actual,final)||!current(context,binding,e)||!gizmo::check_clones(gizmos,e)||!npc_quest_target_runtime::check_clones(zones,e)||![&](){std::optional<live_host_state::apply_timing::Scope> measured;if(live_host_state::apply_timing::client_party_prepare_depth)measured.emplace(live_host_state::apply_timing::Part::ClientPartySceneBuildingCheck);return buildings::check(building_preview,e);}()||direct::registry!=direct_before)return fail(e,e.empty()?"Scene clone complete native preview changed before publication":e);
    if(retained){Raw final_held;if(!read_held_template(state_->baseline,template_exterior,held_node,final_held,e)||
        !same_raw(held,final_held))return fail(e,e.empty()?"Scene clone inverse-owned template changed before publication":e);}
    const auto unknown=[&](){state_->locked=true;state_->cause="Scene clone registry publication incomplete: "+e;e=state_->cause;return false;};
    attempted=true;
    if(!gizmo::publish_clones(gizmos,e)||!npc_quest_target_runtime::publish_clones(zones,e)||![&](){std::optional<live_host_state::apply_timing::Scope> measured;if(live_host_state::apply_timing::client_party_prepare_depth)measured.emplace(live_host_state::apply_timing::Part::ClientPartySceneBuildingPublish);return buildings::publish(building_preview,e);}())return unknown();
    direct::registry=std::move(direct_after);
    if(!identities(final,direct::registry,e)||![&](){std::optional<live_host_state::apply_timing::Scope> measured;if(live_host_state::apply_timing::client_party_prepare_depth)measured.emplace(live_host_state::apply_timing::Part::ClientPartySceneGizmoFinal);return gizmo::read_current(admitted,e);}())return unknown();
    for(auto& pool:final.pools)for(auto& member:pool.members){const auto row=std::find_if(mapping.members.begin(),mapping.members.end(),[&](const auto& value){return value.scene==pool.scene.owner.key&&value.family==member.family&&value.child==member.child;});
        if(row==mapping.members.end()||row->key!=member.key){e="Scene clone published alias differs from complete mapping";return unknown();}member.origin=row->origin;member.ordinal=row->ordinal;}
    for(const auto& pool:state_->baseline.pools)if(!pool_named(final,pool.scene.owner.key))state_->retired_pools[pool.scene.owner.key]=pool;
    for(const auto& row:changes.restored)state_->retired_pools.erase(row.scene);
    state_->mapping=mapping;state_->baseline=std::move(final);state_->prepared=false;state_->operations.insert(operation);out=std::move(mapping);e.clear();return true;
}
bool Runtime::confirm(const action_session::Context& context,const action_session::Binding& binding,
    const std::string& operation,const Mapping* authority,Mapping& out,gizmo::Census& admitted,
    bool& attempted,std::string& e){
    return confirm_impl(context,binding,operation,authority,out,admitted,attempted,e,{},nullptr,0);
}
bool Runtime::confirm_template(const action_session::Context& context,const action_session::Binding& binding,
    const action_session::Request& request,const Mapping* authority,Mapping& out,gizmo::Census& admitted,
    bool& attempted,std::string& e,uintptr_t held_node){
    out={};admitted={};attempted=false;
    using A=action_session::Action;
    const auto controller=typed_action_runtime::controller_status();
    const bool place=request.action==A::PlaceBuilding;
        const bool move=request.action==A::MoveBuilding;
    const bool other=request.action==A::RemoveSnapshotBuilding||request.action==A::UndoBuilding||request.action==A::RedoBuilding;
    if(!request.dungeon_building||(!place&&!move&&!other)||request.snapshot_uuid!=context.snapshot_uuid||
       !dungeon_scene_key::building_key(request.canonical_record_key)||
       !controller.value("started",false)||controller.value("locked",true)||!controller.value("pending",false)||
       controller.value("action_uuid",std::string{})!=request.action_uuid)
        return fail(e,"Scene clone template confirmation requires exact pending Dungeon Building request");
    if(!state_||!state_->prepared)return fail(e,"Scene clone template phase was not prepared");
    const bool history=request.action==A::UndoBuilding||request.action==A::RedoBuilding;
    if(!action_session::valid_dungeon_building_scene(request.dungeon_scene))
        return fail(e,"Scene clone Dungeon Building scene family is invalid");
    if(request.dungeon_scene!="world"){
        if(held_node)return fail(e,"Contained Building action cannot own a detached Dungeon");
        return confirm_impl(context,binding,request.action_uuid,authority,out,admitted,attempted,e,{},nullptr,0);
    }
    const Raw* restoration=nullptr;
    if(history){
        const auto source=state_->template_history.find(request.target_action_uuid);
        if(source==state_->template_history.end()||source->second.exterior!=request.canonical_record_key)
            return fail(e,"Scene clone template history lacks its confirmed source action");
        const bool source_remove=source->second.kind==A::RemoveSnapshotBuilding;
        const bool source_move=source->second.kind==A::MoveBuilding;
        const bool restoring=!source_move&&((request.action==A::UndoBuilding)==source_remove);
        if(restoring)restoration=source_remove?&source->second.before:&source->second.after;
    }else if(!request.target_action_uuid.empty()||state_->template_history.contains(request.action_uuid))
        return fail(e,"Scene clone template source action was already retained or has a foreign history target");
    const auto before=state_->baseline;
    if(!confirm_impl(context,binding,request.action_uuid,authority,out,admitted,attempted,e,request.canonical_record_key,restoration,held_node))return false;
    if(!history)state_->template_history.emplace(request.action_uuid,State::TemplateHistory{request.action,request.canonical_record_key,before,state_->baseline,held_node});
    else state_->template_history.at(request.target_action_uuid).held_node=held_node;
    return true;
}
bool Runtime::resolve_retained_destination(uintptr_t pointer,std::string& key,bool& matched,std::string& e,std::string* scene){
    if(scene)scene->clear();
    key.clear();matched=false;e.clear();if(!state_)return true;
    const State::TemplateHistory* source=nullptr;const Member* expected=nullptr;std::string source_id;
    for(auto& [id,history]:state_->template_history){
        if(!history.held_node)continue;
        const auto& raw=history.kind==action_session::Action::RemoveSnapshotBuilding?history.before:history.after;
        for(const auto& pool:raw.pools)if(pool.scene.owner.exterior_key==history.exterior)
            for(const auto& member:pool.members)if(member.object==pointer&&
                (member.family==Family::Building||member.family==Family::MonsterZone||member.family==Family::DirectMonster)){
                matched=true;
                bool present=false;
                const auto direction=history.kind==action_session::Action::RemoveSnapshotBuilding?
                    action_session::Action::UndoBuilding:action_session::Action::RedoBuilding;
                if(!building_history_slice::retained_dungeon_node(direction,id,history.held_node,present,e))return false;
                if(!present){history.held_node=0;matched=source!=nullptr;continue;}
                if(source)return fail(e,"Held Dungeon destination belongs to multiple retained original graphs");
                source=&history;expected=&member;source_id=id;
            }
    }
    if(!matched)return true;
    if(state_->locked||state_->thread!=GetCurrentThreadId()||!current(state_->context,state_->binding,e))
        return fail(e,e.empty()?"Held Dungeon destination has no healthy current Mainthread binding":e);
    const auto& before=source->kind==action_session::Action::RemoveSnapshotBuilding?source->before:source->after;
    Raw held;
    if(!read_held_template(before,source->exterior,source->held_node,held,e))return false;
    for(const auto& pool:held.pools)for(const auto& member:pool.members)
        if(member.object==pointer&&member.uid==expected->uid&&member.key==expected->key&&!member.key.empty()){
            bool present=false;
            const auto direction=source->kind==action_session::Action::RemoveSnapshotBuilding?
                action_session::Action::UndoBuilding:action_session::Action::RedoBuilding;
            if(!building_history_slice::retained_dungeon_node(direction,source_id,source->held_node,present,e))return false;
            if(!present)return fail(e,"Held Dungeon destination original graph changed during owner read");
            key=member.key;if(scene)*scene=pool.scene.owner.key;e.clear();return true;
        }
    return fail(e,"Held Dungeon destination lost its exact original canonical member");
}
bool Runtime::cancel_prepare(const action_session::Context& context,const action_session::Binding& binding,std::string& e){
    if(!state_||!state_->prepared||state_->locked||state_->thread!=GetCurrentThreadId()||
       state_->context!=context||state_->binding!=binding||!current(context,binding,e))
        return fail(e,e.empty()?"Scene clone cancellation lacks its exact healthy prepared world":e);
    Raw actual;
    if(!read_raw(actual,e)||!same_raw(state_->prepared_raw,actual)){
        state_->locked=true;state_->cause=e.empty()?"Scene clone rejected operation changed the prepared raw roster":e;
        return fail(e,state_->cause);
    }
    state_->prepared=false;state_->prepared_raw={};e.clear();return true;
}
void Runtime::reset(){state_.reset();}
}
