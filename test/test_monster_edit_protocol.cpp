#include "typed_action/action_session.h"
#include "monster_move_key.h"
#include <iostream>
#include <memory>
#include <stdexcept>
namespace fixture {
using namespace mt2mp::action_session;
constexpr const char* id="11111111-1111-4111-8111-111111111111";
constexpr const char* source="22222222-2222-4222-8222-222222222222";
constexpr const char* snapshot="33333333-3333-4333-8333-333333333333";
unsigned checks=0;
void need(bool ok,const std::string& message){++checks;if(!ok)throw std::runtime_error(message);}
MonsterActor actor(int32_t rank=0){return {7,3,rank,1,4,3,"Ordinary monster",{1,2,3},{1,1,1},{0,0,0,1}};}
std::unique_ptr<Message> history(bool remove,bool undo){
    auto out=std::make_unique<Message>();auto& m=*out;auto& r=m.request;auto& e=m.evidence;
    m.kind=Kind::HostResult;m.session=5;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    r.action=undo?Action::UndoBuilding:Action::RedoBuilding;r.peer=2;r.sequence=1;r.action_uuid=id;r.snapshot_uuid=snapshot;
    r.target_action_uuid=source;r.canonical_record_key=std::string("monster-history:")+source;
    e.before.cash=e.before.record_cash=5000;e.before.record_present=1;e.before.flat_day=e.before.record_day=2;e.after=e.before;
    MonsterHistoryProof proof;proof.origin={uint8_t(remove?30:29),std::string("action:")+source,"world",63,actor(),actor()};
    proof.origin.after.position={4,5,6};if(remove)proof.origin.after={};
    proof.quests_before=proof.quests_after=proof.references_before=proof.references_after=std::string(64,'a');
    proof.count_before=proof.count_after=2;proof.completed_before=proof.completed_after=1;
    e.monster_history_proof=proof;e.monster_before.roster={0,2,undo?2u:1u,10,4,{1,2,3,4},{5,6,7,8},{},std::string(64,'b')};
    e.monster_after.roster=e.monster_before.roster;e.monster_after.roster.undo_count=undo?1:2;
    e.monster_before.actor=undo?proof.origin.after:proof.origin.before;e.monster_after.actor=undo?proof.origin.before:proof.origin.after;
    if(remove){++e.monster_after.roster.created_monsters;e.monster_after.roster.monsters=undo?3:1;
        if(undo)++e.monster_after.roster.next_tag;e.target_retired=!undo;}
    e.monster_after.roster.content_sha256=std::string(64,'c');e.undo_confirmed=e.retirement_confirmed=true;return out;
}
}
int main()try{using namespace fixture;std::string error;std::vector<uint8_t> bytes;auto decoded=std::make_unique<Message>();
    for(int rank=0;rank<=2;++rank)for(bool remove:{false,true})for(bool undo:{false,true}){
        auto packet=history(remove,undo);auto& e=packet->evidence;e.monster_history_proof->origin.before.rank=rank;
        if(!remove)e.monster_history_proof->origin.after.rank=rank;
        if(e.monster_before.actor!=MonsterActor{})e.monster_before.actor.rank=rank;if(e.monster_after.actor!=MonsterActor{})e.monster_after.actor.rank=rank;
        need(encode(*packet,bytes,error),error);need(decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==*packet,error);
        const auto wire=nlohmann::json::parse(bytes);need(wire.at("schema")==36,"Monster history must use schema36");
        const auto reject=[&](nlohmann::json value){const auto text=value.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),*decoded,error),"Invalid Monster history payload accepted");};
        auto bad=wire;bad["schema"]=35;reject(bad);bad=wire;bad["request"]["action"]=66;reject(bad);
        bad=wire;bad["evidence"]["monster_history"]["origin"]["action"]=28;reject(bad);
        bad=wire;bad["evidence"]["monster_history"]["origin"]["key"]="native:123";reject(bad);
        bad=wire;bad["evidence"]["monster_history"]["foreign"]=true;reject(bad);
        bad=wire;bad["evidence"]["monster_history"]["count_before"]=-1;reject(bad);
        bad=wire;bad["evidence"]["monster_history"]["origin"]["region"]=1.25;reject(bad);
        auto invalid=std::make_unique<Message>(*packet);++invalid->evidence.after.line_counts[19];need(!encode(*invalid,bytes,error),"History changed full Ledger");
        *invalid=*packet;++invalid->evidence.monster_after.roster.region_random[0];need(!encode(*invalid,bytes,error),"History changed Region RNG");
        *invalid=*packet;invalid->evidence.monster_history_proof.reset();need(!encode(*invalid,bytes,error),"Confirmed history lacks source proof");
        *invalid=*packet;invalid->evidence.monster_after.actor=actor(3);need(!encode(*invalid,bytes,error),"Unproved Monster rank admitted");
    }
    for(int32_t rank=0;rank<=2;++rank){auto packet=history(false,false);auto& r=packet->request;auto& e=packet->evidence;
        r.action=Action::MoveMonster;r.canonical_record_key=std::string("action:")+source;r.monster_move={63,{4,5,6}};
        e.monster_history_proof.reset();e.monster_before.actor=actor(rank);e.monster_after.actor=e.monster_before.actor;e.monster_after.actor.position=r.monster_move.position;
        e.selection_confirmed=true;need(encode(*packet,bytes,error),error);need(decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==*packet,error);
        auto wire=nlohmann::json::parse(bytes);wire["schema"]=31;const auto old=wire.dump();
        need(decode(reinterpret_cast<const uint8_t*>(old.data()),old.size(),*decoded,error)==(rank>0),"Legacy31 rank contract changed");
        r.target_action_uuid.clear();r.canonical_record_key=std::string("action:")+source+":monster:2";
        need(encode(*packet,bytes,error)&&decode(bytes.data(),bytes.size(),*decoded,error)&&*decoded==*packet,"Generated Move key lost: "+error);
        wire=nlohmann::json::parse(bytes);wire["schema"]=31;const auto generated=wire.dump();
        need(!decode(reinterpret_cast<const uint8_t*>(generated.data()),generated.size(),*decoded,error),"Legacy31 generated identity widened");
    }
    std::cout<<checks<<" ordinary Monster/history protocol cases passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
