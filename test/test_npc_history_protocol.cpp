#include "typed_action/action_session.h"
#include "building_history_slice/cash_effect.h"
#include "npc_history_budget.h"
#include <iostream>
#include <memory>
#include <stdexcept>

namespace npc_history_protocol_fixture {
namespace a=mt2mp::action_session;
unsigned checks=0;
void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
constexpr const char* source_id="11111111-1111-4111-8111-111111111111";
a::NpcActor actor(int32_t type){a::NpcActor value;value.name="Original NPC";value.type=type;value.character_slot=3;value.character_type=2;value.level=7;value.quest_count=0;
    value.position={1,2,3};value.rotation={0,0,0,1};value.scale={1,1,1};
    if(type==0){mt2mp::npc_quest_records::Record row;row.sequence_id=0;row.type=1;row.name="Original quest";row.description="Original text";
        row.action=2;row.destination_key="action:55555555-5555-4555-8555-555555555555";row.xp_reward=23;row.gold_reward=7;
        row.accepted={{4,3},2,9};row.completed={{1,2},1,4};row.rating={.5f,.25f,.75f,true};value.quests.push_back(row);value.quest_count=1;}
    return value;
}
std::unique_ptr<a::Message> result(uint8_t original,int32_t type,bool undo){
    auto message=std::make_unique<a::Message>();auto& m=*message;m.kind=a::Kind::HostResult;m.session=5;m.revision=1;m.required_mask=3;m.outcome=a::Outcome::Confirmed;
    auto& request=m.request;request.action=undo?a::Action::UndoBuilding:a::Action::RedoBuilding;request.peer=2;request.sequence=1;
    request.action_uuid="22222222-2222-4222-8222-222222222222";request.snapshot_uuid="33333333-3333-4333-8333-333333333333";
    request.target_action_uuid=source_id;request.canonical_record_key="npc-history:"+request.target_action_uuid;
    auto& e=m.evidence;e.undo_confirmed=e.retirement_confirmed=true;e.npc_history_proof.emplace();auto& proof=*e.npc_history_proof;
    proof.origin.action=original;proof.origin.region=7;proof.origin.key="action:"+std::string(source_id);
    if(original!=15)proof.origin.before=actor(type);if(original!=16)proof.origin.after=actor(type);
    if(original==17)proof.origin.after.position={7,8,9};
    proof.quests_before=proof.quests_after=std::string(64,'a');proof.references_before=proof.references_after=std::string(64,'b');
    proof.count_before=proof.count_after=2;proof.completed_before=proof.completed_after=1;
    const bool restoring=original!=17&&((original==16)==undo),removing=original!=17&&!restoring;
    auto& before=e.npc_before;before.count=4;before.undo_count=3;before.roster_sha256=std::string(64,'c');before.global_random={1,2,3,4};
    before.created=restoring?a::NpcActor{}:(undo?proof.origin.after:proof.origin.before);
    if(type==0)before.quest_targets_sha256=std::string(64,'d');
    auto& after=e.npc_after;after=before;after.undo_count=undo?2:4;after.count=restoring?5:removing?3:4;
    after.created=removing?a::NpcActor{}:(undo?proof.origin.before:proof.origin.after);e.target_retired=removing;
    const std::array<int32_t,3> costs{1000,100,500};proof.cash=original==15?(undo?costs[size_t(type)]:-costs[size_t(type)]):0;
    e.before.cash=10000;e.before.flat_day=1;e.before.record_present=1;e.before.record_day=1;e.before.record_cash=10000;
    std::string error;need(mt2mp::game::building_history_slice::cash_effect(e.before,proof.cash,e.after,error),error);
    return message;
}
void run(){std::string error;std::vector<uint8_t> wire;auto decoded=std::make_unique<a::Message>();
    for(uint8_t original:{uint8_t(15),uint8_t(16),uint8_t(17)})for(int32_t type=0;type<3;++type)for(bool undo:{false,true}){
        auto current=result(original,type,undo);
        need(a::encode(*current,wire,error)&&a::decode(wire.data(),wire.size(),*decoded,error)&&*decoded==*current,error);
        const auto raw=nlohmann::json::parse(wire);need(raw.at("schema")==36,"NPC history did not use schema36");
        a::Context context{5,1,1,3,a::Role::Host,current->request.snapshot_uuid};const auto& proof=*current->evidence.npc_history_proof;
        need(mt2mp::game::npc_history_budget::preflight(context,current->request,proof.origin,current->evidence.npc_before,current->evidence.before,proof.cash,proof.count_before,proof.completed_before,error),error);
        auto pending=std::make_unique<a::Message>(*current);pending->kind=a::Kind::Request;pending->revision=0;pending->required_mask=0;pending->outcome=a::Outcome::Pending;
        auto empty=std::make_unique<a::Evidence>();pending->evidence=std::move(*empty);
        need(a::encode(*pending,wire,error)&&a::decode(wire.data(),wire.size(),*decoded,error)&&*pending==*decoded,error);
        const auto reject=[&](auto mutate){auto bad=std::make_unique<a::Message>(*current);mutate(*bad);need(!a::encode(*bad,wire,error)&&!error.empty(),"invalid NPC original history encoded");};
        reject([](auto& bad){bad.request.target_action_uuid=bad.request.action_uuid;});
        reject([](auto& bad){bad.request.canonical_record_key+="x";});
        reject([](auto& bad){bad.evidence.npc_history_proof.reset();});
        reject([](auto& bad){bad.evidence.npc_history_proof->origin.action=18;});
        reject([](auto& bad){bad.evidence.npc_history_proof->quests_after="bad";});
        reject([](auto& bad){bad.evidence.npc_after.count+=2;});
        reject([](auto& bad){bad.evidence.npc_after.undo_count=3;});
        reject([](auto& bad){bad.evidence.after.cash++;});
        reject([](auto& bad){bad.evidence.npc_history_proof->cash++;});
        reject([](auto& bad){bad.evidence.undo_confirmed=false;});
        reject([](auto& bad){bad.evidence.selection_confirmed=true;});
        reject([](auto& bad){bad.evidence.target_retired=!bad.evidence.target_retired;});
        reject([](auto& bad){bad.evidence.property_before.content.id=56;});
        for(unsigned mutation=0;mutation<5;++mutation){auto malformed=raw;
            if(mutation==0)malformed["schema"]=35;if(mutation==1)malformed["request"]["action"]=16;
            if(mutation==2)malformed["evidence"]["npc_history"]["extra"]=0;
            if(mutation==3)malformed["evidence"]["npc_history"]["origin"]["uid"]=123;
            if(mutation==4)malformed["request"]["extra"]=0;
            const auto payload=malformed.dump();need(!a::decode(reinterpret_cast<const uint8_t*>(payload.data()),payload.size(),*decoded,error),"wrong NPC schema/action/extra field accepted");
        }
    }
    auto large=result(15,0,true);auto& proof=*large->evidence.npc_history_proof;proof.origin.after.quests[0].description=std::string(50000,'x');
    large->evidence.npc_before.created=proof.origin.after;
    a::Context context{5,1,1,3,a::Role::Host,large->request.snapshot_uuid};
    need(!mt2mp::game::npc_history_budget::preflight(context,large->request,proof.origin,large->evidence.npc_before,large->evidence.before,proof.cash,proof.count_before,proof.completed_before,error)&&error.find("wire")!=std::string::npos,"oversized known History result was not rejected before original");
}
}
int main()try{npc_history_protocol_fixture::run();std::cout<<npc_history_protocol_fixture::checks<<" complete NPC history protocol checks passed\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
