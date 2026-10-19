#include "typed_action/action_session.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace mt2mp::action_session;
void need(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
Message sample(NpcPropertyKind kind,int32_t online=1){
    Message m;m.session=7;m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
    auto& r=m.request;r.action=Action::EditNpcProperty;r.peer=2;r.sequence=1;
    r.action_uuid="11111111-1111-4111-8111-111111111111";r.snapshot_uuid="22222222-2222-4222-8222-222222222222";
    r.canonical_record_key="action:33333333-3333-4333-8333-333333333333";r.npc_property.kind=kind;
    r.npc_property.name=kind==NpcPropertyKind::Name?"New name":"";r.npc_property.online=kind==NpcPropertyKind::OnlineState?online:0;
    auto& e=m.evidence;e.retirement_confirmed=true;e.npc_property_before=NpcPropertyState{};
    auto& a=*e.npc_property_before;a.online=2;a.force_tint=false;a.tint={1,1,1,1};
    a.command_level=5;a.region_min=1;a.region_max=10;
    a.actor.count=2;a.actor.roster_sha256=std::string(64,'a');a.actor.created.name="Old name";
    a.actor.created.type=1;a.actor.created.character_slot=0;a.actor.created.character_type=1;
    a.actor.created.level=1;a.actor.created.quest_count=0;a.actor.created.rotation={0,0,0,1};a.actor.created.scale={1,1,1};
    e.npc_property_after=a;auto& b=*e.npc_property_after;
    if(kind==NpcPropertyKind::Name){b.actor.created.name=r.npc_property.name;b.actor.roster_sha256=std::string(64,'b');}
    else if(kind==NpcPropertyKind::OnlineState){b.online=online;b.force_tint=online!=2;b.tint={0.5f,0.25f,0.75f,1};}
    else b.command_level+=kind==NpcPropertyKind::RaiseLevel?1:-1;
    return m;
}
int main(){try{unsigned count=0;std::string error;std::vector<uint8_t> bytes;Message decoded;
    for(auto kind:{NpcPropertyKind::Name,NpcPropertyKind::OnlineState,NpcPropertyKind::RaiseLevel,NpcPropertyKind::LowerLevel}){
        auto result=sample(kind);need(encode(result,bytes,error),error);need(decode(bytes.data(),bytes.size(),decoded,error),error);
        need(decoded==result,"NPC property result roundtrip");++count;
        auto req=result;req.kind=Kind::Request;req.revision=0;req.required_mask=0;req.outcome=Outcome::Pending;req.evidence={};
        need(encode(req,bytes,error),error);need(decode(bytes.data(),bytes.size(),decoded,error),error);need(decoded==req,"request roundtrip");++count;
        auto reject=[&](auto change){auto bad=result;change(bad);need(!encode(bad,bytes,error),"invalid property effect accepted");++count;};
        reject([](auto& m){++m.evidence.after.cash;});
        reject([](auto& m){++m.evidence.npc_property_after->actor.global_random[0];});
        reject([](auto& m){++m.evidence.npc_property_after->actor.undo_count;});
        reject([](auto& m){++m.evidence.npc_property_after->actor.count;});
        reject([](auto& m){m.evidence.npc_property_after->actor.created.scale[0]=2;});
        reject([](auto& m){m.evidence.npc_property_after->actor.created.level=2;});
        reject([](auto& m){m.evidence.npc_property_after->tint[0]=std::numeric_limits<float>::infinity();});
        reject([](auto& m){m.evidence.undo_confirmed=true;});
        reject([](auto& m){m.evidence.npc_property_after.reset();});
        reject([](auto& m){m.request.npc.type=2;});
        reject([](auto& m){m.request.npc_property.online=3;});
        reject([](auto& m){m.request.canonical_record_key="region:67";});
        reject([](auto& m){++m.evidence.npc_property_after->region_max;});
        reject([](auto& m){m.evidence.npc_property_after->command_level=-1;});
        need(encode(req,bytes,error),error);auto raw=nlohmann::json::parse(bytes);raw["request"]["npc_property"]["pointer"]=1234;
        auto text=raw.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),decoded,error),"unknown property payload accepted");++count;
    }
    auto enabled=sample(NpcPropertyKind::OnlineState);enabled.request.npc_property.online=2;
    enabled.evidence.npc_property_before->online=1;enabled.evidence.npc_property_before->force_tint=true;
    enabled.evidence.npc_property_after=*enabled.evidence.npc_property_before;
    enabled.evidence.npc_property_after->online=2;enabled.evidence.npc_property_after->force_tint=false;
    need(encode(enabled,bytes,error),error);++count;
    enabled.evidence.npc_property_after->tint[0]=0.125f;need(!encode(enabled,bytes,error),"online2 original retained tint differs");++count;
    auto giver=sample(NpcPropertyKind::Name);mt2mp::npc_quest_records::Record quest;quest.sequence_id=0;
    quest.destination_key="action:44444444-4444-4444-8444-444444444444";quest.accepted.history={1,2};quest.completed.history={0,1};
    for(auto* row:{&*giver.evidence.npc_property_before,&*giver.evidence.npc_property_after}){
        row->actor.created.type=0;row->actor.created.quest_count=1;row->actor.created.quests={quest};
        row->actor.quest_targets_sha256=std::string(64,'c');}
    need(encode(giver,bytes,error),error);
    auto quest_packet=nlohmann::json::parse(bytes);
    const auto depth=[](const auto& self,const nlohmann::json& value,size_t level)->size_t{
        size_t maximum=level;if(value.is_structured())for(const auto& child:value)maximum=std::max(maximum,self(self,child,level+1));return maximum;};
    need(depth(depth,quest_packet,0)==9,"complete NPC Quest history fixture depth differs");++count;
    need(decode(bytes.data(),bytes.size(),decoded,error),error);
    need(giver==decoded,"QuestGiver full Quest nested roundtrip");++count;
    auto& history_value=quest_packet["evidence"]["npc_property_before"]["actor"]["created"]["quests"][0]["accepted"]["history"][0];
    history_value=nlohmann::json::array({history_value});
    const auto too_deep=quest_packet.dump();
    need(!decode(reinterpret_cast<const uint8_t*>(too_deep.data()),too_deep.size(),decoded,error)&&error.find("nesting")!=std::string::npos,"NPC Quest history accepted depth10");++count;
    ++giver.evidence.npc_property_after->actor.created.quests[0].gold_reward;
    need(!encode(giver,bytes,error),"unrelated Quest mutation accepted");++count;
    for(auto kind:{NpcPropertyKind::RaiseLevel,NpcPropertyKind::LowerLevel}){
        auto level=sample(kind);level.evidence.npc_property_after->command_level=level.evidence.npc_property_before->command_level;
        need(!encode(level,bytes,error),"missing original command effect accepted");++count;
        level=sample(kind);auto& a=*level.evidence.npc_property_before;
        a.command_level=kind==NpcPropertyKind::RaiseLevel?a.region_max:a.region_min;level.evidence.npc_property_after=a;
        need(!encode(level,bytes,error),"clamped no-op accepted");++count;
        level=sample(kind);level.request.npc_property.name="Raise Level";
        need(!encode(level,bytes,error),"level intent accepted unrelated name payload");++count;
    }
    std::cout<<count<<" actual NPC property codec cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
