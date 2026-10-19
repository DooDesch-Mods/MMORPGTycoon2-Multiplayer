#include "live_host_state/monster_state.inc"
#include "live_host_state/monster_apply.h"
#include "live_host_state/scalar_store.h"
#include "monster_move_read.h"
#include "direct_monster_identity.h"
#include <cstring>
#include <iostream>
#include <map>
namespace live_host_state=mt2mp::game::live_host_state;
namespace ml=live_host_state::monsters;
namespace mr=mt2mp::game::monster_move_read;
namespace identity=mt2mp::game::direct_monster_identity;
using json=nlohmann::json;
namespace {
constexpr uintptr_t test_object=0x100000,test_region=0x200000,test_manager=0x300000,test_root=0x400000,test_map=0x500000,test_character=0x600000,test_render_actor=0x700000,test_skeleton=0x800000,test_spatial=0x900000,test_quadtree=0xa00000,test_model=0xb00000,test_proxy=0xc00000,test_ready_data=0xd00000,test_other=0xe00000;
const std::string test_birth="11111111-1111-4111-8111-111111111111";
constexpr uint64_t test_uid=UINT64_C(0x100001d01);
struct{uintptr_t image=0x140000000;}state;
size_t checks=0;
void need(bool ok,const std::string& why){++checks;if(!ok)throw std::runtime_error(why);}
bool fail(std::string& e,const std::string& why){e=why;return false;}
std::map<uintptr_t,uint8_t> memory;
template<class T>void put(uintptr_t address,const T& value){const auto* p=reinterpret_cast<const uint8_t*>(&value);for(size_t i=0;i<sizeof(T);++i)memory[address+i]=p[i];}
struct Memory{bool copy(uintptr_t address,void* out,size_t bytes,std::string& e){auto* p=static_cast<uint8_t*>(out);for(size_t i=0;i<bytes;++i){auto f=memory.find(address+i);if(f==memory.end())return fail(e,"unreadable fixture address="+std::to_string(address+i));p[i]=f->second;}return true;}};
template<class T>bool field(Memory& m,uintptr_t address,size_t offset,T& out,std::string& e){return m.copy(address+offset,&out,sizeof out,e);}
template<class T>T get(uintptr_t address){T v{};Memory m;std::string e;need(field(m,address,0,v,e),e);return v;}
struct World{uintptr_t root=::test_root;struct{uintptr_t map=::test_map;}origin;};
struct LiveWorld{World world;};
struct LiveLocal{identity::Entry entry;mr::Actor actor;std::array<int32_t,ml::animation_slots> animation_index{};std::array<float,ml::animation_slots> animation_length{};};
struct Animation{bool playing=false,paused=false;float time=0,length=2,speed=1;};
std::array<Animation,ml::animation_slots> animations;
std::vector<std::string> calls;
bool registered=true,available=true,wrong_uid=false;
int fail_call=0,native_calls=0,cas_calls=0,fail_cas=0;bool corrupt_other=false,corrupt_members=false,corrupt_target=false;int corrupt_witness=0;ml::TargetObservation observed_target;
uint32_t native(){++native_calls;return fail_call==native_calls?5:0;}
void ready(bool value){available=value;const int32_t count=value?2:1;put(test_manager+0x78,count);put(test_ready_data,test_other);if(value)put(test_ready_data+8,test_object);}
uint32_t live_membership_call(uintptr_t receiver,uintptr_t target,bool& out){need(target==test_object,"membership target changed");need(receiver==test_region+0x7c8||receiver==test_spatial,"membership receiver changed");out=receiver==test_spatial?available:registered;return 0;}
uint32_t live_lifecycle_call(uintptr_t rva,uintptr_t target,bool value){need(target==test_object,"lifecycle receiver changed");calls.push_back(rva==0x39a920?(value?"onmap":"offmap"):(value?"ready":"occupied"));if(auto code=native())return code;
    if(rva==0x39a920){registered=value;put(test_object+0x368,uint8_t(value));put(test_object+0x488,value?test_model:uintptr_t(0));}
    else {need(rva==0x3bbc00,"forbidden lifecycle original");ready(value);if(corrupt_members)put(test_ready_data,uintptr_t(0xeeee00));}
    return 0;
}
uint32_t live_expire(uintptr_t target){need(target==test_object,"expiry receiver changed");calls.push_back("expire");if(auto code=native())return code;const auto references=get<int32_t>(test_proxy+0x10);put(test_proxy+8,uintptr_t(0));put(test_object+0x600,uintptr_t(0));if(!references)for(size_t i=0;i<0x18;++i)memory.erase(test_proxy+i);return 0;}
uint32_t live_byte(uintptr_t address,uint8_t before,uint8_t after,bool& changed){calls.push_back("byte");if(auto code=native())return code;changed=get<uint8_t>(address)==before;if(changed)put(address,after);return 0;}
uint32_t live_transform(uintptr_t target,const mt2mp::game::subscriber_motion::Pose& pose){need(target==test_object,"transform receiver changed");calls.push_back("pose");if(auto code=native())return code;
    put(test_object+0x100,pose.orientation);put(test_object+0x110,pose.position);put(test_object+0x11c,pose.scale);put(test_object+0x480,uint8_t(1));return 0;}
uint32_t live_spatial(uintptr_t target){need(target==test_object,"spatial receiver changed");calls.push_back("quadtree");return native();}
uint32_t live_animation_set(uintptr_t c,uintptr_t a,int32_t semantic,int32_t index,bool playing,uint32_t time,bool paused,uint32_t speed){
    need(c==test_character&&a==test_render_actor&&index==semantic,"animation local mapping changed");calls.push_back(playing?"play":"stop");if(auto code=native())return code;
    auto& clip=animations.at(size_t(semantic));clip.playing=playing;if(playing){clip.time=std::bit_cast<float>(time);clip.paused=paused;clip.speed=std::bit_cast<float>(speed);}return 0;
}
uint32_t live_animation_read(uintptr_t c,uintptr_t a,int32_t semantic,bool& playing,float& time,float& length,bool& paused,float& speed){
    need(c==test_character&&a==test_render_actor,"animation read receiver changed");const auto& clip=animations.at(size_t(semantic));playing=clip.playing;time=clip.time;length=clip.length;paused=clip.paused;speed=clip.speed;return 0;
}
uint32_t live_max(uintptr_t target,int32_t i,float& out){need(target==test_object,"maximum receiver changed");out=i==0?20.0f:0.0f;return 0;}
namespace npc_census{bool read_name(Memory&,uintptr_t,std::string& out,std::string&){out="Airg";return true;}}
struct Pin{World world;uintptr_t region=::test_region;};
struct Undo{uintptr_t active_graph=0;bool operator==(const Undo&)const=default;};
namespace d{
struct Sample{mt2mp::game::monster_zone_read::State state;Undo undo;int release_edits=0,money=100;};
bool sample(Memory&,uintptr_t r,Sample& out,std::string&){need(r==test_region,"census Region changed");out={};auto& s=out.state;s.monster_manager=test_manager;
    mt2mp::game::monster_zone_read::Monster m;m.pointer=test_object;m.uid=test_uid;m.status=get<int32_t>(test_object+0x824);m.position=get<std::array<float,3>>(test_object+0x110);m.rotation=get<std::array<float,4>>(test_object+0x100);m.scale=get<std::array<float,3>>(test_object+0x11c);s.monsters.push_back(m);
    m={};m.pointer=test_other;m.uid=test_uid+1;m.status=7;if(corrupt_other&&native_calls)m.status=8;s.monsters.push_back(m);if(native_calls){if(corrupt_witness==1)++out.money;if(corrupt_witness==2)++s.global_random[0];if(corrupt_witness==3)++s.region_random[0];if(corrupt_witness==4)++out.undo.active_graph;if(corrupt_witness==5)++out.release_edits;}return true;
}}
bool pin(Memory&,int,const std::string&,int32_t,Pin& out,std::string&){out={};return true;}
bool same_world(const World& a,const World& b){return a.root==b.root&&a.origin.map==b.origin.map;}
int ledger(int value){return value;}
class MonsterLivePort{
public:
    std::map<std::string,LiveLocal> locals_;int binding_=0;struct{std::string snapshot_uuid="shared";}context_;
    json lifecycle_transitions_=json::array();std::set<std::string>wounded_;uint64_t lifecycle_transition_dropped_=0;
    bool world(Memory&,LiveWorld& out,std::string&){out={};return true;}
    bool full(Memory&,const LiveWorld&,uintptr_t target,identity::Entry& entry,std::string& key,std::string& e){
        if(wrong_uid)return fail(e,"full UID changed");need(target==test_object,"full receiver changed");entry={{"local-origin",8,9,test_root,test_region,test_manager},test_object,test_uid,true,false};key=test_birth;return true;}
    bool actor(Memory& m,uintptr_t target,LiveLocal& out,std::string& e){
        if(!mr::actor(state.image,target,[&](uintptr_t base,size_t offset,auto& value,std::string& error){return field(m,base,offset,value,error);},out.actor,e))return false;
        for(size_t i=0;i<ml::animation_slots;++i)out.animation_index[i]=int32_t(i);return true;
    }
    bool target(Memory&,const LiveWorld&,uintptr_t,ml::TargetObservation& out,std::string&){out=observed_target;return true;}
    bool read(ml::State& out,std::string& e){Memory m;LiveWorld p;ml::Row value;LiveLocal local;out={};if(!row(m,p,test_object,value,local,e))return false;locals_[test_birth]=local;out.rows.push_back(value);return ml::valid(out,e);}
    bool present(uintptr_t target,LiveLocal&,float dt,std::string&){need(target==test_object&&dt==0,"presentation simulated Client");calls.push_back("present");if(corrupt_target)observed_target={"mapped","subscriber:67"};return true;}
#include "monster_live_lifecycle_bodies.inc"
};
void reset(){memory.clear();calls.clear();animations={};registered=available=true;wrong_uid=false;fail_call=native_calls=cas_calls=fail_cas=0;corrupt_other=corrupt_members=corrupt_target=false;corrupt_witness=0;observed_target={};
    put(test_object,state.image+0x16c0770);put(state.image+0x16c0770+0x290,state.image+0x39a920);put(state.image+0x16c0770+0x2a0,state.image+0x3b9eb0);
    put(test_object+0x368,uint8_t(1));put(test_object+0x6f0,test_character);put(test_character,test_render_actor);put(test_render_actor+0x40,test_skeleton);put(test_skeleton+0xe4,uint8_t(1));put(test_object+0x1c0,uint8_t(1));put(test_object+0x808,uint8_t(1));
    put(test_object+0x82c,std::array<float,3>{8020,70,8550});put(test_object+0x100,std::array<float,4>{0,0,0,1});put(test_object+0x110,std::array<float,3>{8020,70,8550});put(test_object+0x11c,std::array<float,3>{1,1,1});
    put(test_region+0x834,int32_t(67));put(test_region+0x70,test_root);put(test_manager+0x80,test_region);put(test_manager+0x70,test_ready_data);put(test_manager+0x7c,int32_t(8));put(test_manager+0x88,test_spatial);ready(true);
    put(test_root+0x48,int32_t(0));put(state.image+0x13df8c0,test_map);put(test_map+0x290,test_quadtree);put(test_object+0x488,test_model);put(test_model+0x118,test_object);put(test_model+8,test_object+0x30);put(test_model,uint32_t(0x10));
    put(test_object+0x600,test_proxy);put(test_proxy+8,test_object);put(test_proxy+0x10,int32_t(2));put(test_object+0x480,uint8_t(0));
    put(test_object+0x820,int32_t(0));put(test_object+0x838,int32_t(1));put(test_object+0x4a8,int32_t(1));put(test_object+0x4a4,int32_t(0));put(test_object+0x6f8,int32_t(1));
    put(test_object+0x4ac,uint8_t(0));put(test_object+0x828,int32_t(0));put(test_object+0x824,int32_t(3));put(test_object+0x910,uint32_t(0));put(test_object+0x7f8,int32_t(-1));
    for(size_t i=0;i<3;++i){put(test_object+0x730+i*0x28,state.image+0x16dd550);put(test_object+0x750+i*0x28,std::bit_cast<uint32_t>(i==0?20.0f:0.0f));put(test_object+0x754+i*0x28,uint32_t(0));}
    animations[0].playing=true;
}
struct Calls{MonsterLivePort& port;ml::State before,after;bool read(ml::State& out,std::string& e){return port.read(out,e);}bool set(const ml::Row& a,const ml::Row& b,bool& attempted,std::string& e){return port.set(a,b,attempted,e);}void observed(const ml::State& a,const ml::State& b){before=a;after=b;}};
ml::State sample(MonsterLivePort& port){ml::State s;std::string e;need(port.read(s,e),"actual row: "+e);return s;}
ml::Receipt apply(MonsterLivePort& port,const ml::State& target){Calls c{port};auto r=ml::apply_motion(c,target);if(r.confirmed)port.trace(c.before,c.after,target,17);return r;}
void okay(MonsterLivePort& port,const ml::State& target){auto r=apply(port,target);need(r.confirmed&&!r.unknown,"actual lifecycle apply: "+r.cause);need(ml::same_applied(sample(port).rows[0],target.rows[0]),"actual owned after differs");}
}
namespace mt2mp::game::live_host_state{
bool compare_fixed_i32(uintptr_t address,int32_t before,int32_t after,std::string& e){++cas_calls;if(fail_cas==cas_calls)return fail(e,"retained CAS failure");if(get<int32_t>(address)!=before)return fail(e,"CAS expected bits changed");put(address,after);return true;}
}
int main(){try{
    reset();MonsterLivePort port;auto before=sample(port),wanted=before;auto& r=wanted.rows[0];
    r.host_combat.values[0]=std::bit_cast<uint32_t>(2.0f);r.host_combat.recovery[0]=std::bit_cast<uint32_t>(1.0f);r.host_combat.status=4;
    okay(port,wanted);need(cas_calls==3,"scalar-only update skipped or touched other fields");need(port.lifecycle_transitions_.at(0).at("phase")=="wounded","first damage trace missing");
    calls.clear();r.host_combat.dead=true;r.host_combat.ai=3;r.host_combat.values[0]=0;r.lifecycle.ready=false;r.lifecycle.respawn_time_bits=std::bit_cast<uint32_t>(10.0f);r.lifecycle.action=46;r.animation[0].reset();r.animation_speed[0]=0;r.animation[46]=std::bit_cast<uint32_t>(-0.25f);r.animation_speed[46]=std::bit_cast<uint32_t>(1.0f);
    okay(port,wanted);need(calls.front()=="expire"&&calls[1]=="occupied","death original order changed");need(get<uintptr_t>(test_object+0x600)==0&&get<uintptr_t>(test_proxy+8)==0&&get<int32_t>(test_proxy+0x10)==2,"previous-life reference not expired");
    need(std::find(calls.begin(),calls.end(),"stop")<std::find(calls.begin(),calls.end(),"play"),"base animation not stopped before death action");
    r.animation[46]=std::bit_cast<uint32_t>(3.5f);r.animation_paused[46]=true;okay(port,wanted);need(animations[46].paused&&animations[46].time==3.5f,"paused native overshoot clipped");
    r.on_map=false;r.animation[46].reset();r.animation_speed[46]=0;r.animation_paused[46]=false;okay(port,wanted);need(!registered&&!available&&get<uintptr_t>(test_object+0x488)==0,"off-map original membership incomplete");
    calls.clear();r.on_map=true;r.host_combat.dead=false;r.host_combat.ai=0;r.host_combat.values[0]=std::bit_cast<uint32_t>(20.0f);r.lifecycle.ready=true;r.lifecycle.respawn_time_bits=0;r.lifecycle.action=-1;r.spawn[1]=71;r.pose.position=r.spawn;r.animation[0]=std::bit_cast<uint32_t>(0.5f);r.animation_speed[0]=std::bit_cast<uint32_t>(1.0f);
    okay(port,wanted);need(std::find(calls.begin(),calls.end(),"pose")<std::find(calls.begin(),calls.end(),"onmap")&&registered&&available,"respawn pose must precede registry add");
    need(port.lifecycle_transitions_.size()==4&&port.lifecycle_transitions_[3]["phase"]=="respawn"&&port.lifecycle_transitions_[3]["local_uid"]==test_uid,"first cycle trace lost identity/order");
    calls.clear();const auto unchanged=apply(port,wanted);need(unchanged.confirmed&&!unchanged.attempted&&calls.empty(),"duplicate accepted lifecycle replayed effects");
    for(int failure=0;failure<8;++failure){reset();MonsterLivePort bad;auto goal=sample(bad);goal.rows[0].host_combat.dead=true;goal.rows[0].lifecycle.ready=false;
        if(failure==0)put(test_proxy+8,test_other);if(failure==1)put(test_model+0x118,test_other);if(failure==2)put(test_manager+0x80,uintptr_t(42));if(failure==3)put(test_ready_data+8,test_other);
        if(failure==4)put(test_object+0x730,uintptr_t(42));if(failure==5)wrong_uid=true;if(failure==6)put(state.image+0x13df8c0,uintptr_t(42));if(failure==7)put(test_proxy+0x10,int32_t(-1));
        auto result=apply(bad,goal);need(!result.confirmed&&!result.attempted&&!result.unknown&&!result.cause.empty(),"bad owner/domain mutated first "+std::to_string(failure));}
    for(int mode=0;mode<10;++mode){reset();MonsterLivePort bad;auto goal=sample(bad);goal.rows[0].host_combat.dead=true;goal.rows[0].host_combat.values[0]=0;goal.rows[0].lifecycle.ready=false;
        if(mode==0)fail_call=2;if(mode==1)fail_cas=1;if(mode==2)corrupt_other=true;if(mode==3)corrupt_members=true;if(mode==4)corrupt_target=true;if(mode>=5)corrupt_witness=mode-4;
        auto result=apply(bad,goal);need(!result.confirmed&&result.attempted&&result.unknown&&!result.cause.empty(),"partial lifecycle failure lost Unknown "+std::to_string(mode));
        if(mode==1)need(result.cause=="retained CAS failure","first CAS cause overwritten");}
    reset();MonsterLivePort zero;auto zero_goal=sample(zero);zero_goal.rows[0].host_combat.dead=true;put(test_proxy+0x10,int32_t(0));okay(zero,zero_goal);
    need(!memory.contains(test_proxy+8)&&get<uintptr_t>(test_object+0x600)==0,"zero-ref proxy read after native free or retained");
    auto current=sample(zero);for(size_t i=0;i<40;++i){auto next=current;next.rows[0].on_map=!current.rows[0].on_map;zero.trace(current,next,next,20+i);current=next;}
    need(zero.lifecycle_transitions_.size()<=32,"lifecycle trace unbounded");
    reset();MonsterLivePort codec;auto state_value=sample(codec);auto altered=state_value;altered.rows[0].host_combat.maxima[0]=std::bit_cast<uint32_t>(99.0f);std::string e;
    need(!ml::preflight(state_value,altered,e)&&calls.empty(),"derived maxima mismatch accepted");
    altered=state_value;altered.rows[0].spawn[1]+=1;need(!ml::preflight(state_value,altered,e),"ordinary spawn-height mutation accepted");
    auto encoded=ml::encode(state_value);ml::State decoded;need(ml::decode(encoded,decoded,e)&&decoded==state_value,"full49 codec changed bits");
    encoded["rows"][0]["animation_paused"].erase(encoded["rows"][0]["animation_paused"].end()-1);need(!ml::decode(encoded,decoded,e)&&decoded.rows.empty(),"short phase array accepted");
    std::cout<<"PASS "<<checks<<" actual row/membership/proxy/scalar/set/trace lifecycle assertions\n";return 0;
}catch(const std::exception& x){std::cerr<<"FAIL "<<x.what()<<'\n';return 1;}}
