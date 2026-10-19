#include "live_host_state/monster_state.inc"
#include "monster_move_read.h"
#include "direct_monster_identity.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
namespace ml=mt2mp::game::live_host_state::monsters;
namespace mr=mt2mp::game::monster_move_read;
namespace identity=mt2mp::game::direct_monster_identity;
namespace {
constexpr uintptr_t test_object=0x100000,test_region=0x200000,test_character=0x300000,test_actor=0x400000;
struct Memory{};struct LiveWorld{};struct{uintptr_t image=0;}image_state;
struct LiveLocal{identity::Entry entry;mr::Actor actor;std::array<int32_t,ml::animation_slots> animation_index=[] {std::array<int32_t,ml::animation_slots> a;a.fill(-1);a[0]=4;a[1]=7;return a;}();std::array<float,ml::animation_slots> animation_length{};};
std::map<uintptr_t,std::vector<unsigned char>> fields;
float raw_time=0,asset_length=1;
uint32_t native_fault=0;
bool is_playing=true;
std::vector<int32_t> semantic_reads;
unsigned checks=0;
bool fail(std::string& e,const std::string& why){e=why;return false;}
void need(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
template<class T>void put(uintptr_t address,const T& value){auto& bytes=fields[address];bytes.resize(sizeof(T));std::memcpy(bytes.data(),&value,sizeof(T));}
template<class T>bool field(Memory&,uintptr_t base,size_t offset,T& out,std::string& e){
    const auto found=fields.find(base+offset);if(found==fields.end()||found->second.size()!=sizeof(T))return fail(e,"fixture unreadable row field");
    std::memcpy(&out,found->second.data(),sizeof(T));return true;
}
namespace npc_census{bool read_name(Memory&,uintptr_t,std::string& out,std::string&){out="Airg";return true;}}
uint32_t live_max(uintptr_t object,int32_t,float& out){need(object==test_object,"maximum reader target differs");out=100;return 0;}
uint32_t live_animation_read(uintptr_t character,uintptr_t actor,int32_t semantic,bool& playing,float& time,float& length,bool& paused,float& speed){
    need(character==test_character&&actor==test_actor,"animation actor binding differs");semantic_reads.push_back(semantic);
    paused=false;speed=1;playing=semantic==0&&is_playing;time=semantic==0?raw_time:0;length=semantic==0?asset_length:2;return native_fault;
}
class MonsterLivePort{
public:
    struct Membership{bool available=true;};
    bool membership(Memory&,const LiveWorld&,const LiveLocal&,Membership&,std::string&){return true;}
    bool full(Memory&,const LiveWorld&,uintptr_t object,identity::Entry& entry,std::string& birth,std::string&){
        need(object==test_object,"full target differs");entry.scope.region=test_region;entry.pointer=object;entry.uid=0x100001d01;birth="11111111-1111-4111-8111-111111111111";return true;
    }
    bool actor(Memory&,uintptr_t,LiveLocal& local,std::string&){local.actor.character=test_character;local.actor.actor=test_actor;local.actor.on_map=true;return true;}
    bool target(Memory&,const LiveWorld&,uintptr_t,ml::TargetObservation& out,std::string&){out={};return true;}
#define state image_state
#include "monster_live_animation_row.inc"
#undef state
};
void reset(){
    fields.clear();semantic_reads.clear();raw_time=0;asset_length=1;native_fault=0;is_playing=true;
    put(test_region+0x834,int32_t(67));put(test_object+0x820,int32_t(0));put(test_object+0x838,int32_t(1));put(test_object+0x4a8,int32_t(1));put(test_object+0x4a4,int32_t(0));put(test_object+0x6f8,int32_t(1));
    put(test_object+0x100,std::array<float,4>{0,0,0,1});put(test_object+0x110,std::array<float,3>{8020,70,8550});put(test_object+0x11c,std::array<float,3>{1,1,1});
    put(test_object+0x910,uint32_t(0));put(test_object+0x7f8,int32_t(-1));put(test_object+0x4ac,uint8_t(0));put(test_object+0x828,int32_t(0));put(test_object+0x824,int32_t(3));
    for(size_t i=0;i<3;++i){put(test_object+0x730+i*0x28,uintptr_t(0x16dd550));put(test_object+0x750+i*0x28,uint32_t(0));put(test_object+0x754+i*0x28,uint32_t(0));}
}
bool sample(ml::Row& out,std::string& e){MonsterLivePort port;Memory memory;LiveWorld world;LiveLocal local;out={};return port.row(memory,world,test_object,out,local,e);}
void accepts(float time){
    reset();raw_time=time;ml::Row row;std::string e;need(sample(row,e),"actual row rejected native raw time: "+e);
    need(row.animation[0]==std::bit_cast<uint32_t>(time)&&row.animation_length[0]==std::bit_cast<uint32_t>(1.0f),"native raw phase was clamped/wrapped");
    need(!row.animation[1]&&!row.animation[2]&&row.animation_length[1]&&!row.animation_length[2],"stopped or absent semantic clip changed");
    need(semantic_reads==std::vector<int32_t>{0,1},"semantic mapping/absent clip differs");
    ml::State state{{row},{}},decoded;need(ml::valid(state,e)&&ml::decode(ml::encode(state),decoded,e)&&decoded==state,"real animation codec changed raw bits: "+e);
    auto before=state;before.rows[0].animation[0]=std::bit_cast<uint32_t>(0.25f);
    need(ml::preflight(before,state,e)&&ml::adopted(state,decoded,e),"phase-only update rejected: "+e);
}
void rejected(float time,float length,uint32_t fault,const std::string& cause){
    reset();raw_time=time;asset_length=length;native_fault=fault;ml::Row row;std::string e;need(!sample(row,e),"invalid sample accepted");
    need(e.find(cause)!=std::string::npos&&e.find("semantic=0 index=4 time=")!=std::string::npos&&e.find("length=")!=std::string::npos,"sample lost measured cause: "+e);
    need(e.find("time_bits="+std::to_string(std::bit_cast<uint32_t>(time)))!=std::string::npos&&e.find("length_bits="+std::to_string(std::bit_cast<uint32_t>(length)))!=std::string::npos,"sample lost exact float bits");
}
}
int main(){try{
    // Exact 0x257e0 mode2 does one subtraction; mode3 only flips direction.
    accepts((0.75f+2.0f)-1.0f); // Loop overshoot remains 1.75 for length1.
    accepts(0.75f+0.5f); // Ping-pong forward overshoot remains 1.25.
    accepts(0.25f-0.5f); // Ping-pong reverse overshoot remains -0.25.
    for(float time:{0.0f,-0.0f,1.0f,0.5f,-4.0f,8.0f})accepts(time);
    rejected(std::numeric_limits<float>::infinity(),1,0,"time is not finite");
    rejected(-std::numeric_limits<float>::infinity(),1,0,"time is not finite");
    rejected(std::numeric_limits<float>::quiet_NaN(),1,0,"time is not finite");
    rejected(0,0,0,"asset length invalid");rejected(0,-1,0,"asset length invalid");
    rejected(0,std::numeric_limits<float>::infinity(),0,"asset length invalid");
    rejected(0,std::numeric_limits<float>::quiet_NaN(),0,"asset length invalid");
    rejected(-0.25f,1,5,"observation SEH=5");
    reset();ml::Row row;std::string e;need(sample(row,e),e);
    for(uint32_t bits:{std::bit_cast<uint32_t>(INFINITY),std::bit_cast<uint32_t>(-INFINITY),std::bit_cast<uint32_t>(NAN)}){
        auto invalid=row;invalid.animation[0]=bits;ml::State decoded;
        need(!ml::decode(ml::encode({{invalid},{}}),decoded,e)&&decoded.rows.empty(),"codec accepted nonfinite native time");}
    reset();is_playing=false;raw_time=std::numeric_limits<float>::quiet_NaN();need(sample(row,e)&&!row.animation[0],"stopped time leaked onto wire");
    std::cout<<"PASS "<<checks<<" actual Monster row/raw-time codec assertions\n";return 0;
}catch(const std::exception& x){std::cerr<<"FAIL "<<x.what()<<'\n';return 1;}}
