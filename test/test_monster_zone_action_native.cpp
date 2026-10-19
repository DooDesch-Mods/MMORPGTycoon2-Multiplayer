#include "typed_action/action_session.h"
#include "monster_zone_read.h"
#include "monster_zone_effect.h"
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <algorithm>
namespace a=mt2mp::action_session;
namespace z=mt2mp::game::monster_zone_read;
namespace q=mt2mp::game::npc_quest_targets;
namespace monster_zone_effect=mt2mp::game::monster_zone_effect;
using json=nlohmann::json;
namespace {
void need(bool v,const std::string& e){if(!v)throw std::runtime_error(e);}
struct Undo {std::vector<uintptr_t> undo;uintptr_t active_graph=0;};
struct Memory {};
namespace d {struct Sample{z::State state;a::Ledger money;Undo undo;std::array<int32_t,8> release_edits{};};}
struct World {uintptr_t root=100;};
struct Pin {World world;q::World region;int32_t min_level=1,max_level=2;};
struct Brush {std::array<unsigned char,0x750> bytes{};};
struct Operation {
    a::Binding binding;a::Request request;Pin pin;a::Evidence evidence;
    std::unique_ptr<Brush> brush=std::make_unique<Brush>();uintptr_t graph=500,created=0;uint64_t uid=0;
    bool constructed=false,attempted=false,returned=false,confirmed=false,retired=false;
    unsigned stamps=0;std::string cause;
};
struct Runtime {uintptr_t image=0x140000000;uint32_t thread=7;bool ready=true,capture_fault=false;
    void(*finished)(void*,const void*)=nullptr;std::map<std::string,Operation> operations;} state;
struct Model {
    d::Sample sample;a::ZoneState value;std::string fault;
    unsigned construct=0,paint=0,finish=0,destroy=0,append_begin=0,append_finish=0,seed=0;
    bool after=false,overlap=false,preparation_unknown=false,pending=true,throw_pin=false;uint32_t thread=7;
    void reset(){*this={};sample.money.cash=sample.money.record_cash=100;sample.money.record_present=1;sample.money.flat_day=sample.money.record_day=1;value.content_sha256=std::string(64,'a');}
} model;
uint32_t GetCurrentThreadId(){return model.thread;}
bool fail(std::string& e,const std::string& reason){e=reason;return false;}
namespace typed_action_runtime {json controller_status(){return {{"pending",model.pending},{"action_uuid","11111111-1111-4111-8111-111111111111"}};}}
bool all_code(Memory&,bool,std::string& e){return model.fault!="code"||fail(e,"exact code differs");}
bool pin(Memory&,const a::Binding&,const std::string&,int32_t id,int32_t,Pin& out,std::string& e){
    if(model.throw_pin)throw std::runtime_error("owner read exception");if(model.fault=="theme_lifetime"){state.capture_fault=true;return fail(e,"Zone original Window.Find temporary lifetime Unknown; no replay");}out.region.region=900;out.region.saved_region=id;
    return model.fault!="owner"||fail(e,"current Origin changed");}
bool same_origin(const Pin&,const Pin&){return model.fault!="after_owner"||!model.after;}
a::Ledger ledger(const a::Ledger& value){return value;}
bool effect(Memory&,const Pin&,d::Sample& out,a::ZoneState& value,std::string& e){
    if(model.fault=="readback"&&model.after)return fail(e,"original final census unavailable");out=model.sample;value=model.value;return true;}
namespace d {bool sample(::Memory&,const q::World&,Sample& out,std::string&){out=model.sample;return true;}}
uint32_t shard_region(uintptr_t,uintptr_t,const std::array<float,3>&,uintptr_t& out){out=900;return 0;}
bool no_overlap(Memory&,const a::ZoneIntent&,const d::Sample&,bool& unknown,std::string& e){unknown=model.preparation_unknown;
    return (!model.overlap&&!unknown)||fail(e,unknown?"original PolyShape destructor exception=5":"Zone new stroke intersects an existing Zone");}
namespace npc_quest_target_runtime {
bool begin_zone_append(int32_t,const std::string&,std::string& e){++model.append_begin;return model.fault!="begin"||fail(e,"existing canonical member changed");}
bool finish_zone_append(int32_t,const std::string& id,uintptr_t,uint64_t,const q::Content&,std::string& key,std::string& e){++model.append_finish;
    key="action:"+id;return model.fault!="append"||fail(e,"unexpected third Zone member");}
}
namespace action_runtime {uint32_t seed(uintptr_t,const std::array<uint64_t,4>& value){++model.seed;if(model.fault!="seed_mismatch")model.value.global_random=value;
    if(model.fault=="seed_region"){++model.value.region_random[0];++model.sample.state.region_random[0];}
    return model.fault=="seed"?5:0;}}
bool native_ok(uint32_t code,const char* name,std::string& e){return !code||fail(e,std::string(name)+" original exception="+std::to_string(code));}
uint32_t construct(Operation& op){++model.construct;op.constructed=true;return model.fault=="construct"?5:0;}
bool paint_private(Operation& op,const a::ZoneStamp&,std::string& e){++model.paint;++op.stamps;
    if(model.fault=="paint")return fail(e,"original AddPaint partial failure");return true;}
template<class T>bool field(Memory&,uintptr_t,size_t offset,T& out,std::string&){
    if constexpr(std::is_same_v<T,uintptr_t>){out=offset==0x710?700:offset==0x18?42:0;}return true;
}
uint32_t binary(void(*)(void*,const void*),void*,const void*){
    ++model.finish;model.after=true;if(model.fault=="finish")return 5;
    z::Zone zone;zone.value.pointer=700;zone.value.uid=42;zone.value.content.type=1;zone.value.content.level=1;zone.value.content.monster_type=0;
    for(int32_t i=0;i<4;++i){z::Monster m;m.pointer=800+uintptr_t(i);m.uid=50+uint64_t(i);m.zone=700;m.tag=i;m.slot=1;m.type=0;m.level=1;m.status=3;
        zone.monsters.push_back(m.uid);model.sample.state.monsters.push_back(m);}
    model.sample.state.next_monster_tag=model.sample.state.created_monsters=4;
    model.sample.state.zones.push_back(zone);model.sample.undo.undo.push_back(500);++model.value.zones;++model.value.undo_count;
    model.value.monsters=4;model.value.next_tag=model.value.created_monsters=4;++model.value.release_edits[5];model.value.content_sha256=std::string(64,'b');
    if(model.fault=="finish_region"){++model.value.region_random[0];++model.sample.state.region_random[0];}
    if(model.fault=="global_after")++model.value.global_random[0];
    if(model.fault=="ledger")++model.sample.money.cash;if(model.fault=="counter")++model.value.next_tag;return 0;
}
using Unary=void(*)(void*);
uint32_t unary(Unary,void*){++model.destroy;return model.fault=="destroy"?5:0;}
#include "zone_perform_body.inc"
a::Request request(){a::Request v;v.action=a::Action::PlaceMonsterZone;v.action_uuid="11111111-1111-4111-8111-111111111111";
    v.snapshot_uuid="22222222-2222-4222-8222-222222222222";v.canonical_record_key="action:"+v.action_uuid;v.zone.region=67;v.zone.character_type=0;v.zone.level=1;
    v.zone.stamps.push_back({{8020,0,8550},{{{1,2},{3,4},{5,6}}}});return v;}
void reset(){state.operations.clear();model.reset();}
}
int main(){try{unsigned checks=0;const auto req=request();const a::Binding binding{3,4};std::string e;
    reset();need(perform(binding,req,nullptr,e)==a::Outcome::Confirmed,e);const auto authority=state.operations.at(req.action_uuid).evidence;
    need(model.construct==1&&model.paint==1&&model.finish==1&&model.destroy==1&&model.append_begin==1&&model.append_finish==1,"original native ordering/counts differ");++checks;
    need(perform(binding,req,nullptr,e)==a::Outcome::Unknown&&e.find("no replay")!=std::string::npos&&model.finish==1,"confirmed original action replayed");++checks;
    reset();need(perform(binding,req,&authority,e)==a::Outcome::Confirmed,e);need(model.seed==1&&model.finish==1,"Client did not seed/execute once");++checks;
    for(const auto fault:{"code","owner","begin"}){reset();model.fault=fault;need(perform(binding,req,nullptr,e)==a::Outcome::RejectedNoEffect,e);need(!model.construct&&!model.finish,"preflight performed native mutation");++checks;}
    reset();model.overlap=true;need(perform(binding,req,nullptr,e)==a::Outcome::RejectedNoEffect&&!model.construct&&e.find("intersects")!=std::string::npos,e);++checks;
    reset();model.preparation_unknown=true;need(perform(binding,req,nullptr,e)==a::Outcome::Unknown&&!model.construct&&e.find("destructor")!=std::string::npos,e);
    need(perform(binding,req,nullptr,e)==a::Outcome::Unknown&&e.find("no replay")!=std::string::npos,e);++checks;
    for(const auto fault:{"construct","paint","finish","destroy","append","readback","ledger","counter","after_owner"}){
        reset();model.fault=fault;need(perform(binding,req,nullptr,e)==a::Outcome::Unknown,std::string(fault)+": "+e);
        const auto calls=model.paint+model.finish+model.destroy;need(!state.operations.at(req.action_uuid).confirmed,"partial action confirmed");
        need(perform(binding,req,nullptr,e)==a::Outcome::Unknown&&calls==model.paint+model.finish+model.destroy,"partial original replayed");++checks;}
    reset();auto wrong=authority;wrong.zone_before.content_sha256=std::string(64,'c');need(perform(binding,req,&wrong,e)==a::Outcome::RejectedNoEffect&&!model.construct,e);++checks;
    reset();wrong=authority;wrong.zone_after.content_sha256=std::string(64,'c');need(perform(binding,req,&wrong,e)==a::Outcome::Unknown&&model.finish==1,e);++checks;
    reset();auto invalid=req;invalid.zone.level=3;need(perform(binding,invalid,nullptr,e)==a::Outcome::RejectedNoEffect&&!model.construct&&e.find("level range")!=std::string::npos,e);++checks;
    reset();model.throw_pin=true;need(perform(binding,req,nullptr,e)==a::Outcome::Unknown&&e.find("owner read exception")!=std::string::npos,e);++checks;
    reset();const std::array<uint64_t,4> client_region{11031208541562408887ull,7264565988762834373ull,9860586388526540594ull,17295355788292699441ull};
    model.value.region_random=client_region;model.sample.state.region_random=client_region;
    need(perform(binding,req,&authority,e)==a::Outcome::Confirmed,e);
    const auto& actual=state.operations.at(req.action_uuid).evidence;
    need(actual.zone_before.region_random==client_region&&actual.zone_after.region_random==client_region&&
        actual.zone_after.region_random!=authority.zone_after.region_random,"Client Region RNG receipt replaced by Host value");++checks;
    reset();model.fault="seed_region";need(perform(binding,req,&authority,e)==a::Outcome::Unknown&&!model.construct&&e.find("changed local Region RNG")!=std::string::npos,e);++checks;
    reset();model.fault="finish_region";need(perform(binding,req,&authority,e)==a::Outcome::Unknown&&model.finish==1&&e.find("Region RNG")!=std::string::npos,e);++checks;
    reset();model.fault="global_after";need(perform(binding,req,&authority,e)==a::Outcome::Unknown&&model.finish==1,e);++checks;
    reset();auto seeded=authority;seeded.zone_before.global_random[0]=77;model.fault="seed_mismatch";
    need(perform(binding,req,&seeded,e)==a::Outcome::Unknown&&!model.construct&&e.find("shared readback")!=std::string::npos,e);++checks;
    reset();auto changed=authority;changed.zone_before.content_sha256=std::string(64,'c');
    need(perform(binding,req,&changed,e)==a::Outcome::RejectedNoEffect&&!model.seed&&!model.construct,e);++checks;
    reset();model.fault="theme_lifetime";
    need(perform(binding,req,&authority,e)==a::Outcome::Unknown&&state.capture_fault&&!model.seed&&!model.construct&&e.find("temporary lifetime Unknown")!=std::string::npos,e);++checks;
    std::cout<<checks<<" actual native perform body cases passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
