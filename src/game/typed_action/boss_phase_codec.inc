bool boss_text(const std::string& value){
    if(value.size()>4096||value.find('\0')!=std::string::npos)return false;
    try{(void)json(value).dump();return true;}catch(const json::type_error&){return false;}
}
bool boss_intent_valid(const BossPhaseIntent& v){
    if(v.character_id<0||v.phase<0||v.phase>=10000)return false;
    const bool empty_color=v.color==std::array<float,4>{};
    if(v.kind!=BossPhaseKind::RemoveAbility&&v.element!=-1)return false;
    if(v.kind!=BossPhaseKind::AddAbility&&!v.ability_indices.empty())return false;
    switch(v.kind){
    case BossPhaseKind::Reorder:return v.boundary>=0&&v.boundary<=10000&&v.text.empty()&&!v.health&&empty_color;
    case BossPhaseKind::Name:case BossPhaseKind::Vfx:return v.boundary==-1&&boss_text(v.text)&&!v.health&&empty_color;
    case BossPhaseKind::Health:return v.boundary==-1&&v.text.empty()&&empty_color;
    case BossPhaseKind::Color:return v.boundary==-1&&v.text.empty()&&!v.health&&std::all_of(v.color.begin(),v.color.end(),[](float x){return std::isfinite(x);});
    case BossPhaseKind::Create:return v.boundary==-1&&boss_text(v.text)&&!v.health&&empty_color;
    case BossPhaseKind::Destroy:return v.boundary==-1&&v.text.empty()&&!v.health&&empty_color;
    case BossPhaseKind::RemoveAbility:return v.boundary==-1&&v.element>=0&&v.element<10000&&v.text.empty()&&!v.health&&empty_color;
    case BossPhaseKind::AddAbility:{if(v.boundary!=-1||!boss_text(v.text)||v.health||!empty_color||v.ability_indices.empty()||v.ability_indices.size()>10000)return false;
        int32_t prior=-1;for(auto index:v.ability_indices){if(index<=prior||index>=10000)return false;prior=index;}return true;}
    }return false;
}
bool boss_state_valid(const BossPhaseState& v){
    if(v.bosses.size()>10000||v.release_edits<0)return false;
    for(float value:v.default_color)if(!std::isfinite(value))return false;
    for(size_t i=0;i<v.bosses.size();++i){const auto& b=v.bosses[i];
        if(b.id!=int32_t(i)||!boss_text(b.name)||!boss_text(b.definition)||b.phases.size()>10000||b.ability_names.size()>10000)return false;
        for(const auto& name:b.ability_names)if(!boss_text(name))return false;
        for(const auto& p:b.phases){if(!boss_text(p.name)||!boss_text(p.vfx)||p.abilities.size()>10000)return false;
            for(float f:p.color)if(!std::isfinite(f))return false;
            for(int32_t index:p.abilities)if(index<0||size_t(index)>=b.ability_names.size())return false;}
    }return true;
}
json boss_intent_json(const BossPhaseIntent& v){return {{"character_id",v.character_id},{"phase",v.phase},{"boundary",v.boundary},{"kind",uint8_t(v.kind)},{"text",v.text},{"health",v.health},{"color",v.color},{"element",v.element},{"ability_indices",v.ability_indices}};}
BossPhaseIntent boss_intent_read(const json& j){exact(j,{"character_id","phase","boundary","kind","text","health","color","element","ability_indices"});
    BossPhaseIntent v;v.character_id=i32(j.at("character_id"));v.phase=i32(j.at("phase"));v.boundary=i32(j.at("boundary"));
    const auto kind=i32(j.at("kind"));if(kind<0||kind>8)throw std::runtime_error("Boss phase kind invalid");v.kind=BossPhaseKind(kind);
    v.text=j.at("text").get<std::string>();v.health=i32(j.at("health"));
    if(!j.at("color").is_array()||j.at("color").size()!=4)throw std::runtime_error("Boss phase intent color invalid");
    for(size_t i=0;i<4;++i)v.color[i]=j.at("color").at(i).get<float>();
    v.element=i32(j.at("element"));if(!j.at("ability_indices").is_array()||j.at("ability_indices").size()>10000)throw std::runtime_error("Boss ability indices bounds invalid");
    for(const auto& index:j.at("ability_indices"))v.ability_indices.push_back(i32(index));
    if(!boss_intent_valid(v))throw std::runtime_error("Boss phase intent invalid");return v;}
json boss_state_json(const std::optional<BossPhaseState>& v){
    if(!v)return nullptr;json bosses=json::array();
    for(const auto& b:v->bosses){json phases=json::array();for(const auto& p:b.phases)phases.push_back({{"name",p.name},{"vfx",p.vfx},{"health",p.health},{"color",p.color},{"abilities",p.abilities}});
        bosses.push_back({{"id",b.id},{"name",b.name},{"definition",b.definition},{"rename_day",b.rename_day},{"deleted",b.deleted},{"edited",b.edited},{"released",b.released},{"ability_names",b.ability_names},{"phases",std::move(phases)}});}
    return {{"bosses",std::move(bosses)},{"release_edits",v->release_edits},{"default_color",v->default_color}};
}
std::optional<BossPhaseState> boss_state_read(const json& j){
    if(j.is_null())return {};exact(j,{"bosses","release_edits","default_color"});BossPhaseState v;v.release_edits=i32(j.at("release_edits"));
    if(!j.at("default_color").is_array()||j.at("default_color").size()!=4)throw std::runtime_error("Boss phase default color invalid");
    for(size_t i=0;i<4;++i)v.default_color[i]=j.at("default_color").at(i).get<float>();
    if(!j.at("bosses").is_array()||j.at("bosses").size()>10000)throw std::runtime_error("Boss census bounds invalid");
    for(const auto& b:j.at("bosses")){exact(b,{"id","name","definition","rename_day","deleted","edited","released","ability_names","phases"});BossTypePhases type;
        type.id=i32(b.at("id"));type.rename_day=i32(b.at("rename_day"));type.name=b.at("name").get<std::string>();type.definition=b.at("definition").get<std::string>();
        for(const auto* flag:{"deleted","edited","released"})if(!b.at(flag).is_boolean())throw std::runtime_error("Boss type flag invalid");
        type.deleted=b.at("deleted").get<bool>();type.edited=b.at("edited").get<bool>();type.released=b.at("released").get<bool>();
        if(!b.at("ability_names").is_array()||b.at("ability_names").size()>10000)throw std::runtime_error("Boss ability name census invalid");
        for(const auto& name:b.at("ability_names"))type.ability_names.push_back(name.get<std::string>());
        if(!b.at("phases").is_array()||b.at("phases").size()>10000)throw std::runtime_error("Boss phases bounds invalid");
        for(const auto& p:b.at("phases")){exact(p,{"name","vfx","health","color","abilities"});BossPhaseRow row;
            row.name=p.at("name").get<std::string>();row.vfx=p.at("vfx").get<std::string>();row.health=i32(p.at("health"));
            if(!p.at("color").is_array()||p.at("color").size()!=4||!p.at("abilities").is_array()||p.at("abilities").size()>10000)throw std::runtime_error("Boss phase fields bounds invalid");
            for(size_t index=0;index<4;++index)row.color[index]=p.at("color").at(index).get<float>();
            for(const auto& index:p.at("abilities"))row.abilities.push_back(i32(index));type.phases.push_back(std::move(row));}
        v.bosses.push_back(std::move(type));}
    if(!boss_state_valid(v))throw std::runtime_error("Boss phase state invalid");return v;
}
bool boss_effect(const Request& r,const Evidence& e){
    if(!boss_intent_valid(r.boss_phase)||!e.boss_phase_before||!e.boss_phase_after||!boss_state_valid(*e.boss_phase_before)||!boss_state_valid(*e.boss_phase_after)||
       e.before!=e.after||e.target_retired||e.undo_confirmed||e.selection_confirmed||e.object_before!=BuildingState{}||e.object_after!=BuildingState{})return false;
    const auto& intent=r.boss_phase;auto expected=*e.boss_phase_before;
    if(size_t(intent.character_id)>=expected.bosses.size())return false;auto& owner=expected.bosses[size_t(intent.character_id)];
    if(owner.deleted)return false;
    if(intent.kind==BossPhaseKind::Create){
        if(size_t(intent.phase)!=owner.phases.size()||owner.phases.size()>=10000||intent.text!="{design_boss_defaultphasename} "+std::to_string(owner.phases.size()+1))return false;
        owner.phases.push_back({intent.text,"",100,expected.default_color,{}});return expected==*e.boss_phase_after;
    }
    if(size_t(intent.phase)>=owner.phases.size())return false;
    if(intent.kind==BossPhaseKind::Destroy){owner.phases.erase(owner.phases.begin()+intent.phase);return expected==*e.boss_phase_after;}
    if(intent.kind==BossPhaseKind::Reorder){
        if(size_t(intent.boundary)>owner.phases.size()||intent.boundary==intent.phase||intent.boundary==intent.phase+1)return false;
        auto phase=owner.phases[size_t(intent.phase)];owner.phases.erase(owner.phases.begin()+intent.phase);
        owner.phases.insert(owner.phases.begin()+(intent.boundary>intent.phase?intent.boundary-1:intent.boundary),std::move(phase));
    }else{auto& phase=owner.phases[size_t(intent.phase)];const auto prior=phase;
        switch(intent.kind){case BossPhaseKind::Name:phase.name=intent.text;break;case BossPhaseKind::Health:phase.health=intent.health;break;
        case BossPhaseKind::Vfx:phase.vfx=intent.text;break;case BossPhaseKind::Color:phase.color=intent.color;break;
        case BossPhaseKind::RemoveAbility:if(size_t(intent.element)>=phase.abilities.size())return false;phase.abilities.erase(phase.abilities.begin()+intent.element);break;
        case BossPhaseKind::AddAbility:{std::vector<int32_t> matches;for(size_t i=0;i<owner.ability_names.size();++i)if(owner.ability_names[i]==intent.text)matches.push_back(int32_t(i));
            if(matches!=intent.ability_indices||phase.abilities.size()+matches.size()>10000)return false;phase.abilities.insert(phase.abilities.end(),matches.begin(),matches.end());break;}
        default:return false;}
        if(prior==phase)return false;
    }
    return expected==*e.boss_phase_after;
}
