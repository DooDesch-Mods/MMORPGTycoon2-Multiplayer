#include "direct_monster_fixture.h"
#include "monster_zone_read.h"
#include "direct_monster_identity.h"
#include <iostream>
#include <optional>
#include <cstring>
namespace a=mt2mp::action_session;namespace z=mt2mp::game::monster_zone_read;
namespace q=mt2mp::game::npc_quest_targets;namespace identity=mt2mp::game::direct_monster_identity;using json=nlohmann::json;
namespace {
struct Memory{int process=0;bool copy(uintptr_t from,void* to,size_t n,std::string&){std::memcpy(to,reinterpret_cast<const void*>(from),n);return true;}};struct World{uintptr_t root=100;};struct Pin{World world;q::World region;};
struct Undo{std::vector<uintptr_t> undo;uintptr_t active_graph=0;};
namespace d{struct Sample{z::State state;a::Ledger money;Undo undo;std::array<int32_t,8> release_edits{};};}
struct ToolPin{uintptr_t behaviour=100,cursor=200;int32_t rank=1;uint8_t loose=0;};
struct Operation{a::Binding binding;a::Request request;Pin pin;a::Evidence evidence;uintptr_t pointer=0;uint64_t uid=0;unsigned attempted=0,returned=0;bool confirmed=false,retired=false;std::string cause;json preparation=json::object();};
struct Pending{a::MonsterIntent intent;a::Binding binding;std::string snapshot,token;};
struct Runtime{uintptr_t image=0x140000000;unsigned thread=7;bool ready=true,replaying=false,fault=false;
 std::map<std::string,Operation> operations;std::optional<Pending> pending;std::string ui_token,cause;std::map<std::string,json> ui_jobs;uint64_t captures=0;}state;
struct Model{d::Sample sample;a::MonsterState value;std::string fault;unsigned prepare=0,hover=0,press=0,transform=0,release=0,seed=0;bool after=false,pending=true;uint8_t pressed=0;unsigned thread=7;std::array<float,3> preview_scale{};}model;
unsigned GetCurrentThreadId(){return model.thread;}
bool fail(std::string& e,const std::string& reason){e=reason;return false;}
bool gate(const char* fault,std::string& e){return model.fault!=fault||fail(e,std::string(fault)+" failed");}
namespace typed_action_runtime{json controller_status(){return {{"started",true},{"locked",false},{"pending",model.pending},{"action_uuid",action_id}};}}
bool code(Memory&,bool,std::string& e){return gate("code",e);}
bool pin(Memory&,const a::Binding&,const std::string&,int32_t id,Pin& p,std::string& e){
 if(model.fault=="exception")throw std::runtime_error("current owner read exception");p.region.region=900;p.region.saved_region=id;return gate("pin",e);}
bool same_world(const World&,const World&){return !model.after||model.fault!="after_owner";}
identity::Scope scope(const Pin&,const d::Sample&){return {snapshot_id,3,4,100,900,1000};}
a::Ledger ledger(const a::Ledger& value){return value;}
bool effect(Memory&,const Pin& p,d::Sample& sample,a::MonsterState& value,std::string& e,uint64_t uid=0,const std::string& pending={}){
 if(model.after&&!gate("readback",e))return false;
 sample=model.sample;value=model.value;if(!uid)value.actor={};
 else{std::string key;if(!identity::registry.resolve(scope(p,sample),800,uid,key,e,pending))return false;}
 return true;
}
namespace d{bool sample(Memory&,const q::World&,Sample& out,std::string& e){out=model.sample;return gate("sample",e);}}
uint32_t shard_region(uintptr_t,uintptr_t,const std::array<float,3>&,uintptr_t& r){r=model.fault=="point"?901:900;return 0;}
bool no_gameplay_change(const d::Sample& before,const d::Sample& after){return before.money==after.money&&before.undo.undo==after.undo.undo&&before.release_edits==after.release_edits&&before.state.monsters==after.state.monsters&&before.state.zones==after.state.zones;}
bool prepare(Memory&,const Pin&,const a::MonsterIntent&,ToolPin&,Operation& op,std::string& e){++model.prepare;op.attempted|=1;
 if(!gate("prepare",e))return false;op.returned|=1;if(model.fault=="prepare_gameplay")++model.sample.money.cash;return true;}
bool pose(Memory&,const ToolPin&,const a::MonsterIntent&,uintptr_t,std::string& e,bool complete=true){return gate(complete?"pose_after":"pose_before",e);}
uint32_t set_transform(const ToolPin&,const a::MonsterIntent& intent){model.preview_scale=intent.scale;++model.transform;return model.fault=="transform"?5:0;}
template<class T>bool field(Memory&,uintptr_t,size_t offset,T& out,std::string&){out=T(offset==0x3e6?model.pressed:offset==0x23c||offset==0x238?0:1);return true;}
bool full_uid(Memory&,const Pin&,const z::Monster&,std::string& e){return gate("uid",e);}
namespace undo_probe{
struct GraphWitness{bool locked=true;unsigned operations=1,nodes=0,pending_delete=0;};
bool read_graph(int,uintptr_t,uintptr_t,GraphWitness& out,std::string& e){if(model.fault=="graph")out.operations=2;return gate("undo_read",e);}
bool validate_undo_commit(const Undo&,const Undo&,const GraphWitness&,std::string& e){return gate("undo_commit",e);}}
namespace action_runtime{uint32_t seed(uintptr_t,const std::array<uint64_t,4>& values){++model.seed;
 if(model.fault!="seed_mismatch")model.value.roster.global_random=model.sample.state.global_random=values;
 if(model.fault=="seed_region")++model.value.roster.region_random[0];return model.fault=="seed"?5:0;}}
uint32_t input(uintptr_t rva,const ToolPin&,const std::array<float,3>&){
 if(rva==0x5bf6e0){++model.hover;if(model.fault=="hover_gameplay")++model.sample.money.cash;return model.fault=="hover"?5:0;}
 if(rva==0x5beaf0){++model.press;model.pressed=model.fault=="pressed"?0:1;return model.fault=="press"?5:0;}
 ++model.release;model.after=true;if(model.fault=="release")return 5;model.value=born(model.value);
 model.sample.state.global_random=model.value.roster.global_random;model.sample.state.region_random=model.value.roster.region_random;
 model.sample.state.next_monster_tag=model.value.roster.next_tag;model.sample.state.created_monsters=model.value.roster.created_monsters;
 model.sample.release_edits=model.value.roster.release_edits;model.sample.undo.undo.push_back(500);
 const auto& v=model.value.actor;z::Monster row;row.pointer=800;row.uid=3713;row.tag=v.tag;row.status=v.status;row.rank=v.rank;row.slot=v.character_slot;row.type=v.character_type;row.level=v.level;row.position=v.position;row.rotation=v.rotation;row.scale=v.scale;
 if(model.fault=="zone")row.zone=700;model.sample.state.monsters.push_back(row);model.pressed=model.fault=="release_pressed"?1:0;
 if(model.fault=="counter")++model.value.roster.next_tag;
 if(model.fault=="ledger")++model.sample.money.cash;
 if(model.fault=="region_rng")++model.value.roster.region_random[0];
 if(model.fault=="global_rng")++model.value.roster.global_random[0];
 if(model.fault=="name")model.value.actor.name="Different";
 if(model.fault=="actor_scale")model.value.actor.scale={2,2,2};
 return 0;
}
#include "monster_perform_body.inc"
void reset(){state={};model={};model.sample.money=money();model.value=empty();model.sample.state.monster_manager=1000;identity::registry={};}
}
int main(){try{std::string e;unsigned count=0;const auto req=request().request;const a::Binding binding{3,4};
 reset();need(perform(binding,req,nullptr,e)==a::Outcome::Confirmed,e);auto authority=state.operations.at(action_id).evidence;
 need(model.prepare==1&&model.hover==1&&model.press==1&&model.transform==1&&model.release==1&&!model.pressed,"complete original creation sequence differs");++count;
 need(perform(binding,req,nullptr,e)==a::Outcome::Unknown&&model.release==1,"original Release replayed");++count;
 reset();need(perform(binding,req,&authority,e)==a::Outcome::Confirmed,e);need(model.seed==1&&model.release==1,"Client did not seed/release once");++count;
 for(const auto* f:{"code","pin","point"}){reset();model.fault=f;need(perform(binding,req,nullptr,e)==a::Outcome::RejectedNoEffect,e);need(!model.prepare&&!model.release,"preflight mutated native state");++count;}
 for(const auto* f:{"prepare","prepare_gameplay","hover","hover_gameplay","pose_before","press","pressed","transform","pose_after","release","after_owner","sample","zone","uid","readback","graph","undo_read","undo_commit","release_pressed","counter","ledger","region_rng"}){
  reset();model.fault=f;need(perform(binding,req,nullptr,e)==a::Outcome::Unknown,std::string(f)+": "+e);need(!state.operations.at(action_id).confirmed,"partial original effect confirmed");
  const auto calls=model.prepare+model.hover+model.press+model.transform+model.release;need(perform(binding,req,nullptr,e)==a::Outcome::Unknown&&calls==model.prepare+model.hover+model.press+model.transform+model.release,"Unknown replayed original calls");++count;}
 for(const auto* f:{"seed","seed_mismatch","seed_region"}){reset();model.fault=f;auto host=authority;host.monster_before.roster.global_random[0]=77;need(perform(binding,req,&host,e)==a::Outcome::Unknown&&!model.release,std::string(f)+": "+e);++count;}
 for(const auto* f:{"global_rng","name"}){reset();model.fault=f;need(perform(binding,req,&authority,e)==a::Outcome::Unknown&&model.release==1,std::string(f)+": "+e);++count;}
 reset();model.value.roster.region_random={11,22,33,44};model.sample.state.region_random=model.value.roster.region_random;need(perform(binding,req,&authority,e)==a::Outcome::Confirmed,e);need(state.operations.at(action_id).evidence.monster_after.roster.region_random==std::array<uint64_t,4>{11,22,33,44},"Client Region RNG replaced by Host");++count;
 reset();auto preview=req;preview.monster.scale={2,2,2};
 need(perform(binding,preview,nullptr,e)==a::Outcome::Confirmed,e);
 auto elite_authority=state.operations.at(action_id).evidence;
 need(model.preview_scale==preview.monster.scale&&elite_authority.monster_after.actor.scale==std::array<float,3>{1,1,1},"Elite preview scale was confused with spawned Prop scale");++count;
 reset();need(perform(binding,preview,&elite_authority,e)==a::Outcome::Confirmed,e);need(model.seed==1&&model.release==1,"Elite Client original Release differs");++count;
 reset();model.fault="actor_scale";need(perform(binding,preview,nullptr,e)==a::Outcome::Unknown&&e=="Monster original transition: actor native unit scale differs",e);
 need(perform(binding,preview,nullptr,e)==a::Outcome::Unknown&&model.release==1,"bad native scale was replayed");++count;
 reset();auto wrong=authority;wrong.monster_before.roster.content_sha256=std::string(64,'c');need(perform(binding,req,&wrong,e)==a::Outcome::Unknown&&!model.seed&&!model.release,"wrong Host BEFORE reached RNG/Release");++count;
 // Loaded members retain their local serial; a fresh Spawn may reuse that value.
 auto loaded=[&](int32_t tag,int32_t next,int32_t created){reset();model.value.roster.monsters=1;
  model.value.roster.next_tag=model.sample.state.next_monster_tag=next;
  model.value.roster.created_monsters=model.sample.state.created_monsters=created;
  z::Monster old;old.pointer=9000;old.uid=0x100000001ull;old.tag=tag;model.sample.state.monsters={old};};
 loaded(-11752269,0,3);need(perform(binding,req,nullptr,e)==a::Outcome::Confirmed,e);const auto loaded_host=state.operations.at(action_id).evidence;
 loaded(7,7,3);need(perform(binding,req,&loaded_host,e)==a::Outcome::Confirmed,e);
 const auto& local=state.operations.at(action_id).evidence;
 need(model.release==1&&model.sample.state.monsters.size()==2&&model.sample.state.monsters[0].tag==7&&model.sample.state.monsters[1].tag==7&&
  local.monster_before.roster.next_tag==7&&local.monster_after.roster.next_tag==8&&local.monster_after.actor.tag==7&&local.monster_after.roster.created_monsters==4,
  "normal Place after reload lost local serial/counter witnesses");++count;
 reset();model.fault="exception";need(perform(binding,req,nullptr,e)==a::Outcome::Unknown&&e.find("current owner read exception")!=std::string::npos,"boundary exception cause hidden");++count;
 std::cout<<count<<" actual native perform body cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
