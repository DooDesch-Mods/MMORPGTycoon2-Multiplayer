#include "npc_quest_authoring_fixture.h"
#include <iostream>
int main(){try{unsigned n=0;std::string e;std::vector<uint8_t> bytes;Message out;
 for(auto value:{request(),result()}){need(encode(value,bytes,e),e);need(decode(bytes.data(),bytes.size(),out,e)&&out==value,"Quest schema34 full record roundtrip");++n;}
 need(npc_quest_transition(intent(),before(),after(before())),"original Quest edit transition rejected");++n;
 auto capped=before();capped.undo_count=20;need(npc_quest_transition(intent(),capped,after(capped)),"original capped Undo rejected");++n;
 need(encode(request(),bytes,e),e);auto raw=nlohmann::json::parse(bytes);need(raw["schema"]==34&&raw["request"]["action"]==31,"Quest action/schema differ");
 auto bad=[&](nlohmann::json value){const auto text=value.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),out,e),"invalid Quest wire accepted");++n;};
 for(const auto field:{"region","destination_region","action","kills","monster_type"}){auto v=raw;v["request"]["npc_quest"][field]=true;bad(v);}
 for(const auto field:{"before","family","destination_key"}){auto v=raw;v["request"]["npc_quest"].erase(field);bad(v);}
 {auto v=raw;v["schema"]=33;bad(v);} {auto v=raw;v["request"]["npc_quest"]["pointer"]=123;bad(v);}
 {auto v=raw;v["request"]["npc_quest"]["before"]["extra"]=0;bad(v);}
 {auto v=raw;v["request"]["npc_quest"]["destination_key"]="local:123";bad(v);}
 auto invalid=[&](auto change){auto v=result();change(v);need(!encode(v,bytes,e),"changed unrelated Quest field/evidence accepted");++n;};
 invalid([](auto& v){++v.evidence.quest_after.record.sequence_id;});invalid([](auto& v){++v.evidence.quest_after.record.type;});
 invalid([](auto& v){v.evidence.quest_after.record.description="Replaced";});invalid([](auto& v){++v.evidence.quest_after.record.xp_reward;});
 invalid([](auto& v){++v.evidence.quest_after.record.gold_reward;});invalid([](auto& v){++v.evidence.quest_after.record.accepted.total;});
 invalid([](auto& v){++v.evidence.quest_after.record.completed.today;});invalid([](auto& v){v.evidence.quest_after.record.rating.beauty=.9f;});
 invalid([](auto& v){++v.evidence.quest_after.count;});invalid([](auto& v){++v.evidence.quest_after.completed;});
 invalid([](auto& v){v.evidence.quest_after.undo_count=0;});invalid([](auto& v){v.evidence.quest_after.record.destination_key=owner_key;});
 invalid([](auto& v){++v.evidence.after.cash;});invalid([](auto& v){v.evidence.undo_confirmed=false;});
 invalid([](auto& v){v.evidence.retirement_confirmed=false;});invalid([](auto& v){v.request.monster_remove.region=67;});
 invalid([](auto& v){v.evidence.monster_quests.before_count=1;});
 auto noop=intent();noop.before.destination_key=noop.destination_key;noop.before.action=noop.action;noop.before.monsters_to_kill=noop.kills;
 need(!npc_quest_record_transition(noop,noop.before),"original no-op published as edit");++n;
 for(auto family:{QuestDestinationFamily::Building,QuestDestinationFamily::Zone,QuestDestinationFamily::DirectMonster,QuestDestinationFamily::NPC}){
  auto value=result();auto& i=value.request.npc_quest;i.family=family;i.monster_type=0;
  i.destination_key=family==QuestDestinationFamily::NPC?std::string(64,'a')+":world:npc:36":target_key;
  i.action=family==QuestDestinationFamily::NPC?7:family==QuestDestinationFamily::Zone?1:family==QuestDestinationFamily::DirectMonster?6:2;
  i.kills=family==QuestDestinationFamily::Zone?8:family==QuestDestinationFamily::DirectMonster?1:0;
  auto& row=value.evidence.quest_after.record;row.destination_key=i.destination_key;row.action=i.action;
  row.monsters_to_kill=i.kills;row.monster_type=i.monster_type;
  need(encode(value,bytes,e)&&decode(bytes.data(),bytes.size(),out,e)&&out==value,"closed four-family schema34 receipt roundtrip: "+e);++n;
  auto broken=value;broken.request.npc_quest.action=family==QuestDestinationFamily::NPC?6:7;
  need(!encode(broken,bytes,e),"cross-family action7 admitted");++n;
  if(family==QuestDestinationFamily::NPC){
   auto raw_key=value;raw_key.request.npc_quest.destination_key="local:123";
   need(!encode(raw_key,bytes,e),"NPC target accepted local pointer identity");++n;
   auto counts=value;++counts.request.npc_quest.kills;need(!encode(counts,bytes,e),"NPC target accepted kill count");++n;
   auto monster=value;++monster.request.npc_quest.monster_type;need(!encode(monster,bytes,e),"NPC target accepted Monster type");++n;
   need(encode(value,bytes,e),e);auto wire=nlohmann::json::parse(bytes);
   need(wire["schema"]==34&&wire["request"]["npc_quest"]["family"]==4,"NPC target changed schema or reused existing family value");++n;
   wire["request"]["npc_quest"]["family"]=5;bad(wire);
  }
 }
 std::cout<<n<<" actual Quest codec and exact row transition cases passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
