#include "world_state.h"
#include "../snapshot_building_key.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <set>

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
    return game::snapshot_building_key::valid(value);
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
bool valid_object_key(const std::string& value){return record(value)||(value.starts_with("action:")&&uuid(value.substr(7)));}
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
    const auto key="action:"+original;
    if(!origin_.world_epoch||origin!=origin_||!uuid(action)||!uuid(original)||action==original||
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
}
