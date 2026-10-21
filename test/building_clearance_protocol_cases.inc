namespace building_clearance_protocol_cases {
void run(){
    const std::string placed="33333333-3333-4333-8333-333333333333",undo="44444444-4444-4444-8444-444444444444";
    auto owner=std::make_unique<Message>();auto& m=*owner;
    m.kind=Kind::HostResult;m.outcome=Outcome::Confirmed;m.session=4;m.revision=1;m.required_mask=3;
    m.request.action=Action::PlaceBuilding;m.request.peer=2;m.request.sequence=1;m.request.action_uuid=placed;
    m.request.snapshot_uuid=fixture_snapshot;m.request.canonical_record_key="action:"+placed;
    m.request.building.type="SmallInn";m.request.building.variant="base";m.request.building.alive=true;
    auto& proof=m.evidence;proof.before.record_present=1;proof.before.cash=proof.before.record_cash=1000;proof.after=proof.before;
    proof.after.cash=proof.after.record_cash=600;proof.after.record_expense=400;proof.after.line_counts[6]=1;proof.after.line_income[6]=-400;
    proof.object_after=m.request.building;proof.undo_confirmed=proof.retirement_confirmed=true;
    SceneryStrokeEffect clearance;clearance.untouched_sha=std::string(64,'a');
    clearance.removed={object(mt2mp::game::scenery_scene_key::root_record(std::string(64,'c'),67,3,0))};
    proof.building_clearance=clearance;
    std::vector<uint8_t> bytes;std::string error;auto decoded=std::make_unique<Message>();
    need(encode(m,bytes,error),"Building clearance encode: "+error);
    auto json=nlohmann::json::parse(bytes);need(json.at("schema")==38,"Building clearance did not identify its wire extension");
    need(decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==m,"Full canonical clearance result did not round trip: "+error);
    const auto original=std::make_unique<Message>(m);
    auto reject=[&](auto change){m=*original;change(m);need(!encode(m,bytes,error),"Invalid Building clearance owner/effect accepted");};
    reject([](auto& value){value.request.action=Action::MoveBuilding;});
    reject([](auto& value){value.kind=Kind::Request;});
    reject([](auto& value){value.outcome=Outcome::RejectedNoEffect;});
    reject([](auto& value){value.request.canonical_record_key="action:"+fixture_snapshot;});
    reject([](auto& value){value.evidence.building_clearance->created=value.evidence.building_clearance->removed;});
    reject([](auto& value){value.evidence.building_clearance->removed[0].family=DuplicateFamily::Building;});
    reject([](auto& value){value.evidence.building_clearance->removed[0].pose[0]=0x7fc00000;});
    m=*original;proof.building_clearance->removed.clear();need(encode(m,bytes,error)&&decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==m,"Empty actual clearance proof rejected");
    json=nlohmann::json::parse(bytes);json["schema"]=2;const auto old=json.dump();
    need(!decode(reinterpret_cast<const uint8_t*>(old.data()),old.size(),*decoded,error),"Old schema silently consumed new evidence");
    m=*original;m.request.action=Action::UndoBuilding;m.request.action_uuid=undo;m.request.target_action_uuid=placed;m.request.building={};
    proof.object_before=original->evidence.object_after;proof.object_after=proof.object_before;proof.object_after.alive=false;
    proof.target_retired=true;proof.before=original->evidence.after;proof.after=proof.before;
    proof.after.cash=proof.after.record_cash=1000;proof.after.record_income=400;proof.after.line_counts[6]=2;proof.after.line_income[6]=0;
    std::swap(proof.building_clearance->removed,proof.building_clearance->created);
    need(encode(m,bytes,error)&&decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==m,"Undo same saved Scenery aliases did not round trip: "+error);
    m.request.action=Action::RedoBuilding;std::swap(proof.object_before,proof.object_after);proof.target_retired=false;
    proof.before=proof.after;proof.after=proof.before;proof.after.cash-=400;proof.after.record_cash-=400;
    proof.after.record_income-=400;proof.after.line_counts[6]+=1;proof.after.line_income[6]-=400;
    std::swap(proof.building_clearance->removed,proof.building_clearance->created);
    need(encode(m,bytes,error)&&decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==m,"Redo same clearance set did not round trip: "+error);
    // The unchanged ordinary result remains the old schema without this proof.
    m=*original;proof.building_clearance.reset();need(encode(m,bytes,error)&&nlohmann::json::parse(bytes).at("schema")==2,"Ordinary legacy Building codec changed");
}
}
