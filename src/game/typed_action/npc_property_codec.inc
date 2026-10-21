bool npc_property_intent_valid(const NpcPropertyIntent& v){
    return v.kind==NpcPropertyKind::Name?(region_name(v.name)&&v.online==0):
        v.kind==NpcPropertyKind::OnlineState?(v.name.empty()&&v.online>=0&&v.online<=2):
        (v.kind==NpcPropertyKind::RaiseLevel||v.kind==NpcPropertyKind::LowerLevel)&&v.name.empty()&&v.online==0;
}
bool npc_property_state_valid(const NpcPropertyState& v){
    if(!valid_npc_state(v.actor)||v.actor.created==NpcActor{}||v.online<0||v.online>2||
       v.force_tint!=(v.online!=2)||v.region_min<0||v.region_max<v.region_min||
       v.command_level<0)return false;
    for(float f:v.tint)if(!std::isfinite(f))return false;
    return true;
}
json npc_property_intent_json(const NpcPropertyIntent& v){
    return {{"kind",uint8_t(v.kind)},{"name",v.name},{"online",v.online}};
}
NpcPropertyIntent npc_property_intent_read(const json& j){
    exact(j,{"kind","name","online"});NpcPropertyIntent v;const auto kind=u64(j.at("kind"));
    if(kind<1||kind>4)throw std::runtime_error("NPC property kind invalid");
    v.kind=NpcPropertyKind(kind);v.name=j.at("name").get<std::string>();v.online=i32(j.at("online"));
    if(!npc_property_intent_valid(v))throw std::runtime_error("NPC property intent invalid");return v;
}
json npc_property_state_json(const std::optional<NpcPropertyState>& v){
    return v?json{{"actor",npc_state_json(v->actor,true)},{"online",v->online},
        {"force_tint",v->force_tint},{"tint",v->tint},{"command_level",v->command_level},
        {"region_min",v->region_min},{"region_max",v->region_max}}:json(nullptr);
}
std::optional<NpcPropertyState> npc_property_state_read(const json& j){
    if(j.is_null())return {};
    exact(j,{"actor","online","force_tint","tint","command_level","region_min","region_max"});NpcPropertyState v;
    v.actor=npc_state_read(j.at("actor"),true);v.online=i32(j.at("online"));
    v.command_level=i32(j.at("command_level"));v.region_min=i32(j.at("region_min"));v.region_max=i32(j.at("region_max"));
    if(!j.at("force_tint").is_boolean()||!j.at("tint").is_array()||j.at("tint").size()!=4)
        throw std::runtime_error("NPC property tint shape invalid");
    v.force_tint=j.at("force_tint").get<bool>();for(size_t i=0;i<4;++i)v.tint[i]=scalar(j.at("tint")[i]);
    if(!npc_property_state_valid(v))throw std::runtime_error("NPC property state invalid");return v;
}
bool npc_property_effect(const Request& r,const Evidence& e){
    if(!npc_property_intent_valid(r.npc_property)||!e.npc_property_before||!e.npc_property_after||
       !npc_property_state_valid(*e.npc_property_before)||!npc_property_state_valid(*e.npc_property_after)||
       e.before!=e.after||e.target_retired||e.undo_confirmed||e.selection_confirmed||
       e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
    const auto& a=*e.npc_property_before;const auto& b=*e.npc_property_after;auto expected=a;
    if(r.npc_property.kind==NpcPropertyKind::Name){
        if(a.actor.created.name==r.npc_property.name||a.actor.roster_sha256==b.actor.roster_sha256)return false;
        expected.actor.created.name=r.npc_property.name;expected.actor.roster_sha256=b.actor.roster_sha256;
    }else if(r.npc_property.kind==NpcPropertyKind::OnlineState){
        if(a.online==r.npc_property.online)return false;
        expected.online=r.npc_property.online;expected.force_tint=expected.online!=2;
        // The original setter selects the native mode color. The runtime verifies that color.
        if(expected.force_tint)expected.tint=b.tint;
    }else{
        if(npc_property_unchanged(a,r.npc_property))return false;
        expected.command_level=npc_property_level(a,r.npc_property.kind);
    }
    return b==expected;
}
