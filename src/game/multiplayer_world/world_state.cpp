#include "world_state.h"
#include "../snapshot_building_key.h"
#include "../scenery_scene_key.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <set>
#include <type_traits>

namespace mt2mp::world_state {
namespace {
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
Admission reject(std::string& error,const std::string& cause){error=cause;return Admission::Rejected;}
bool uuid(const std::string& value){
    if(value.size()!=36)return false;
    for(size_t i=0;i<value.size();++i){
        if(i==8||i==13||i==18||i==23){if(value[i]!='-')return false;}
        else if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')))return false;
    }
    return true;
}
bool sha(const std::string& value){return value.size()==64&&value.find_first_not_of("0123456789abcdef")==std::string::npos;}
bool origin_valid(const Origin& value){return uuid(value.snapshot_uuid)&&sha(value.sha256)&&value.world_epoch&&value.generation;}
bool state_valid(const Building& value){
    return !value.type.empty()&&!value.variant.empty()&&std::isfinite(value.x)&&std::isfinite(value.y)&&std::isfinite(value.z)&&
        std::all_of(value.rotation.begin(),value.rotation.end(),[](float v){return std::isfinite(v);})&&
        std::all_of(value.scale.begin(),value.scale.end(),[](float v){return std::isfinite(v);});
}
bool record(const std::string& value){
    return game::snapshot_building_key::valid(value)||game::scenery_scene_key::root_saved(value);
}
bool snapshot_rows(const Origin& origin,const std::vector<SnapshotObject>& objects,std::map<std::string,Row>& rows,std::string& error){
    std::set<std::string> record_keys;
    for(const auto& object:objects){
        if(!valid_object_key(object.object_key)||!record(object.record_key)||object.record_key.substr(0,64)!=origin.sha256||
           !object.state.alive||!state_valid(object.state))return fail(error,"world_snapshot_object_id_record_or_state_invalid");
        if(!record_keys.insert(object.record_key).second||!rows.emplace(object.object_key,Row{object.state,object.record_key}).second)
            return fail(error,"world_snapshot_duplicate_object_or_record_key");
    }
    return true;
}
}
bool valid_object_key(const std::string& value){return record(value)||game::scenery_scene_key::saved(value)||(value.starts_with("action:")&&uuid(value.substr(7)));}
bool Table::bind_snapshot(const Origin& origin,const std::vector<SnapshotObject>& objects,uint64_t revision,std::string& error){
    if(origin_.world_epoch)return fail(error,"world_table_already_bound_use_snapshot_rebind");
    if(!origin_valid(origin))return fail(error,"world_snapshot_origin_invalid");
    std::map<std::string,Row> rows;
    if(!snapshot_rows(origin,objects,rows,error))return false;
    origin_=origin;rows_=std::move(rows);revision_=revision;error.clear();return true;
}
bool Table::rebind_snapshot(const Origin& origin,const std::vector<SnapshotObject>& objects,std::string& error){
    if(!origin_.world_epoch||!origin_valid(origin))return fail(error,"world_rebind_requires_bound_table_and_valid_origin");
    if(origin.generation<=origin_.generation)return fail(error,"world_rebind_generation_not_new actual="+std::to_string(origin.generation)+" previous="+std::to_string(origin_.generation));
    std::map<std::string,Row> fresh;
    if(!snapshot_rows(origin,objects,fresh,error))return false;
    const auto alive=std::count_if(rows_.begin(),rows_.end(),[](const auto& entry){return entry.second.state.alive;});
    if(fresh.size()!=static_cast<size_t>(alive))return fail(error,"world_rebind_alive_coverage_differs actual="+std::to_string(fresh.size())+" required="+std::to_string(alive));
    auto rebound=rows_;
    for(const auto& [key,row]:fresh){
        const auto old=rows_.find(key);
        if(old==rows_.end()||!old->second.state.alive||old->second.state!=row.state)return fail(error,"world_rebind_shared_id_or_full_state_differs key="+key);
        rebound.at(key).snapshot_record_key=row.snapshot_record_key;
    }
    origin_=origin;rows_=std::move(rebound);last_.reset();error.clear();return true;
}
Admission Table::prepare(const Origin& origin,const Change& change,std::string& error) const {
    if(!origin_.world_epoch||origin!=origin_)return reject(error,"world_change_current_origin_differs");
    if(!uuid(change.action_uuid)||!valid_object_key(change.object_key)||!change.revision)return reject(error,"world_change_identity_invalid");
    if(change.revision<=revision_){
        if(last_&&*last_==change){error.clear();return Admission::Duplicate;}
        return reject(error,"world_change_stale_or_conflicting_revision actual="+std::to_string(change.revision)+" confirmed="+std::to_string(revision_));
    }
    const auto found=rows_.find(change.object_key);
    if(change.kind==Kind::Create){
        if(change.object_key!="action:"+change.action_uuid||found!=rows_.end()||change.before!=Building{}||!change.after.alive||!state_valid(change.after))
            return reject(error,"world_create_id_or_full_state_invalid");
    }else if(change.kind==Kind::Move||change.kind==Kind::Remove){
        if(found==rows_.end()||!found->second.state.alive||change.before!=found->second.state)
            return reject(error,"world_change_existing_before_state_differs key="+change.object_key);
        if(!state_valid(change.after)||change.after.type!=change.before.type||change.after.variant!=change.before.variant)
            return reject(error,"world_change_type_or_variant_differs");
        if(change.kind==Kind::Move&&!change.after.alive)return reject(error,"world_move_after_not_alive");
        auto retired=change.before;retired.alive=false;
        if(change.kind==Kind::Remove&&change.after!=retired)return reject(error,"world_remove_after_not_exact_tombstone");
    }else return reject(error,"world_change_kind_invalid");
    error.clear();return Admission::New;
}
Admission Table::commit(const Origin& origin,const Change& change,const LocalBinding& observed,std::string& error){
    const auto admission=prepare(origin,change,error);
    if(admission!=Admission::New)return admission;
    if(observed.origin!=origin_||observed.object_key!=change.object_key||!observed.local_uid||observed.state!=change.after)
        return reject(error,"world_confirmed_native_binding_or_full_readback_differs");
    if(change.kind==Kind::Create)rows_.emplace(change.object_key,Row{change.after,{}});
    else rows_.at(change.object_key).state=change.after;
    revision_=change.revision;last_=change;error.clear();return Admission::New;
}
Admission Table::commit_moves(const Origin& origin,const std::vector<Change>& changes,const std::vector<LocalBinding>& observed,std::string& error){
    if(changes.size()<2||changes.size()!=observed.size())return reject(error,"world_move_batch_complete_count_differs");
    std::set<std::string> keys;std::set<uint64_t> uids;
    const auto revision=changes.front().revision;const auto& action=changes.front().action_uuid;
    for(size_t i=0;i<changes.size();++i){
        const auto& change=changes[i];const auto& actual=observed[i];
        if(change.kind!=Kind::Move||change.revision!=revision||change.action_uuid!=action||!keys.insert(change.object_key).second)
            return reject(error,"world_move_batch_action_revision_or_unique_key_differs index="+std::to_string(i));
        if(prepare(origin,change,error)!=Admission::New)return Admission::Rejected;
        if(actual.origin!=origin_||actual.object_key!=change.object_key||!actual.local_uid||!uids.insert(actual.local_uid).second||actual.state!=change.after)
            return reject(error,"world_move_batch_actual_full_binding_or_pose_differs index="+std::to_string(i));
    }
    // Finish every allocation and validation before changing any shared row.
    std::vector<Building> replacements;replacements.reserve(changes.size());
    for(const auto& change:changes)replacements.push_back(change.after);
    static_assert(std::is_nothrow_move_assignable_v<Building>);
    for(size_t i=0;i<changes.size();++i)rows_.at(changes[i].object_key).state=std::move(replacements[i]);
    revision_=revision;last_.reset();error.clear();return Admission::New;
}
Admission Table::commit_creates(const Origin& origin,uint64_t revision,const std::string& action,
                               const std::vector<LocalBinding>& observed,std::string& error){
    if(!origin_.world_epoch||origin!=origin_||!uuid(action)||revision<=revision_||observed.empty())
        return reject(error,"world_create_batch_current_origin_action_revision_or_count_invalid");
    std::set<std::string> keys;std::set<uint64_t> uids;
    for(size_t i=0;i<observed.size();++i){const auto& actual=observed[i];
        if(actual.origin!=origin_||!actual.object_key.starts_with("action:")||!valid_object_key(actual.object_key)||
           !keys.insert(actual.object_key).second||rows_.contains(actual.object_key)||!actual.local_uid||
           !uids.insert(actual.local_uid).second||!actual.state.alive||!state_valid(actual.state))
            return reject(error,"world_create_batch_complete_new_key_full_binding_or_state_differs index="+std::to_string(i));
    }
    // Copy first so allocation failure cannot expose a partially inserted set.
    // Child UUIDs belong to the one validated request, not separate revisions.
    auto next=rows_;
    for(const auto& actual:observed)next.emplace(actual.object_key,Row{actual.state,{}});
    rows_.swap(next);revision_=revision;last_.reset();error.clear();return Admission::New;
}
Admission Table::commit_dungeon_census(const Origin& origin,uint64_t revision,const std::string& action,
    const std::string& target,const std::vector<LocalBinding>& before,const std::vector<LocalBinding>& after,std::string& error){
    if(!origin_.world_epoch||origin!=origin_||!uuid(action)||revision<=revision_||!valid_object_key(target))
        return reject(error,"world_dungeon_census_current_origin_action_revision_or_target_invalid");
    std::map<std::string,const LocalBinding*> old,current;
    auto collect=[&](const std::vector<LocalBinding>& values,auto& result){
        std::set<uint64_t> uids;
        for(const auto& value:values)
            if(value.origin!=origin_||!valid_object_key(value.object_key)||!value.local_uid||
               !uids.insert(value.local_uid).second||!value.state.alive||!state_valid(value.state)||
               !result.emplace(value.object_key,&value).second)return false;
        return true;
    };
    if(!collect(before,old)||!collect(after,current)||(!old.contains(target)&&!current.contains(target)))
        return reject(error,"world_dungeon_census_complete_binding_or_target_missing");
    for(const auto& [key,value]:old){
        const auto known=rows_.find(key);
        if(known!=rows_.end()&&known->second.state!=value->state)
            return reject(error,"world_dungeon_census_before_full_state_differs key="+key);
        const auto survivor=current.find(key);
        if(survivor!=current.end()&&(survivor->second->local_uid!=value->local_uid||
           (key!=target&&survivor->second->state!=value->state)))
            return reject(error,"world_dungeon_census_surviving_identity_or_unrelated_state_differs key="+key);
    }
    for(const auto& [key,value]:current)if(!old.contains(key)){
        const auto known=rows_.find(key);
        if(known!=rows_.end()){
            auto expected=known->second.state;expected.alive=true;
            if(known->second.state.alive||expected!=value->state)
                return reject(error,"world_dungeon_census_restored_tombstone_full_state_differs key="+key);
        }else if(!key.starts_with("action:"))
            return reject(error,"world_dungeon_census_birth_requires_original_action_alias key="+key);
    }
    // SceneClone has admitted the complete original scene before this one
    // publication. Existing natural clone rows can first enter the table here.
    auto next=rows_;
    for(const auto& [key,value]:old){
        next.try_emplace(key,Row{value->state,{}});
        if(!current.contains(key))next.at(key).state.alive=false;
    }
    for(const auto& [key,value]:current){
        auto [found,inserted]=next.try_emplace(key,Row{value->state,{}});
        if(!inserted)found->second.state=value->state;
    }
    rows_.swap(next);revision_=revision;last_.reset();error.clear();return Admission::New;
}
bool Table::refresh_current(const Origin& origin,const std::string& key,const LocalBinding& actual,std::string& error){
    if(!origin_.world_epoch||origin!=origin_)return fail(error,"world_height_refresh_current_origin_differs");
    const auto found=rows_.find(key);
    if(found==rows_.end()||!found->second.state.alive)return fail(error,"world_height_refresh_id_missing_or_removed key="+key);
    if(actual.origin!=origin_||actual.object_key!=key||!actual.local_uid)return fail(error,"world_height_refresh_projected_native_binding_differs");
    const auto& expected=found->second.state;
    const auto& observed=actual.state;
    if(!observed.alive)return fail(error,"world_height_refresh_native_state_not_alive");
    if(observed.type!=expected.type)return fail(error,"world_height_refresh_type_differs actual="+observed.type+" required="+expected.type);
    if(observed.variant!=expected.variant)return fail(error,"world_height_refresh_variant_differs actual="+observed.variant+" required="+expected.variant);
    if(observed.x!=expected.x)return fail(error,"world_height_refresh_x_differs actual="+std::to_string(observed.x)+" required="+std::to_string(expected.x));
    if(observed.z!=expected.z)return fail(error,"world_height_refresh_z_differs actual="+std::to_string(observed.z)+" required="+std::to_string(expected.z));
    if(observed.rotation!=expected.rotation)return fail(error,"world_height_refresh_rotation_differs");
    if(observed.scale!=expected.scale)return fail(error,"world_height_refresh_scale_differs");
    if(!std::isfinite(observed.y))return fail(error,"world_height_refresh_y_not_finite actual="+std::to_string(observed.y)+" required=finite");
    found->second.state.y=observed.y;error.clear();return true;
}
Admission Table::commit_scenery_stroke(const Origin& origin,uint64_t revision,const std::string& action,
                                     const std::vector<std::string>& removed,const std::vector<LocalBinding>& created,std::string& error){
    return commit_scenery_batch(origin,revision,action,removed,created,false,error);
}
Admission Table::commit_scenery_history(const Origin& origin,uint64_t revision,const std::string& action,
                                      const std::vector<std::string>& removed,const std::vector<LocalBinding>& restored,std::string& error){
    return commit_scenery_batch(origin,revision,action,removed,restored,true,error);
}
Admission Table::commit_scenery_batch(const Origin& origin,uint64_t revision,const std::string& action,
                                     const std::vector<std::string>& removed,const std::vector<LocalBinding>& created,bool restore,std::string& error){
    if(!origin_.world_epoch||origin!=origin_||!uuid(action)||revision<=revision_||(removed.empty()&&created.empty()))
        return reject(error,"world_scenery_stroke_current_origin_action_revision_or_count_invalid");
    std::set<std::string> keys;std::set<uint64_t> uids;
    for(const auto& key:removed){const auto found=rows_.find(key);
        if(!keys.insert(key).second||found==rows_.end()||!found->second.state.alive)
            return reject(error,"world_scenery_stroke_removed_key_missing_retired_or_duplicate key="+key);
    }
    for(size_t i=0;i<created.size();++i){const auto& actual=created[i];
        const auto prior=rows_.find(actual.object_key);
        if(actual.origin!=origin_||(!restore&&!actual.object_key.starts_with("action:"))||!valid_object_key(actual.object_key)||
           !keys.insert(actual.object_key).second||!actual.local_uid||!uids.insert(actual.local_uid).second||
           !actual.state.alive||!state_valid(actual.state))
            return reject(error,"world_scenery_stroke_created_full_binding_or_state_differs index="+std::to_string(i));
        if(restore){
            if(prior==rows_.end()||prior->second.state.alive)return reject(error,"world_scenery_history_restore_target_not_tombstone key="+actual.object_key);
            auto expected=prior->second.state;expected.alive=true;
            if(expected!=actual.state)return reject(error,"world_scenery_history_restore_full_native_state_differs key="+actual.object_key);
        }else if(prior!=rows_.end())return reject(error,"world_scenery_stroke_created_key_already_exists key="+actual.object_key);
    }
    auto next=rows_;for(const auto& key:removed)next.at(key).state.alive=false;
    for(const auto& actual:created){if(restore)next.at(actual.object_key).state=actual.state;else next.emplace(actual.object_key,Row{actual.state,{}});}
    rows_.swap(next);revision_=revision;last_.reset();error.clear();return Admission::New;
}
bool Table::retire_native_effects(const Origin& origin,uint64_t revision,const std::string& action,
    const std::vector<std::string>& keys,std::string& error) {
    if(!origin_.world_epoch||origin!=origin_||!uuid(action)||revision<=revision_)return fail(error,"world_native_retirement_origin_action_or_revision_invalid");
    std::set<std::string> unique;
    for(const auto& key:keys) {
        const auto row=rows_.find(key);
        if(!unique.insert(key).second||row==rows_.end()||!row->second.state.alive)
            return fail(error,"world_native_retirement_key_missing_removed_or_duplicate key="+key);
    }
    for(const auto& key:keys)rows_.at(key).state.alive=false;
    revision_=revision;last_.reset();error.clear();return true;
}

bool Table::restore_native_building(const Origin& origin,uint64_t revision,const std::string& action,
    const std::string& original,const LocalBinding& actual,std::string& error) {
    if(!uuid(original)||action==original)return fail(error,"world_building_redo_original_action_invalid");
    return restore_native_building_key(origin,revision,action,"action:"+original,actual,error);
}
bool Table::restore_native_building_key(const Origin& origin,uint64_t revision,const std::string& action,
                                    const std::string& key,const LocalBinding& actual,std::string& error) {
    if(!origin_.world_epoch||origin!=origin_||!uuid(action)||!valid_object_key(key)||
       revision<=revision_||actual.origin!=origin_||actual.object_key!=key||!actual.local_uid||!actual.state.alive)
        return fail(error,"world_building_redo_origin_revision_or_binding_invalid");
    const auto row=rows_.find(key);
    if(row==rows_.end()||row->second.state.alive)
        return fail(error,"world_building_redo_target_not_tombstone");
    auto expected=row->second.state;expected.alive=true;
    if(actual.state!=expected)return fail(error,"world_building_redo_full_native_state_differs");
    row->second.state=actual.state;revision_=revision;last_.reset();error.clear();return true;
}
bool Table::resolve_current(const Origin& origin,const std::string& key,RegistryReader& registry,LocalBinding& result,std::string& error) const {
    result={};
    if(origin!=origin_||!origin_.world_epoch)return fail(error,"world_resolve_current_origin_differs");
    const auto found=rows_.find(key);
    if(found==rows_.end()||!found->second.state.alive)return fail(error,"world_resolve_id_missing_or_removed key="+key);
    LocalBinding actual;
    if(!registry.current(key,actual,error))return false;
    if(actual.origin!=origin_||actual.object_key!=key||!actual.local_uid||actual.state!=found->second.state)
        return fail(error,"world_resolve_actual_registry_or_full_readback_differs key="+key);
    result=std::move(actual);error.clear();return true;
}
bool Table::restore_native_buildings(const Origin& origin,uint64_t revision,const std::string& action,
                                    const std::vector<LocalBinding>& observed,std::string& error){
    if(!origin_.world_epoch||origin!=origin_||!uuid(action)||revision<=revision_||observed.empty())
        return fail(error,"world_restore_batch_current_origin_action_revision_or_count_invalid");
    std::set<std::string> keys;std::set<uint64_t> uids;
    for(size_t i=0;i<observed.size();++i){const auto& actual=observed[i];const auto row=rows_.find(actual.object_key);
        if(actual.origin!=origin_||!valid_object_key(actual.object_key)||!keys.insert(actual.object_key).second||
           !actual.local_uid||!uids.insert(actual.local_uid).second||row==rows_.end()||row->second.state.alive)
            return fail(error,"world_restore_batch_exact_tombstone_binding_differs index="+std::to_string(i));
        auto expected=row->second.state;expected.alive=true;
        if(actual.state!=expected||!state_valid(actual.state))
            return fail(error,"world_restore_batch_full_native_state_differs index="+std::to_string(i));
    }
    // The original graph restores the complete set before this publication.
    // Validate all tombstones first and advance its one shared revision once.
    for(const auto& actual:observed)rows_.at(actual.object_key).state.alive=true;
    revision_=revision;last_.reset();error.clear();return true;
}
}
