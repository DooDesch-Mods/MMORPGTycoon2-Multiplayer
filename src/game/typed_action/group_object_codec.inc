bool group_object_action(Action value){return value==Action::MoveGroup||value==Action::RemoveGroup;}
bool group_object_history(const Request& r){return (r.action==Action::UndoBuilding||r.action==Action::RedoBuilding)&&
    uuid(r.target_action_uuid)&&r.canonical_record_key=="group-object:"+r.target_action_uuid;}
bool group_object_intent_valid(bool moving,const GroupObjectIntent& v){return game::group_object_transition::valid(moving,v,[](const auto& value){return key(value);},max_payload);}
std::string group_object_target(const GroupObjectState& v){std::string target;
    for(const auto& row:v.records.sources)if(row.family==DuplicateFamily::Group){if(!target.empty())return {};target=row.key;}return target;
}
bool group_object_state_valid(const GroupObjectState& v){
    if(!group_state_valid(v.ownership)||!v.records.created.empty()||v.records.history||v.records.random!=v.ownership.random||
       !game::duplicate_transition::sha(v.records.untouched_sha)||v.records.sources.size()>max_payload)return false;
    const auto target=group_object_target(v);if(target.empty())return v.records.sources.empty();
    return game::group_object_transition::valid(v,target,[](const auto& value){return key(value);},max_payload);
}
json group_object_json(const GroupObjectIntent& v){return json::array({v.key,v.before,v.after,v.members});}
GroupObjectIntent group_object_read(const json& j,bool moving){
    if(!j.is_array()||j.size()!=4||!j[3].is_array()||j[3].size()>max_payload)throw std::runtime_error("Group object intent width/member count differs");
    GroupObjectIntent v{j[0].get<std::string>(),duplicate_bits<10>(j[1]),duplicate_bits<10>(j[2]),j[3].get<std::vector<std::string>>()};
    if(!group_object_intent_valid(moving,v))throw std::runtime_error("Group object canonical pose/member domain differs");return v;
}
json group_object_state_json(const std::optional<GroupObjectState>& v){
    if(!v)return nullptr;auto groups=group_state_json(v->ownership);
    return json::array({std::move(groups[0]),std::move(groups[1]),std::move(groups[2]),duplicate_objects(v->records.sources),v->records.untouched_sha,v->records.release});
}
std::optional<GroupObjectState> group_object_state_read(const json& j){
    if(j.is_null())return {};if(!j.is_array()||j.size()!=6||!j[3].is_array()||j[3].size()>max_payload||!j[5].is_array()||j[5].size()!=8)
        throw std::runtime_error("Group object complete state width/count differs");
    GroupObjectState v;const auto groups=group_state_read(json::array({j[0],j[1],j[2]}));if(!groups)throw std::runtime_error("Group object ownership is absent");
    v.ownership=*groups;for(const auto& row:j[3])v.records.sources.push_back(scenery_stroke_object(row));
    v.records.untouched_sha=j[4].get<std::string>();v.records.random=v.ownership.random;
    for(size_t index=0;index<8;++index)v.records.release[index]=i32(j[5][index]);
    if(!group_object_state_valid(v))throw std::runtime_error("Group object complete canonical Record/ownership state differs");return v;
}
bool group_object_effect(const Request& r,const Evidence& e){
    if((!group_object_action(r.action)&&!group_object_history(r))||!e.group_object_before||!e.group_object_after||
       !group_object_state_valid(*e.group_object_before)||!group_object_state_valid(*e.group_object_after)||
       !e.undo_confirmed||!e.selection_confirmed||e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
    const auto& before=*e.group_object_before;const auto& after=*e.group_object_after;GroupObjectIntent intent=r.group_object;
    bool moving=r.action==Action::MoveGroup,restoring=false;
    if(group_object_history(r)){
        const auto prior=group_object_target(before),next=group_object_target(after);moving=!prior.empty()&&!next.empty();restoring=prior.empty();
        if((prior.empty()&&next.empty())||(moving&&prior!=next))return false;
        const auto& live=restoring?after:before;intent.key=restoring?next:prior;
        const auto source=std::find_if(live.ownership.groups.begin(),live.ownership.groups.end(),[&](const auto& row){return row.key==intent.key;});
        if(source==live.ownership.groups.end())return false;intent.before=source->pose;
        for(const auto& member:source->members)intent.members.push_back(member.key);
        if(moving){const auto destination=std::find_if(after.ownership.groups.begin(),after.ownership.groups.end(),[&](const auto& row){return row.key==intent.key;});
            if(destination==after.ownership.groups.end())return false;intent.after=destination->pose;}
    }
    std::string cause;
    if(!game::group_object_transition::effect(moving,intent,restoring?after:before,restoring?before:after,[](const auto& value){return key(value);},max_payload,cause,restoring))return false;
    if(moving)return !e.target_retired&&e.before==e.after&&e.duplicate_cash.empty();
    if(e.target_retired==restoring||e.duplicate_cash.size()!=1||e.duplicate_cash[0].amount==INT32_MIN)return false;
    const auto& cash=e.duplicate_cash[0];const std::array<uint32_t,3> position{intent.before[4],intent.before[5],intent.before[6]};
    if(cash.position!=position||(restoring?cash.amount<0:cash.amount>0))return false;
    return game::duplicate_history::ledger(e.before,e.after,{{-cash.amount,cash.position}});
}
