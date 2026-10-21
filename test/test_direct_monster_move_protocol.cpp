#include "direct_monster_fixture.h"
#include <iostream>
constexpr const char* move_id="33333333-3333-4333-8333-333333333333";
Message moving(){auto m=request();m.request.action=Action::MoveMonster;m.request.action_uuid=move_id;
 m.request.target_action_uuid=action_id;m.request.monster={};m.request.monster_move={63,{8030,70.576828f,8560}};return m;}
Message moved(){auto m=moving();m.kind=Kind::HostResult;m.revision=1;m.required_mask=3;m.outcome=Outcome::Confirmed;
 auto& e=m.evidence;e.before=e.after=money();e.monster_before=born(empty());e.monster_after=e.monster_before;
 e.monster_after.actor.position=m.request.monster_move.position;++e.monster_after.roster.undo_count;
 e.monster_after.roster.content_sha256=std::string(64,'c');e.undo_confirmed=e.selection_confirmed=e.retirement_confirmed=true;return m;}
int main(){try{std::string e;std::vector<uint8_t> bytes;Message out;unsigned count=0;
 for(auto m:{moving(),moved()}){need(encode(m,bytes,e),e);need(decode(bytes.data(),bytes.size(),out,e)&&out==m,e);++count;}
 need(encode(moving(),bytes,e),e);auto raw=nlohmann::json::parse(bytes);need(raw["schema"]==36,"MoveMonster schema differs");
 auto bad=[&](nlohmann::json j){auto s=j.dump();need(!decode(reinterpret_cast<const uint8_t*>(s.data()),s.size(),out,e),"bad move wire accepted");++count;};
 for(auto value:{nlohmann::json(true),nlohmann::json(1.0),nlohmann::json(-1),nlohmann::json("1")}){auto j=raw;j["request"]["monster_move"]["region"]=value;bad(j);}
 {auto j=raw;j["schema"]=30;bad(j);} {auto j=raw;j["request"]["monster_move"]["extra"]=1;bad(j);}
 {auto j=raw;j["request"]["target_action_uuid"]=move_id;bad(j);} {auto j=raw;j["request"]["canonical_record_key"]=std::string("action:")+move_id;bad(j);}
 auto invalid=[&](auto change){auto m=moved();change(m);need(!encode(m,bytes,e),"invalid Move effect accepted");++count;};
 invalid([](Message& m){++m.evidence.after.cash;});invalid([](Message& m){++m.evidence.monster_after.roster.monsters;});
 invalid([](Message& m){++m.evidence.monster_after.roster.next_tag;});invalid([](Message& m){++m.evidence.monster_after.roster.created_monsters;});
 invalid([](Message& m){++m.evidence.monster_after.roster.region_random[0];});invalid([](Message& m){++m.evidence.monster_after.roster.release_edits[6];});
 invalid([](Message& m){--m.evidence.monster_after.roster.undo_count;});invalid([](Message& m){++m.evidence.monster_after.actor.tag;});
 invalid([](Message& m){++m.evidence.monster_after.actor.position[0];});invalid([](Message& m){m.evidence.selection_confirmed=false;});
 invalid([](Message& m){m.evidence.retirement_confirmed=false;});invalid([](Message& m){m.request.npc.type=1;});
 auto noop=moved();noop.request.monster_move.position=noop.evidence.monster_before.actor.position;
 noop.evidence.monster_after.actor=noop.evidence.monster_before.actor;noop.evidence.monster_after.roster.content_sha256=noop.evidence.monster_before.roster.content_sha256;
 need(encode(noop,bytes,e),e);++count;
 std::cout<<count<<" MoveMonster codec/transition cases passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
