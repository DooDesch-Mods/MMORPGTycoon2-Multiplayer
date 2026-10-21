namespace scene_scenery_control_protocol_fixture {
namespace a=mt2mp::action_session;namespace c=mt2mp::game::dungeon_gizmo_control;
template<class Check>void run(Check& check){
    for(bool mixed:{false,true})for(auto command:{c::Command::Move,c::Command::Duplicate,c::Command::Destroy}){
        auto message=std::make_unique<a::Message>(input_fixture::result());auto& intent=message->request.dungeon_input;
        intent={};intent.key=std::string(64,'b')+":world:building:17";intent.scene=intent.key+"/template";
        intent.tool=a::DungeonInputTool::Gizmos;intent.before_sha256=std::string(64,'a');message->request.canonical_record_key=intent.key;
        intent.controls.emplace();auto& controls=*intent.controls;controls.command=command;
        if(mixed){c::Row row;row.key=mt2mp::game::dungeon_gizmo_identity::birth("33333333-3333-4333-8333-333333333333",0);
            row.before.type="container";row.before.variant="base";row.before.name="Box";row.after=c::pose(row.before);
            if(command!=c::Command::Destroy)row.after[4]=std::bit_cast<uint32_t>(10.0f);row.cost=command==c::Command::Move?0:100;controls.rows.push_back(row);}
        c::Row scenery;scenery.key=mt2mp::game::scenery_scene_key::record(std::string(64,'b'),67,3,-1,0);
        scenery.scenery=c::Scenery{"Trees","oak",c::pose(mt2mp::game::dungeon_gizmo_reader::Record{})};scenery.after=scenery.scenery->pose;
        if(command!=c::Command::Destroy)scenery.after[4]=std::bit_cast<uint32_t>(20.0f);scenery.cost=command==c::Command::Move?0:10;controls.rows.push_back(scenery);
        auto& proof=message->evidence;proof.before=proof.after=input_fixture::money();proof.dungeon_input_before=input_fixture::before();
        proof.dungeon_input_before.birth.reset();proof.dungeon_input_before.edits.emplace();proof.dungeon_input_before.scenery_edits.emplace();proof.dungeon_input_before.scenery_release.emplace();
        for(const auto& row:controls.rows)if(row.scenery)proof.dungeon_input_before.scenery_edits->push_back({row.key,*row.scenery});else proof.dungeon_input_before.edits->push_back({row.key,row.before});
        proof.dungeon_input_after=proof.dungeon_input_before;++proof.dungeon_input_after.protected_state.dungeon.undo_count;
        proof.dungeon_input_after.scenes_sha256=std::string(64,'f');if(command==c::Command::Duplicate)++proof.dungeon_input_after.scenery_release->at(2);proof.dungeon_input_after.edits->clear();proof.dungeon_input_after.scenery_edits->clear();
        for(size_t i=0;i<controls.rows.size()&&command!=c::Command::Destroy;++i){const auto& row=controls.rows[i];
            const auto key=command==c::Command::Duplicate?c::birth(row,message->request.action_uuid,uint32_t(i)):row.key;
            if(row.scenery){auto value=*row.scenery;value.pose=row.after;value.height=std::bit_cast<uint32_t>(2.0f);proof.dungeon_input_after.scenery_edits->push_back({key,value});}
            else {auto value=row.before;c::pose(value,row.after);proof.dungeon_input_after.edits->push_back({key,value});}}
        const int32_t cost=c::total(controls);if(command!=c::Command::Move){const int32_t amount=command==c::Command::Destroy?cost:-cost;
            proof.after.cash+=amount;proof.after.record_cash+=amount;proof.after.line_income[6]+=amount;
            if(command==c::Command::Destroy)proof.after.record_income+=cost;else proof.after.record_expense+=cost;
            proof.after.line_counts[6]+=command==c::Command::Duplicate?1:int32_t(controls.rows.size());}
        std::vector<uint8_t> wire;std::string error;
        check(a::encode(*message,wire,error),error);auto decoded=std::make_unique<a::Message>();
        check(a::decode(wire.data(),wire.size(),*decoded,error)&&*decoded==*message,"Actual Scene-only/mixed66 complete state wire round trip");
        std::vector<std::string> names(controls.rows.size(),"container #2");
        check(a::dungeon_input_wire_fits(message->request,proof.dungeon_input_before,proof.before,error,nullptr,nullptr,command==c::Command::Duplicate?&names:nullptr),error);
        const auto reject=[&](const nlohmann::json& value){const auto bytes=value.dump();return !a::decode(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size(),*decoded,error);};
        auto packet=nlohmann::json::parse(wire);auto& row=packet["request"]["dungeon_input"]["controls"]["rows"].back();
        check(row.contains("scenery")&&!row.contains("before"),"Scene Scenery is a real typed row without a fake Gizmo record");
        row["before"]={};check(reject(packet),"Mixed66 rejects a row with both native families");
        packet=nlohmann::json::parse(wire);packet["evidence"]["dungeon_input_after"].erase("scenery_edits");
        check(reject(packet),"Mixed66 rejects missing complete Scenery after values");
        auto changed=std::make_unique<a::Message>(*message);changed->request.dungeon_input.controls->rows.back().before.name="fake Gizmo";
        check(!a::encode(*changed,wire,error),"Scene Scenery cannot conceal populated fake Gizmo inputs");
        changed=std::make_unique<a::Message>(*message);changed->evidence.dungeon_input_before.scenery_edits->at(0).value.height=std::bit_cast<uint32_t>(3.0f);
        check(!a::encode(*changed,wire,error),"Scene Scenery complete original height must match before effect");
        if(command==c::Command::Duplicate){changed=std::make_unique<a::Message>(*message);--changed->evidence.dungeon_input_after.scenery_release->at(2);
            check(!a::encode(*changed,wire,error),"Scene Duplicate omitted its one original Scenery Release edit");}
        changed=std::make_unique<a::Message>(*message);changed->request.dungeon_input.controls->rows.back().scenery->pose[0]=0x7fc00000;
        check(!a::encode(*changed,wire,error),"Scene Scenery nonfinite stored input rejected");
        auto history=std::make_unique<a::Message>(*message);history->request.action=a::Action::UndoBuilding;
        history->request.target_action_uuid=message->request.action_uuid;history->request.action_uuid="44444444-4444-4444-8444-444444444444";
        history->request.canonical_record_key="dungeon-input:"+message->request.action_uuid;history->request.dungeon_input={};
        history->evidence.dungeon_input_origin=intent;history->evidence.dungeon_input_before=proof.dungeon_input_after;history->evidence.dungeon_input_after=proof.dungeon_input_before;history->evidence.dungeon_input_after.scenery_release=history->evidence.dungeon_input_before.scenery_release;
        history->evidence.before=history->evidence.after=proof.after;
        if(command!=c::Command::Move){const int32_t amount=command==c::Command::Destroy?-cost:cost;
            history->evidence.after.cash+=amount;history->evidence.after.record_cash+=amount;history->evidence.after.record_income+=amount;
            history->evidence.after.line_counts[6]+=int32_t(controls.rows.size());history->evidence.after.line_income[6]+=amount;}
        check(a::encode(*history,wire,error)&&a::decode(wire.data(),wire.size(),*decoded,error)&&*decoded==*history,"Mixed66 original history retains both real families and complete cash");
        changed=std::make_unique<a::Message>(*history);changed->evidence.dungeon_input_after.scenery_edits->at(0).value.variant="other";
        check(!a::encode(*changed,wire,error),"Mixed66 Undo rejects changed original Scenery resource");
    }
}
}
