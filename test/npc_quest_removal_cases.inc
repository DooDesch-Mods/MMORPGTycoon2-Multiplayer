void npc_quest_removal_cases(size_t& checks){
    std::string error;std::vector<uint8_t> bytes;auto output=std::make_unique<Message>();
    const auto check=[&](bool okay,const std::string& label){++checks;need(okay,label+" cause="+error);};
    for(int32_t type=0;type<3;++type){
        auto storage=std::make_unique<Message>();auto& message=*storage;
        message.session=5;message.kind=Kind::HostResult;message.revision=1;message.required_mask=3;message.outcome=Outcome::Confirmed;
        auto& r=message.request;r.action=Action::RemoveNPC;r.peer=2;r.sequence=1;r.action_uuid=action_id;
        r.snapshot_uuid=snapshot_id;r.canonical_record_key=owner_key;r.target_action_uuid=owner_key.substr(7);
        auto& e=message.evidence;e.npc_before.count=3;e.npc_before.undo_count=1;e.npc_before.roster_sha256=std::string(64,'a');
        e.npc_before.created.name="Removed NPC";e.npc_before.created.type=type;
        e.npc_before.created.character_slot=0;e.npc_before.created.character_type=0;e.npc_before.created.level=1;
        e.npc_before.created.quest_count=0;
        e.npc_before.created.scale={1,1,1};
        auto incoming=record(2);incoming.destination_key=owner_key;incoming.completed.total=1;
        auto owned=record(0);
        if(type==0){e.npc_before.created.quests={owned};e.npc_before.created.quest_count=1;e.npc_before.quest_targets_sha256=std::string(64,'c');}
        e.npc_after=e.npc_before;--e.npc_after.count;++e.npc_after.undo_count;e.npc_after.created={};
        e.npc_after.roster_sha256=std::string(64,'b');e.undo_confirmed=e.target_retired=e.selection_confirmed=e.retirement_confirmed=true;
        e.npc_quest_removal.emplace();auto& proof=*e.npc_quest_removal;auto& quests=proof.quests;
        proof.references_before=std::string(64,'d');proof.references_after=std::string(64,'e');
        quests.before_sha256=std::string(64,'a');quests.after_sha256=std::string(64,'b');
        quests.before_count=3;quests.completed_before=type==0?2:1;
        quests.removed.push_back({"action:44444444-4444-4444-8444-444444444444",incoming});
        if(type==0)quests.removed.push_back({owner_key,owned});
        quests.after_count=quests.before_count-uint32_t(quests.removed.size());quests.completed_after=0;
        check(encode(message,bytes,error),"Complete NPC incoming/owned Quest receipt encoding");
        const auto encoded=nlohmann::json::parse(bytes);check(encoded.at("schema")==36,"NPC removal uses isolated schema36");
        check(decode(bytes.data(),bytes.size(),*output,error)&&*output==message,"Complete NPC retirement roundtrip");
        check(npc_quest_removal_wire_fits(r,e.npc_before,e.before,proof,error),"Complete pre-original NPC receipt fits");
        const auto invalid=[&](auto change,const std::string& label){auto bad=std::make_unique<Message>(message);change(*bad);check(!encode(*bad,bytes,error),label);};
        invalid([](auto& m){m.evidence.npc_quest_removal->quests.removed[0].record.destination_key="action:66666666-6666-4666-8666-666666666666";},"Foreign incoming Quest rejected");
        invalid([](auto& m){++m.evidence.npc_quest_removal->quests.after_count;},"Complete retired count required");
        invalid([](auto& m){++m.evidence.npc_quest_removal->quests.completed_after;},"Retired completion aggregate required");
        invalid([](auto& m){m.evidence.npc_quest_removal->references_after="bad";},"Complete active-reference digest required");
        invalid([](auto& m){auto& p=m.evidence.npc_quest_removal->quests;p.removed.push_back(p.removed[0]);--p.after_count;},"Repeated owner/slot rejected");
        invalid([](auto& m){m.request.action=Action::MoveNPC;m.request.building.alive=true;},"Retirement evidence cannot leak to Move");
        for(int kind=0;kind<3;++kind){auto bad=encoded;
            if(kind==0)bad["schema"]=19;
            if(kind==1)bad["evidence"]["npc_quest_removal"]["extra"]=true;
            if(kind==2)bad["evidence"]["retirement_confirmed"]=1;
            const auto text=bad.dump();check(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),*output,error),"Strict schema36 shape and bools retained");
        }
        auto empty=std::make_unique<Message>(message);auto& p=empty->evidence.npc_quest_removal->quests;
        p.removed.clear();p.after_count=p.before_count;p.completed_after=p.completed_before;p.after_sha256=p.before_sha256;
        check(encode(*empty,bytes,error)&&decode(bytes.data(),bytes.size(),*output,error)&&*output==*empty,"NPC without retired Quests still proves expired NPC references");
        auto oversized=proof;oversized.quests.before_count=100;oversized.quests.completed_before=0;oversized.quests.removed.clear();
        for(int32_t slot=0;slot<80;++slot){auto row=incoming;row.sequence_id=slot;row.name=std::string(1000,'x');row.completed.total=0;
            oversized.quests.removed.push_back({"action:44444444-4444-4444-8444-444444444444",std::move(row)});}
        check(!npc_quest_removal_wire_fits(r,e.npc_before,e.before,oversized,error)&&error=="action payload exceeds 60000 bytes","Oversized complete receipt rejected before effect with original cap cause");
    }
}
