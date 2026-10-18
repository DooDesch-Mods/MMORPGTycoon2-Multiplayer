#include "direct_monster_fixture.h"
#include <iostream>
int main(){try{std::string e;std::vector<uint8_t> bytes;Message out;unsigned count=0;
 for(auto m:{request(),result()}){need(encode(m,bytes,e),e);need(decode(bytes.data(),bytes.size(),out,e),e);need(out==m,"Monster exact schema30 roundtrip");++count;}
 auto m=request();need(encode(m,bytes,e),e);auto raw=nlohmann::json::parse(bytes);need(raw["schema"]==30,"Monster schema is not30");
 auto bad=[&](nlohmann::json j){auto text=j.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),out,e),"bad Monster wire accepted");++count;};
 for(auto v:{nlohmann::json(true),nlohmann::json(1.0),nlohmann::json(-1),nlohmann::json("1")}){auto j=raw;j["request"]["monster"]["region"]=v;bad(j);}
 {auto j=raw;j["schema"]=25;bad(j);} {auto j=raw;j["request"]["action"]=23;bad(j);} {auto j=raw;j["request"]["monster"]["extra"]=0;bad(j);}
 {auto j=raw;j["request"]["monster"]["rotation"]={0,0,0,0};bad(j);} {auto j=raw;j["request"]["monster"]["scale"][0]=0;bad(j);}
 {auto j=raw;j["request"]["monster"]["character_id"][0]=0;bad(j);} {auto j=raw;j["request"]["monster"]["level"]=0;bad(j);}
 {auto j=raw;j["request"]["monster"]["loose"]=1;bad(j);} {auto j=raw;j["request"]["monster"]["tool"]="item_monster";bad(j);}
 auto invalid=[&](auto change){auto v=result();change(v);need(!encode(v,bytes,e),"invalid Monster effect accepted");++count;};
 invalid([](Message& v){++v.evidence.after.cash;});invalid([](Message& v){++v.evidence.monster_after.roster.zones;});
 invalid([](Message& v){++v.evidence.monster_after.roster.next_tag;});invalid([](Message& v){++v.evidence.monster_after.roster.created_monsters;});
 invalid([](Message& v){++v.evidence.monster_after.roster.region_random[0];});invalid([](Message& v){++v.evidence.monster_after.roster.release_edits[5];});
 invalid([](Message& v){v.evidence.monster_after.roster.release_edits[6]=0;});invalid([](Message& v){v.evidence.monster_after.roster.undo_count=0;});
 invalid([](Message& v){v.evidence.monster_after.actor.name.clear();});invalid([](Message& v){++v.evidence.monster_after.actor.level;});
 invalid([](Message& v){++v.evidence.monster_after.actor.position[0];});invalid([](Message& v){++v.evidence.monster_after.actor.tag;});
 invalid([](Message& v){v.request.npc.type=1;});invalid([](Message& v){v.evidence.retirement_confirmed=false;});
 invalid([](Message& v){v.evidence.zone_after.content_sha256=std::string(64,'c');});
 auto elite=result();elite.request.monster.scale={2,2,2};
 need(encode(elite,bytes,e),e);need(decode(bytes.data(),bytes.size(),out,e)&&out==elite,"Elite preview scale did not survive protocol roundtrip");++count;
 elite.evidence.monster_after.actor.scale={2,2,2};
 need(!encode(elite,bytes,e),"preview scale accepted as spawned Prop scale");++count;
 auto mixed=result();mixed.evidence.monster_before.roster.zones=2;mixed.evidence.monster_before.roster.monsters=5;
 mixed.evidence.monster_before.roster.next_tag=mixed.evidence.monster_before.roster.created_monsters=5;
 mixed.evidence.monster_after=born(mixed.evidence.monster_before);need(encode(mixed,bytes,e),e);need(decode(bytes.data(),bytes.size(),out,e)&&out==mixed,"mixed Zone/direct state rejected");++count;
 std::cout<<count<<" Monster codec/transition failure cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
