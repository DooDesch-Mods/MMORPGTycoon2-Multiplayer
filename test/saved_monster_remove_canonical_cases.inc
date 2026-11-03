// The loaded actor comes from retained Move evidence. The Remove outcome below
// is a derived codec fixture, not an observed native removal.
void canonical_remove_cases(unsigned& n,const Message& quest){
    namespace tick=mt2mp::action_session::tick_commands;
    std::string error,expected,actual;std::vector<uint8_t> bytes;
    auto canonical=[&](const Message& message,std::string& digest){
        Context context;context.session=message.session;context.snapshot_uuid=message.request.snapshot_uuid;context.peer_mask=static_cast<uint8_t>(message.required_mask);
        tick::Assigned assigned{message.revision,{message.session,message.request.snapshot_uuid,message.request}};
        tick::ExecutionReceipt receipt;receipt.command=assigned;receipt.outcome=Outcome::Confirmed;receipt.native_attempted=true;receipt.evidence=std::make_shared<Evidence>(message.evidence);
        return tick::canonical_evidence(context,assigned,receipt,digest,error);
    };
    auto host=std::make_unique<Message>();const auto retained=std::string(retained_move);
    need(decode(reinterpret_cast<const uint8_t*>(retained.data()),retained.size(),*host,error),error);
    host->request.action=Action::RemoveMonster;host->request.monster_remove.region=host->request.monster_move.region;host->request.monster_move={};
    host->evidence.monster_after=removed(host->evidence.monster_before);host->evidence.target_retired=true;
    host->evidence.monster_quests.before_sha256=host->evidence.monster_quests.after_sha256=std::string(64,'d');
    need(encode(*host,bytes,error),error);const auto original_host_hash=mt2mp::sha256(bytes);
    auto client=std::make_unique<Message>(*host);client->evidence.monster_before.actor.tag=-1010040832;
    need(encode(*client,bytes,error),error);need(mt2mp::sha256(bytes)!=original_host_hash,"Distinct retained loaded serials did not reproduce raw Remove hash disagreement");++n;
    need(monster_shared_state(host->evidence.monster_before,client->evidence.monster_before),"Existing shared actor contract differs");++n;
    need(canonical(*host,expected)&&canonical(*client,actual)&&actual==expected,error);++n;
    need(expected==original_host_hash,"Zero-serial Host Remove canonical bytes changed");++n;
    for(int32_t serial:{INT32_MIN,-1010040832,-1,0,INT32_MAX}){
        auto local=std::make_unique<Message>(*host);local->evidence.monster_before.actor.tag=serial;auto original=std::make_unique<Message>(*local);
        need(canonical(*local,actual)&&actual==expected,"Loaded serial became Remove canonical identity");
        auto decoded=std::make_unique<Message>();need(*local==*original&&encode(*local,bytes,error)&&decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==*local,"Projection modified full raw Remove receipt or codec");
        need(local->evidence.monster_after.actor==MonsterActor{}&&local->evidence.monster_after.actor.tag==-1,"Remove absent actor sentinel changed");++n;
    }
    auto distinct=[&](auto change){auto local=std::make_unique<Message>(*host);change(*local);need(!canonical(*local,actual)||actual!=expected,"Shared Remove evidence collapsed into canonical equality");++n;};
    distinct([](auto& m){++m.evidence.monster_before.actor.position[0];});
    distinct([](auto& m){m.evidence.monster_before.actor.rotation={0,0,0,1};});
    distinct([](auto& m){m.evidence.monster_before.actor.scale={2,2,2};});
    distinct([](auto& m){m.evidence.monster_before.actor.name="Different";});
    distinct([](auto& m){++m.evidence.monster_before.actor.level;});
    distinct([](auto& m){++m.evidence.monster_before.actor.character_type;});
    distinct([](auto& m){++m.evidence.monster_before.roster.next_tag;++m.evidence.monster_after.roster.next_tag;});
    distinct([](auto& m){++m.evidence.monster_before.roster.region_random[0];++m.evidence.monster_after.roster.region_random[0];});
    distinct([](auto& m){++m.evidence.monster_before.roster.global_random[0];});
    distinct([](auto& m){++m.evidence.monster_after.roster.global_random[0];});
    distinct([](auto& m){++m.evidence.monster_after.roster.release_edits[7];});
    distinct([](auto& m){m.evidence.monster_after.roster.content_sha256=std::string(64,'a');});
    distinct([](auto& m){++m.evidence.before.cash;++m.evidence.after.cash;});
    distinct([](auto& m){m.request.canonical_record_key=std::string("action:")+birth_id;m.request.target_action_uuid=birth_id;});
    distinct([](auto& m){++m.request.monster_remove.region;});
    distinct([](auto& m){m.evidence.monster_quests.before_sha256=m.evidence.monster_quests.after_sha256=std::string(64,'a');});
    auto reject=[&](auto change){auto local=std::make_unique<Message>(*host);change(*local);need(!canonical(*local,actual)&&actual.empty(),"Invalid raw Remove transition became canonical success");++n;};
    reject([](auto& m){m.evidence.monster_after.actor.tag=0;});
    reject([](auto& m){m.evidence.monster_before.actor={};});
    reject([](auto& m){m.evidence.monster_after.actor=m.evidence.monster_before.actor;});
    reject([](auto& m){++m.evidence.after.cash;});
    reject([](auto& m){++m.evidence.monster_after.roster.region_random[0];});
    reject([](auto& m){++m.evidence.monster_after.roster.next_tag;});
    reject([](auto& m){--m.evidence.monster_after.roster.undo_count;});
    reject([](auto& m){m.evidence.target_retired=false;});
    reject([](auto& m){m.evidence.selection_confirmed=false;});
    reject([](auto& m){m.evidence.undo_confirmed=false;});
    reject([](auto& m){m.evidence.retirement_confirmed=false;});
    reject([](auto& m){m.evidence.monster_quests.before_sha256.clear();});
    need(canonical(quest,expected),error);auto changed_quest=std::make_unique<Message>(quest);
    changed_quest->evidence.monster_quests.removed.front().record.name="Changed retired Quest";
    need(canonical(*changed_quest,actual)&&actual!=expected,"Removed full Quest record vanished from canonical evidence");++n;
    for(int32_t serial:{0,17,INT32_MAX-1}){
        auto placement=std::make_unique<Message>(result());placement->evidence.monster_before.roster.next_tag=serial;
        placement->evidence.monster_after=born(placement->evidence.monster_before);
        need(encode(*placement,bytes,error),error);const auto original=mt2mp::sha256(bytes);
        need(canonical(*placement,actual)&&actual==original,"New Place actor/counter was projected as a loaded actor");++n;
        ++placement->evidence.monster_after.actor.tag;need(!canonical(*placement,actual)&&actual.empty(),"Place serial not assigned by original Spawn was accepted");++n;
    }
}
