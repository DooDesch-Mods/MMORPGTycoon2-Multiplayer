#include "monster_remove_fixture.h"
#include <iostream>
#include <memory>
int main(){try{std::string e;std::vector<uint8_t> bytes;auto output_storage=std::make_unique<Message>();auto& out=*output_storage;unsigned n=0;
 for(auto make:{&remove_request,&remove_result}){auto message_storage=std::make_unique<Message>(make());auto& m=*message_storage;need(encode(m,bytes,e),e);need(decode(bytes.data(),bytes.size(),out,e)&&out==m,"Remove schema36 roundtrip");++n;}
 auto m_storage=std::make_unique<Message>(remove_request());auto& m=*m_storage;need(encode(m,bytes,e),e);const auto raw=nlohmann::json::parse(bytes);need(raw["schema"]==36,"Remove schema differs");
 auto bad=[&](nlohmann::json j){auto text=j.dump();need(!decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),out,e),"invalid Remove wire accepted");++n;};
 for(auto v:{nlohmann::json(-1),nlohmann::json(true),nlohmann::json(1.0),nlohmann::json("63")}){auto j=raw;j["request"]["monster_remove"]["region"]=v;bad(j);}
 {auto j=raw;j["schema"]=31;bad(j);} {auto j=raw;j["request"]["action"]=29;bad(j);}
 {auto j=raw;j["request"]["target_action_uuid"]=action_id;bad(j);} {auto j=raw;j["request"]["monster_remove"]["extra"]=0;bad(j);}
 auto invalid=[&](auto change){auto invalid_storage=std::make_unique<Message>(remove_result());auto& v=*invalid_storage;change(v);need(!encode(v,bytes,e),"invalid Remove evidence accepted");++n;};
 invalid([](Message& v){++v.evidence.after.line_counts[19];});invalid([](Message& v){++v.evidence.monster_after.roster.zones;});
 invalid([](Message& v){++v.evidence.monster_after.roster.next_tag;});invalid([](Message& v){--v.evidence.monster_after.roster.created_monsters;});
 invalid([](Message& v){++v.evidence.monster_after.roster.region_random[0];});invalid([](Message& v){--v.evidence.monster_after.roster.undo_count;});
 invalid([](Message& v){v.evidence.monster_after.actor=v.evidence.monster_before.actor;});
 invalid([](Message& v){v.evidence.target_retired=false;});invalid([](Message& v){v.evidence.selection_confirmed=false;});
 invalid([](Message& v){v.evidence.undo_confirmed=false;});invalid([](Message& v){v.evidence.retirement_confirmed=false;});
 invalid([](Message& v){v.request.monster_move.region=63;});invalid([](Message& v){v.request.monster=intent();});
 invalid([](Message& v){v.evidence.object_before.alive=true;});
 // Cleanup RNG and Release counters are original observed effects. The schema
 // transports them; the Client and ACK must match the authoritative values.
 auto mixed_storage=std::make_unique<Message>(remove_result());auto& mixed=*mixed_storage;mixed.evidence.monster_before.roster.zones=2;mixed.evidence.monster_before.roster.monsters=5;
 mixed.evidence.monster_after=removed(mixed.evidence.monster_before);mixed.evidence.monster_after.roster.release_edits[7]+=2;
 need(encode(mixed,bytes,e)&&decode(bytes.data(),bytes.size(),out,e)&&out==mixed,"mixed Zone/direct removal rejected");++n;
 auto capped_storage=std::make_unique<Message>(remove_result());auto& capped=*capped_storage;capped.evidence.monster_before.roster.undo_count=20;capped.evidence.monster_after.roster.undo_count=20;
 need(encode(capped,bytes,e)&&decode(bytes.data(),bytes.size(),out,e)&&out==capped,"original twenty-entry Undo history trimming rejected");++n;
 auto quest_storage=std::make_unique<Message>(remove_result());auto& quest=*quest_storage;auto& retirement=quest.evidence.monster_quests;
 retirement.before_count=4;retirement.after_count=1;retirement.completed_before=1;retirement.completed_after=0;
 retirement.before_sha256=std::string(64,'a');retirement.after_sha256=std::string(64,'b');
 mt2mp::npc_quest_records::Record record;record.sequence_id=0;record.name="First";record.action=2;
 record.destination_key=quest.request.canonical_record_key;record.completed.total=1;record.completed.history={1,2};
 retirement.removed.push_back({std::string("action:")+action_id,record});
 record.sequence_id=2;record.name="Second";record.completed.total=0;retirement.removed.push_back({std::string("action:")+action_id,record});
 record.sequence_id=0;record.name="Other owner";retirement.removed.push_back({std::string("action:")+snapshot_id,record});
 need(encode(quest,bytes,e)&&decode(bytes.data(),bytes.size(),out,e)&&out==quest,"matching target Quest records or order lost in codec");++n;
 const auto quest_wire=nlohmann::json::parse(bytes);
 {auto j=quest_wire;j["schema"]=32;bad(j);}
 {auto j=quest_wire;j["evidence"].erase("monster_quests");bad(j);}
 {auto j=quest_wire;j["evidence"]["monster_quests"]["removed"][0]["record"]["extra"]=1;bad(j);}
 {auto j=quest_wire;j["evidence"]["monster_quests"]["after_count"]=2;bad(j);}
 {auto j=quest_wire;j["evidence"]["monster_quests"]["completed_after"]=1;bad(j);}
 {auto j=quest_wire;j["evidence"]["monster_quests"]["removed"][1]["record"]["sequence_id"]=0;bad(j);}
 {auto j=quest_wire;j["evidence"]["monster_quests"]["removed"][0]["owner_key"]="local:123";bad(j);}
 {auto j=quest_wire;j["evidence"]["monster_quests"]["removed"][0]["record"]["destination_key"]=std::string("action:")+action_id;bad(j);}
 auto foreign_storage=std::make_unique<Message>(result());auto& foreign=*foreign_storage;foreign.evidence.monster_quests=retirement;need(!encode(foreign,bytes,e),"Quest retirement accepted on PlaceMonster");++n;
 std::cout<<n<<" actual Remove codec and transition cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
