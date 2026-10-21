namespace gizmo_control_protocol_fixture {
namespace a=mt2mp::action_session;namespace c=mt2mp::game::dungeon_gizmo_control;
template<class Check>void run(Check& check){
    std::string error;std::vector<uint8_t> encoded;
    const auto valid=[](const a::Message& value,std::string& cause){std::vector<uint8_t> wire;return a::encode(value,wire,cause);};
    for(const auto command:{c::Command::Move,c::Command::Duplicate,c::Command::Destroy}){
        auto packet=std::make_unique<a::Message>(input_fixture::result());auto& request=packet->request;auto& intent=request.dungeon_input;
        intent={};intent.key=std::string(64,'b')+":world:building:17";intent.scene=intent.key+"/template";intent.tool=a::DungeonInputTool::Gizmos;
        intent.before_sha256=std::string(64,'a');request.canonical_record_key=intent.key;
        c::Intent controls;controls.command=command;
        for(unsigned i=0;i<2;++i){c::Row row;row.key=mt2mp::game::dungeon_gizmo_identity::birth("33333333-3333-4333-8333-333333333333",i);
            row.before.name="Original crate "+std::to_string(i);row.before.type="container";row.before.variant="base";
            row.before.rotation={0,0,0,1};row.before.scale={1,1,1};row.before.position={float(i),2,3};row.before.gold=3;
            row.after=c::pose(row.before);if(command!=c::Command::Destroy)row.after[4]=std::bit_cast<uint32_t>(float(i)+10);
            row.cost=command==c::Command::Move?0:100;controls.rows.push_back(row);}
        intent.controls=controls;auto& proof=packet->evidence;proof.before=proof.after=input_fixture::money();
        proof.dungeon_input_before=input_fixture::before();proof.dungeon_input_before.birth.reset();proof.dungeon_input_before.edits.emplace();
        for(const auto& row:controls.rows)proof.dungeon_input_before.edits->push_back({row.key,row.before});
        proof.dungeon_input_after=proof.dungeon_input_before;++proof.dungeon_input_after.protected_state.dungeon.undo_count;
        proof.dungeon_input_after.scenes_sha256=std::string(64,'f');proof.dungeon_input_after.edits->clear();
        if(command!=c::Command::Destroy)for(size_t i=0;i<controls.rows.size();++i){const auto& row=controls.rows[i];auto value=row.before;c::pose(value,row.after);
            proof.dungeon_input_after.edits->push_back({command==c::Command::Duplicate?mt2mp::game::dungeon_gizmo_identity::birth(request.action_uuid,uint32_t(i)):row.key,value});}
        if(command!=c::Command::Move){const int32_t amount=command==c::Command::Destroy?200:-200;proof.after.cash+=amount;proof.after.record_cash+=amount;
            if(command==c::Command::Destroy)proof.after.record_income+=amount;else proof.after.record_expense-=amount;
            proof.after.line_counts[6]+=command==c::Command::Duplicate?1:2;proof.after.line_income[6]+=amount;}
        std::vector<std::string> name_bounds;
        if(command==c::Command::Duplicate)for(const auto& row:controls.rows)name_bounds.push_back(mt2mp::game::dungeon_gizmo_name_bound::widest(row.before.type,","));
        const auto* native_names=name_bounds.empty()?nullptr:&name_bounds;
        check(valid(*packet,error),error);check(a::dungeon_input_wire_fits(request,proof.dungeon_input_before,proof.before,error,nullptr,nullptr,native_names),error);
        if(native_names){
            check(!a::dungeon_input_wire_fits(request,proof.dungeon_input_before,proof.before,error),"codec-only fallback lost conservative output bound");
            auto incomplete=name_bounds;incomplete.pop_back();check(!a::dungeon_input_wire_fits(request,proof.dungeon_input_before,proof.before,error,nullptr,nullptr,&incomplete),"incomplete native output-name bounds accepted");
            auto oversized=name_bounds;for(auto& name:oversized)name=std::string(8192,'\1');
            check(!a::dungeon_input_wire_fits(request,proof.dungeon_input_before,proof.before,error,nullptr,nullptr,&oversized),"actual escaped oversized output names bypassed preflight");
        }
        check(a::encode(*packet,encoded,error),error);auto decoded=std::make_unique<a::Message>();
        check(a::decode(encoded.data(),encoded.size(),*decoded,error)&&*decoded==*packet,error);
        const auto rejects=[&](nlohmann::json document){const auto text=document.dump();return !a::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),*decoded,error);};
        auto document=nlohmann::json::parse(encoded);document["request"]["dungeon_input"]["controls"]["rows"][0]["after"][0]=uint64_t(UINT32_MAX)+1;
        check(rejects(document),"Gizmo control accepted oversized float bits");
        document=nlohmann::json::parse(encoded);document["request"]["dungeon_input"]["controls"]["foreign"]=0;
        check(rejects(document),"Gizmo control accepted unknown field");
        document=nlohmann::json::parse(encoded);document["evidence"]["dungeon_input_after"].erase("edits");
        check(rejects(document),"Gizmo control accepted missing complete result rows");
        auto bad=std::make_unique<a::Message>(*packet);++bad->evidence.after.line_counts[7];
        check(!valid(*bad,error),"Gizmo control accepted unrelated ledger change");
        bad=std::make_unique<a::Message>(*packet);bad->request.dungeon_input.controls->rows[1].key=controls.rows[0].key;
        check(!valid(*bad,error),"Gizmo control accepted repeated canonical target");
        auto history=std::make_unique<a::Message>(*packet);auto& history_request=history->request;
        history_request.action=a::Action::UndoBuilding;history_request.target_action_uuid=request.action_uuid;history_request.action_uuid="44444444-4444-4444-8444-444444444444";
        history_request.canonical_record_key="dungeon-input:"+request.action_uuid;history_request.dungeon_input={};
        auto& history_proof=history->evidence;history_proof.dungeon_input_origin=intent;
        history_proof.dungeon_input_before=proof.dungeon_input_after;history_proof.dungeon_input_after=proof.dungeon_input_before;
        history_proof.before=history_proof.after=proof.after;
        if(command!=c::Command::Move){const int32_t amount=command==c::Command::Destroy?-200:200;
            history_proof.after.cash+=amount;history_proof.after.record_cash+=amount;history_proof.after.record_income+=amount;
            history_proof.after.line_counts[6]+=2;history_proof.after.line_income[6]+=amount;}
        check(valid(*history,error),error);check(a::dungeon_input_wire_fits(history_request,history_proof.dungeon_input_before,history_proof.before,error,&intent),error);
        check(a::encode(*history,encoded,error)&&a::decode(encoded.data(),encoded.size(),*decoded,error)&&*decoded==*history,error);
        if(command==c::Command::Duplicate){
            auto redo=std::make_unique<a::Message>(*history);redo->request.action=a::Action::RedoBuilding;
            redo->evidence.dungeon_input_before=history_proof.dungeon_input_after;redo->evidence.dungeon_input_after=history_proof.dungeon_input_before;
            std::vector<std::string> retained;for(const auto& row:*proof.dungeon_input_after.edits)retained.push_back(row.value.name);
            check(a::dungeon_input_wire_fits(redo->request,redo->evidence.dungeon_input_before,redo->evidence.before,error,&intent,nullptr,&retained),error);
        }
        bad=std::make_unique<a::Message>(*history);bad->evidence.dungeon_input_after.edits->at(0).value.name="lost original name";
        check(!valid(*bad,error),"Gizmo Undo accepted incomplete original record restoration");
        auto huge=std::make_unique<a::Message>(*packet);auto& many=*huge->request.dungeon_input.controls;
        for(uint32_t i=2;i<30;++i){auto row=many.rows.front();row.key=mt2mp::game::dungeon_gizmo_identity::birth("33333333-3333-4333-8333-333333333333",i);
            row.before.name=std::string(8192,'x');many.rows.push_back(row);}
        check(!a::dungeon_input_wire_fits(huge->request,huge->evidence.dungeon_input_before,huge->evidence.before,error)&&error.find("bytes")!=std::string::npos,
            "Gizmo complete control proof bypassed actual packet preflight");
    }
}
}
