uint64_t fixture_revision=1;
uint64_t revision()const{return fixture_revision;}
world_state::Admission commit_creates(const world_state::Origin&,uint64_t revision,const std::string&,
    const std::vector<world_state::LocalBinding>& created,std::string& error){
    fixture::calls.push_back("duplicate-commit");
    if(!duplicate_engine_fixture::commit_ok){error="original Duplicate World publication failure";return world_state::Admission::Rejected;}
    for(const auto& row:created)rows_[row.object_key].state=row.state;
    fixture_revision=revision;return world_state::Admission::New;
}
bool refresh_current(const world_state::Origin&,const std::string& key,const world_state::LocalBinding& observed,std::string& error){
    const auto row=rows_.find(key);
    if(row==rows_.end()||!row->second.state.alive||observed.object_key!=key||row->second.state!=observed.state){
        error="fixture fresh current binding differs";return false;}return true;
}
bool retire_native_effects(const world_state::Origin&,uint64_t revision,const std::string&,const std::vector<std::string>& keys,std::string&){
    for(const auto& key:keys)rows_.at(key).state.alive=false;fixture_revision=revision;return true;
}
bool restore_native_buildings(const world_state::Origin&,uint64_t revision,const std::string&,
    const std::vector<world_state::LocalBinding>& restored,std::string&){
    for(const auto& row:restored)rows_.at(row.object_key).state=row.state;fixture_revision=revision;return true;
}

world_state::Admission commit_scenery_stroke(const world_state::Origin&,uint64_t revision,const std::string& action_uuid,
    const std::vector<std::string>& retired,const std::vector<world_state::LocalBinding>& created,std::string& error){
    if(fixture::request.action==action::Action::EditModularBuilding){fixture::calls.push_back("modular-commit");
        if(!fixture::commit_ok){error="actual Modular world commit failure";return world_state::Admission::Rejected;}
        for(const auto& key:retired)rows_.at(key).state.alive=false;for(const auto& row:created)rows_[row.object_key].state=row.state;fixture_revision=revision;return world_state::Admission::New;}
    fixture::calls.push_back("commit");
    fixture::need(action_uuid==fixture::request.action_uuid&&created.size()==1&&created[0].object_key==fixture::request.canonical_record_key&&
        created[0].local_uid==500&&created[0].state==fixture::building,"clearance World batch lost exact native Building birth");
    const auto expected=clearance_placement_fixture::with_scenery?std::vector<std::string>{clearance_placement_fixture::scenery_key}:std::vector<std::string>{};
    fixture::need(retired==expected,"clearance World batch did not carry the exact original retirement set");
    if(!fixture::commit_ok){error="actual world commit failure";return world_state::Admission::Rejected;}
    auto next=rows_;
    for(const auto& key:retired){fixture::need(next.contains(key)&&next.at(key).state.alive,"clearance World batch retired unknown/dead Scenery");next.at(key).state.alive=false;}
    next[created[0].object_key].state=created[0].state;rows_=std::move(next);fixture_revision=revision;return world_state::Admission::New;
}
world_state::Admission commit_scenery_history(const world_state::Origin& origin,uint64_t revision,const std::string& id,
    const std::vector<std::string>& retired,const std::vector<world_state::LocalBinding>& created,std::string& error){
    return commit_scenery_stroke(origin,revision,id,retired,created,error);
}
