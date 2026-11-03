#include "direct_monster_fixture.h"
#include "typed_action/tick_evidence.h"
#include <iostream>
#include "saved_monster_move_retained.inc"
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
 auto canonical=[&](const Message& m,std::string& digest){
  namespace tick=mt2mp::action_session::tick_commands;
  Context context;context.session=m.session;context.snapshot_uuid=m.request.snapshot_uuid;context.peer_mask=static_cast<uint8_t>(m.required_mask);
  tick::Assigned assigned{m.revision,{m.session,m.request.snapshot_uuid,m.request}};
  tick::ExecutionReceipt receipt;receipt.command=assigned;receipt.outcome=Outcome::Confirmed;receipt.native_attempted=true;receipt.evidence=std::make_shared<Evidence>(m.evidence);
  return tick::canonical_evidence(context,assigned,receipt,digest,e);
 };
 const auto retained=std::string(retained_move);Message host;
 need(decode(reinterpret_cast<const uint8_t*>(retained.data()),retained.size(),host,e),e);
 auto client=host;client.evidence.monster_before.actor.tag=client.evidence.monster_after.actor.tag=-1010040832;
 need(encode(host,bytes,e),e);const auto raw_host=mt2mp::sha256(bytes);
 need(raw_host=="8e8e6c7411627fea36f1cca725635d4e071a86916aa44721632ad67845ee4c72","retained Host original codec hash differs: "+raw_host);++count;
 need(encode(client,bytes,e),e);const auto raw_client=mt2mp::sha256(bytes);
 need(raw_client=="0449bc6fd2971e6a18d23db577dd777e28d7380a64b6d97763055c6ffb0bec0c","retained Client original codec hash differs: "+raw_client);++count;
 std::string expected,actual;need(canonical(host,expected)&&canonical(client,actual)&&actual==expected,e);++count;
 need(expected==raw_host,"zero-serial Host canonical bytes changed");++count;
 for(int32_t serial:{INT32_MIN,-1,0,INT32_MAX}){
  auto local=host;local.evidence.monster_before.actor.tag=local.evidence.monster_after.actor.tag=serial;const auto original=local;
  need(canonical(local,actual)&&actual==expected,"local serial became canonical identity");
  need(local==original&&encode(local,bytes,e)&&decode(bytes.data(),bytes.size(),out,e)&&out==local,"canonical projection changed raw receipt or codec");++count;
 }
 auto distinct=[&](auto change){auto local=host;change(local);need(!canonical(local,actual)||actual!=expected,"real shared evidence became identical");++count;};
 distinct([](auto& m){m.request.monster_move.position[0]+=1;m.evidence.monster_after.actor.position[0]+=1;});
 distinct([](auto& m){m.request.monster_move.rotation={0,0,0,1};m.evidence.monster_after.actor.rotation={0,0,0,1};});
 distinct([](auto& m){m.evidence.monster_before.actor.name=m.evidence.monster_after.actor.name="Different";});
 distinct([](auto& m){++m.evidence.monster_before.actor.level;++m.evidence.monster_after.actor.level;});
 distinct([](auto& m){++m.evidence.monster_before.actor.character_type;++m.evidence.monster_after.actor.character_type;});
 distinct([](auto& m){++m.evidence.monster_before.roster.region_random[0];++m.evidence.monster_after.roster.region_random[0];});
 distinct([](auto& m){++m.evidence.monster_before.roster.global_random[0];++m.evidence.monster_after.roster.global_random[0];});
 distinct([](auto& m){m.evidence.monster_after.roster.content_sha256=std::string(64,'a');});
 distinct([](auto& m){++m.evidence.monster_before.roster.next_tag;++m.evidence.monster_after.roster.next_tag;});
 distinct([](auto& m){++m.evidence.before.cash;++m.evidence.after.cash;});
 distinct([](auto& m){m.request.canonical_record_key=std::string("action:")+action_id;m.request.target_action_uuid=action_id;});
 distinct([](auto& m){++m.request.monster_move.region;});
 auto reject_canonical=[&](auto change){auto local=host;change(local);need(!canonical(local,actual)&&actual.empty(),"invalid local transition became canonical success");++count;};
 reject_canonical([](auto& m){++m.evidence.monster_after.actor.tag;});
 reject_canonical([](auto& m){m.evidence.monster_before.actor={};});
 reject_canonical([](auto& m){++m.evidence.after.cash;});
 reject_canonical([](auto& m){++m.evidence.monster_after.roster.region_random[0];});
 reject_canonical([](auto& m){m.evidence.undo_confirmed=false;});
 reject_canonical([](auto& m){m.evidence.selection_confirmed=false;});
 reject_canonical([](auto& m){m.evidence.retirement_confirmed=false;});
 {auto placement=result();placement.evidence.monster_before.roster.next_tag=17;placement.evidence.monster_after=born(placement.evidence.monster_before);
  need(encode(placement,bytes,e),e);const auto raw_placement=mt2mp::sha256(bytes);
  need(canonical(placement,actual)&&actual==raw_placement,"unrelated placement canonical bytes changed");++count;}
 std::cout<<count<<" MoveMonster codec/transition cases passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
