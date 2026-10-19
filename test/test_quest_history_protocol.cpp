#include "npc_quest_list_fixture.h"
#include <iostream>
int main()try{
    unsigned cases=0;std::string error;std::vector<uint8_t> bytes;Message decoded;
    for(auto direction:{Action::UndoBuilding,Action::RedoBuilding})for(size_t old_count:{size_t{0},size_t{2},size_t{5}})for(size_t new_count:{size_t{0},size_t{1},size_t{5}}){
        Message m;m.kind=Kind::HostResult;m.session=1;m.revision=5;m.required_mask=3;m.outcome=Outcome::Confirmed;
        auto& r=m.request;r.action=direction;r.peer=2;r.sequence=1;r.action_uuid=action_id;r.target_action_uuid="44444444-4444-4444-8444-444444444444";
        r.snapshot_uuid=snapshot_id;r.canonical_record_key="quests:"+r.target_action_uuid;
        auto& value=m.evidence;value.undo_confirmed=value.retirement_confirmed=true;
        auto& prior=value.quest_list_before;prior.all_sha256=std::string(64,'a');prior.targets_sha256=std::string(64,'c');prior.references_sha256=std::string(64,'d');
        prior.undo_count=3;prior.count=uint32_t(old_count+3);prior.completed=int32_t(old_count+2);
        for(size_t i=0;i<old_count;++i)prior.records.push_back(record(int32_t(i)));
        auto& current=value.quest_list_after;current=prior;current.records.clear();
        for(size_t i=0;i<new_count;++i){auto row=record(int32_t(i));row.accepted={};row.completed={};row.rating={};current.records.push_back(row);}
        current.count=uint32_t(new_count+3);current.completed=2;current.all_sha256=std::string(64,'b');current.references_sha256=std::string(64,'e');
        current.undo_count=direction==Action::UndoBuilding?2:4;
        need(encode(m,bytes,error)&&decode(bytes.data(),bytes.size(),decoded,error)&&decoded==m,error);++cases;
        auto pending=m;pending.kind=Kind::Request;pending.revision=0;pending.required_mask=0;pending.outcome=Outcome::Pending;pending.evidence={};
        need(encode(pending,bytes,error)&&decode(bytes.data(),bytes.size(),decoded,error)&&decoded==pending,error);++cases;
        auto reject=[&](auto change){auto bad=m;change(bad);need(!encode(bad,bytes,error),"invalid Quest history accepted");++cases;};
        reject([](auto& bad){bad.request.target_action_uuid=bad.request.action_uuid;});
        reject([](auto& bad){bad.request.canonical_record_key+="x";});
        reject([](auto& bad){bad.evidence.quest_list_after.count++;});
        reject([](auto& bad){bad.evidence.quest_list_after.completed++;});
        reject([](auto& bad){bad.evidence.quest_list_after.undo_count=3;});
        reject([](auto& bad){bad.evidence.quest_list_after.targets_sha256=std::string(64,'f');});
        reject([](auto& bad){bad.evidence.after.cash++;});
        reject([](auto& bad){bad.evidence.undo_confirmed=false;});
        reject([](auto& bad){bad.evidence.target_retired=true;});
        reject([](auto& bad){bad.evidence.selection_confirmed=true;});
        reject([](auto& bad){bad.evidence.property_before.content.id=56;});
        if(new_count){
            reject([](auto& bad){bad.evidence.quest_list_after.records.front().completed.total=1;});
            reject([](auto& bad){bad.evidence.quest_list_after.records.front().accepted.today=1;});
            reject([](auto& bad){bad.evidence.quest_list_after.records.front().action=8;});
        }
        need(encode(m,bytes,error),error);auto raw=nlohmann::json::parse(bytes);need(raw["schema"]==35,"Quest history did not use schema35");
        raw["schema"]=10;const auto old=raw.dump();need(!decode(reinterpret_cast<const uint8_t*>(old.data()),old.size(),decoded,error),"Quest history accepted single object schema");++cases;
    }
    std::cout<<cases<<" Quest original whole-list history protocol cases passed\n";return 0;
}catch(const std::exception& failure){std::cerr<<failure.what()<<'\n';return 1;}
