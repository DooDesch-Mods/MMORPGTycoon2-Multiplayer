#include "direct_monster_fixture.h"
#include "monster_target_key.h"
namespace monster_target_key=mt2mp::game::monster_target_key;
#include "monster_move_read.h"
#include "move_kiss/move_kiss.h"
#include <iostream>
#include <cstring>
namespace a=mt2mp::action_session;namespace mr=mt2mp::game::monster_move_read;
namespace monster_zone_read=mt2mp::game::monster_zone_read;namespace d {
struct Undo{std::vector<uintptr_t> undo;uintptr_t active_graph=0;};
struct Sample{monster_zone_read::State state;a::Ledger money;Undo undo;};}
using json=nlohmann::json;namespace move_kiss=mt2mp::move_kiss;
constexpr const char* move_id="33333333-3333-4333-8333-333333333333";
constexpr const char* exe_sha="fixture";
namespace {
struct Memory{int process=0;};struct MonsterMoveMemory{explicit MonsterMoveMemory(Memory&){};};
struct World{uintptr_t root=100;int saves=0,edits=0;uintptr_t active_graph=0;};
struct Region{uintptr_t region=900;};struct Pin{World world;Region region;};
struct MoveOperation{a::Binding binding;a::Request request;Pin pin;a::Evidence evidence;uintptr_t pointer=0;uint64_t uid=0;move_kiss::Receipt receipt;mr::Actor actor_before,actor_after;bool confirmed=false,retired=false,seed_attempted=false;std::string cause;};
std::map<std::string,MoveOperation> moves;bool move_fault=false;json move_preflight=json::object();
struct Runtime{bool ready=true;unsigned thread=7;uintptr_t image=0x140000000;}state;
struct Model{d::Sample sample;a::MonsterState value;mr::Actor actor;std::string fault;unsigned calls=0,seeds=0;bool after=false,pending=true;}model;
unsigned GetCurrentThreadId(){return 7;}
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool gate(const char* name,std::string& e){return model.fault!=name||fail(e,std::string(name)+" failed");}
namespace typed_action_runtime{json controller_status(){return {{"pending",model.pending},{"action_uuid",move_id}};}}
bool move_code(Memory&,std::string& e){return gate("code",e);}
bool same_world(const World&,const World&){return model.fault!="owner";}
bool move_target(Memory&,const a::Binding&,const std::string&,int32_t,const std::string&,Pin&,d::Sample& sample,monster_zone_read::Monster& row,std::string& e){
 if(!gate(model.after?"after_lookup":"lookup",e))return false;sample=model.sample;row=sample.state.monsters[0];return true;}
uint32_t shard_region(uintptr_t,uintptr_t,const std::array<float,3>&,uintptr_t& out){out=model.fault=="region"?901:900;return 0;}
bool effect(Memory&,const Pin&,d::Sample& sample,a::MonsterState& value,std::string& e,uint64_t){
 if(!gate(model.after?"after_effect":"effect",e))return false;sample=model.sample;value=model.value;return true;}
bool move_actor(Memory&,uintptr_t,mr::Actor& out,std::string& e){out=model.actor;return gate("actor",e);}
a::Ledger ledger(const a::Ledger& v){return v;}
a::BuildingState move_pose(const monster_zone_read::Monster& row){a::BuildingState v;v.alive=true;v.x=row.position[0];v.y=row.position[1];v.z=row.position[2];v.rotation=row.rotation;v.scale=row.scale;return v;}
namespace undo_probe{struct GraphWitness{};
bool read_graph(int,uintptr_t,uintptr_t,GraphWitness&,std::string& e){return gate("graph",e);}
bool validate_undo_commit(const d::Undo&,const d::Undo&,const GraphWitness&,std::string& e){return gate("undo",e);}}
bool move_inverse(Memory&,uintptr_t,uint64_t,const a::MonsterActor&,std::string& e){return gate("inverse",e);}
bool move_selected(Memory&,const Pin&,uintptr_t,std::string& e){return gate("selected",e);}
namespace action_runtime{uint32_t seed(uintptr_t,const std::array<uint64_t,4>& v){++model.seeds;model.value.roster.global_random=model.sample.state.global_random=v;
 if(model.fault=="seed_region")++model.sample.state.region_random[0];return model.fault=="seed"?5:0;}}
}
namespace mt2mp::move_kiss {
bool resolve_calls(MonsterMoveMemory&,uintptr_t,const char*,Calls&,std::string& e){return gate("calls",e);}
bool character_transform(const State& value,std::array<unsigned char,0x6c>& out,std::string& e){out={};std::memcpy(out.data()+0x10,&value.x,4);std::memcpy(out.data()+0x14,&value.y,4);std::memcpy(out.data()+0x18,&value.z,4);return gate("transform",e);}
bool execute_once(const Calls&,uintptr_t,const std::array<unsigned char,0x6c>& pose,Receipt& receipt,std::string& e){
 ++model.calls;receipt.started=receipt.move_attempted=true;receipt.graph=500;model.after=true;
 if(!gate("native",e))return false;receipt.move_returned=true;
 auto& row=model.sample.state.monsters[0];std::memcpy(row.position.data(),pose.data()+0x10,12);model.value.actor.position=row.position;
 model.actor.spawn=row.position;++model.value.roster.undo_count;model.value.roster.content_sha256=std::string(64,'c');model.sample.undo.undo.push_back(500);
 if(model.fault=="uid")++row.uid;if(model.fault=="spawn")++model.actor.spawn[0];if(model.fault=="hidden")model.actor.visible=false;if(model.fault=="offmap")model.actor.on_map=false;
 if(model.fault=="tag"){++row.tag;++model.value.actor.tag;}if(model.fault=="counter")++model.value.roster.next_tag;
 if(model.fault=="release")++model.value.roster.release_edits[6];if(model.fault=="ledger")++model.sample.money.cash;
 if(model.fault=="region_rng")++model.value.roster.region_random[0];return true;
}
}
#include "monster_move_native_body.inc"
void reset(){moves.clear();move_fault=false;state={};model={};move_preflight=json::object();model.value=born(empty());model.sample.money=money();model.sample.undo.undo={400};
 model.sample.state.manager=700;model.sample.state.monster_manager=1000;model.sample.state.next_monster_tag=model.value.roster.next_tag;model.sample.state.created_monsters=model.value.roster.created_monsters;
 auto& actor=model.value.actor;monster_zone_read::Monster row;row.pointer=800;row.uid=0x100000001ull;row.tag=actor.tag;row.status=actor.status;row.rank=actor.rank;row.slot=actor.character_slot;row.type=actor.character_type;row.level=actor.level;row.position=actor.position;row.rotation=actor.rotation;row.scale=actor.scale;model.sample.state.monsters={row};
 model.sample.state.global_random=model.value.roster.global_random;model.actor={2000,3000,4000,true,true,row.position};}
a::Request moving(){auto r=request().request;r.action=Action::MoveMonster;r.action_uuid=move_id;r.target_action_uuid=action_id;r.monster={};r.monster_move={63,{8030,70.576828f,8560}};return r;}
int main(){try{unsigned checks=0;std::string e;const auto req=moving();const a::Binding binding{3,4};
 for(const char* fault:{"code","calls","lookup","region","effect","actor","transform"}){reset();model.fault=fault;need(move_perform(binding,req,nullptr,e)==a::Outcome::RejectedNoEffect&&!model.calls&&!e.empty(),std::string("pre-effect guard ")+fault);++checks;}
 for(const char* fault:{"native","after_lookup","owner","uid","after_effect","spawn","offmap","graph","undo","inverse","selected","release","ledger","region_rng","tag","counter"}){
  reset();model.fault=fault;need(move_perform(binding,req,nullptr,e)==a::Outcome::Unknown&&model.calls==1&&!e.empty(),std::string("unknown guard ")+fault+": "+e);
  need(move_perform(binding,req,nullptr,e)==a::Outcome::Unknown&&model.calls==1,"native failure replayed");++checks;}
 reset();model.actor.on_map=false;e.clear();need(move_perform(binding,req,nullptr,e)==a::Outcome::RejectedNoEffect&&!model.calls&&moves.empty(),"off-map target mutated");
 need(e.find("on_map=0 skeleton_visible=1")!=std::string::npos&&move_preflight.at("actor_read")==true&&move_preflight.at("on_map")==false&&move_preflight.at("local_uid")==uint64_t(0x100000001ull),"failed gate values missing");++checks;
 reset();model.actor.visible=false;e.clear();need(move_perform(binding,req,nullptr,e)==a::Outcome::Confirmed&&model.calls==1,e);
 need(move_preflight.at("on_map")==true&&move_preflight.at("skeleton_visible")==false&&move_preflight.at("skeleton_pointer")==4000,"local hidden preflight missing");
 a::Evidence hidden;need(move_observe(binding,req,hidden,e),e);model.actor.visible=true;need(move_observe(binding,req,hidden,e),"local visibility change invalidated ACK");++checks;
 reset();model.fault="hidden";e.clear();need(move_perform(binding,req,nullptr,e)==a::Outcome::Confirmed&&model.calls==1,e);need(move_retire(binding,req,e),e);++checks;
 reset();need(move_perform(binding,req,nullptr,e)==a::Outcome::Confirmed,e);a::Evidence host;need(move_observe(binding,req,host,e),e);need(move_retire(binding,req,e),e);need(move_observe(binding,req,host,e)&&host.retirement_confirmed,e);
 need(move_perform(binding,req,nullptr,e)==a::Outcome::Unknown&&model.calls==1,"confirmed move replayed");++checks;
 model.actor.on_map=false;need(!move_observe(binding,req,host,e),"ACK accepted changed map membership");++checks;
 model.actor.on_map=true;++model.actor.skeleton;need(!move_observe(binding,req,host,e),"ACK accepted replaced skeleton");++checks;
 reset();model.value.roster.region_random={1,2,3,4};model.sample.state.region_random=model.value.roster.region_random;
 need(move_perform(binding,req,&host,e)==a::Outcome::Confirmed&&model.calls==1&&model.seeds==1,e);++checks;
 reset();model.value.actor.tag=model.sample.state.monsters[0].tag=-11752269;
 model.value.roster.next_tag=model.sample.state.next_monster_tag=7;
 need(move_perform(binding,req,&host,e)==a::Outcome::Confirmed&&model.calls==1&&model.seeds==1,e);
 need(moves.at(move_id).evidence.monster_after.actor.tag==-11752269&&moves.at(move_id).evidence.monster_after.roster.next_tag==7,"Move replaced local serial/counter");++checks;
 for(const char* fault:{"seed","seed_region"}){reset();model.fault=fault;need(move_perform(binding,req,&host,e)==a::Outcome::Unknown&&!model.calls&&model.seeds==1,e);need(move_perform(binding,req,&host,e)==a::Outcome::Unknown&&model.seeds==1,"seed replayed");++checks;}
 std::cout<<checks<<" actual MoveMonster adapter body cases passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
