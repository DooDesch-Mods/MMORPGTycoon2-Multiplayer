#include "vendor/nlohmann/json.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
using json=nlohmann::json;
constexpr uintptr_t image=0x10000000,fixture_cursor=0x20000000,fixture_brush=0x21000000,fixture_desired=0x22000000,fixture_region=0x23000000;
constexpr uintptr_t fixture_shape_a=0x24000000,fixture_shape_b=0x25000000,window_a=0x26000000,window_b=0x27000000;
unsigned fixture_mode=0,calls=0,effects=0;
struct Memory{};
struct Runtime{uintptr_t image=::image;unsigned thread=1;bool ready=true,pending=false,fault=false;}state;
unsigned GetCurrentThreadId(){return 1;}
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
void need(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
namespace a{struct Binding{};struct MonsterState{struct{std::string content_sha256="content";}roster;};}
struct Pin{struct{unsigned version=1;}world;struct{uintptr_t region=::fixture_region;}region;};
struct ToolPin{uintptr_t behaviour=fixture_brush;};
namespace d{
struct Sample{
 struct State{unsigned marker=0;std::array<uint64_t,4> global_random{},region_random{};std::vector<int> monsters,zones;
 uintptr_t manager=1,monster_manager=2;bool operator==(const State&)const=default;}state;
 struct{struct{bool paused=true;float time_scale=0;}clock;}money;
 struct{uintptr_t active_graph=0;}undo;
};
json sample_json(const Sample& sample){json ledger=json::array();for(unsigned i=0;i<21;++i)ledger.push_back(sample.state.marker==100+i?1:0);
 return {{"state",{{"marker",sample.state.marker},{"global_random",sample.state.global_random},{"region_random",sample.state.region_random}}},
 {"ledger",ledger},{"undo",sample.undo.active_graph}};}
bool tool(Memory&,uintptr_t,uintptr_t& out,uintptr_t& current,std::string& e){out=fixture_brush;current=fixture_cursor;
 if(fixture_mode==5)return fail(e,"original inventory weak owner differs");if(fixture_mode==6)++out;if(fixture_mode==7)++current;return true;}
}
namespace net{std::string current_snapshot_id(){return "snapshot";}}
namespace typed_action_runtime{
json controller_status(){return {{"started",true},{"locked",fixture_mode==1},{"pending",fixture_mode==2}};}
bool network_binding(const std::string&,a::Binding&,std::string& e){return fixture_mode!=8||fail(e,"native origin not bound");}
}
namespace zone_action_runtime{
bool pending_ui(){return fixture_mode==3;}
json status(){return {{"enabled",true},{"capture_active",false},{"capture_fault",fixture_mode==4}};}
}
bool pin(Memory&,const a::Binding&,const std::string&,int,Pin& p,std::string& e){
 if(fixture_mode==9)return fail(e,"current owned Region absent");if(fixture_mode==10)++p.region.region;
 if(calls&&fixture_mode==40)++p.world.version;return true;}
bool same_world(const decltype(Pin{}.world)& x,const decltype(Pin{}.world)& y){return x.version==y.version;}
bool effect(Memory&,const Pin&,d::Sample& out,a::MonsterState& value,std::string& e){++effects;
 if(fixture_mode==11)return fail(e,"current full UID or canonical membership differs");
 if(!calls&&fixture_mode==12)out.undo.active_graph=1;
 if(!calls&&fixture_mode==13){out.money.clock.paused=false;out.money.clock.time_scale=1;}
 if(calls&&fixture_mode==41)value.roster.content_sha256="changed name";
 if(calls&&fixture_mode==42)++out.state.global_random[0];
 if(calls&&fixture_mode==43)++out.state.region_random[0];
 if(calls&&fixture_mode>=44)out.state.marker=fixture_mode;
 return true;}
std::map<std::pair<uintptr_t,size_t>,uintptr_t> fields;
template<class T>bool field(Memory&,uintptr_t base,size_t offset,T& out,std::string& e){
 const auto found=fields.find({base,offset});if(found==fields.end())return fail(e,"actual field read failed");out=T(found->second);return true;}
bool tools(Memory&,uintptr_t,ToolPin&,json&,std::string& e){return fixture_mode!=61||fail(e,"Elite inventory owner differs");}
namespace action_runtime{
unsigned select_default_tool(uintptr_t,uintptr_t){++calls;if(fixture_mode==14)return 0xc0000005u;
 fields[{fixture_cursor,0x190}]=fixture_mode==39?fixture_brush:fixture_desired;
 for(size_t offset:{size_t(0x6fc),size_t(0x740),size_t(0x718),size_t(0x730),size_t(0x738),size_t(0x5b8),size_t(0x5b9),size_t(0x330),size_t(0x3a0)})fields[{fixture_brush,offset}]=0;
 fields[{fixture_shape_a,0x51}]=0;fields[{fixture_shape_b,0x51}]=0;fields[{fixture_cursor,0x175}]=0;
 fields[{window_a,0x45f}]=0;fields[{window_b,0x45f}]=0;
 fields[{fixture_brush,0x338}]=0;fields[{fixture_brush,0x348}]=0;
 const std::array<std::pair<uintptr_t,size_t>,15> broken{{{fixture_brush,0x6fc},{fixture_brush,0x740},{fixture_brush,0x718},{fixture_brush,0x730},{fixture_brush,0x738},
 {fixture_brush,0x5b8},{fixture_brush,0x5b9},{fixture_brush,0x330},{fixture_brush,0x3a0},{fixture_shape_a,0x51},{fixture_shape_b,0x51},{fixture_cursor,0x175},{window_a,0x45f},{window_b,0x45f},{fixture_brush,0x5d8}}};
 if(fixture_mode>=20&&fixture_mode<=34){if(fixture_mode==34)fields[{fixture_brush,0x5d8}]=0;else fields[broken[fixture_mode-20]]=1;}
 if(fixture_mode==35)fields.erase({fixture_brush,0x740});
 if(fixture_mode==62)fields[{fixture_brush,0x338}]=1;
 return 0;}
}
#include "monster_zone_tool_exit_unchanged_body.inc"
#include "monster_zone_tool_exit_body.inc"
void setup(unsigned current){fixture_mode=current;calls=0;effects=0;state={};fields={
 {{image,0x13df9d0},fixture_cursor},{{fixture_cursor,0},image+0x1786dd0},{{fixture_cursor,0x190},fixture_brush},{{fixture_cursor,0x198},fixture_desired},
 {{fixture_desired,0},image+0x17391a0},{{fixture_desired,0x120},1},{{fixture_brush,0},image+0x172fa80},{{image+0x172fa80,0xb0},image+0x5effd0},
 {{fixture_brush,0x170},fixture_region},{{fixture_region,0x834},63},{{fixture_brush,0x6fc},0},{{fixture_brush,0x740},0},{{fixture_brush,0x718},1},{{fixture_brush,0x730},2},{{fixture_brush,0x738},3},
 {{fixture_brush,0x5b8},1},{{fixture_brush,0x5b9},1},{{fixture_brush,0x330},2},{{fixture_brush,0x3a0},2},{{fixture_brush,0x4a0},fixture_shape_a},{{fixture_brush,0x4a8},fixture_shape_b},
 {{fixture_shape_a,0x51},1},{{fixture_shape_b,0x51},1},{{fixture_cursor,0x175},1},{{fixture_brush,0x5d8},window_a},{{fixture_brush,0x5e0},window_b},
 {{window_a,0x45f},1},{{window_b,0x45f},1},{{fixture_brush,0x148},window_a},{{fixture_brush,0x150},window_b},{{fixture_brush,0x338},1},{{fixture_brush,0x348},1},
 {{image+0x1739500,0xb0},image+0x5be5c0}};
 if(current==15)fields[{fixture_brush,0x6fc}]=1;if(current==16)fields[{fixture_brush,0x740}]=1;
 if(current==17)fields[{fixture_brush,0}]=image+0x123456;
 if(current==18)fields[{image+0x172fa80,0xb0}]=image+0x5be5c0;
 if(current==19)fields.erase({fixture_brush,0x4a0});
 if(current==60||current==61||current==62)fields[{fixture_brush,0}]=image+0x1739500;
 if(current==63)fields[{fixture_cursor,0x190}]=fixture_desired;
}
int main(){try{Memory memory;unsigned cases=0;std::string e;json receipt;
 setup(0);need(info_default_tool(memory,receipt,e),e);need(calls==1&&receipt.at("switch_returned")==true&&receipt.at("cleanup_confirmed")==true&&
 receipt.at("default_confirmed")==true&&receipt.at("gameplay_unchanged")==true&&receipt.at("before")==receipt.at("after"),"Zone original switch receipt differs");++cases;
 for(unsigned current=1;current<=19;++current){setup(current);e.clear();need(!info_default_tool(memory,receipt,e),"guard accepted fixture_mode="+std::to_string(current));
 need(!e.empty(),"guard lost upstream cause");need(calls==(current==14?1u:0u),"guard executed original before proof");++cases;}
 for(unsigned current=20;current<=35;++current){setup(current);e.clear();need(!info_default_tool(memory,receipt,e)&&calls==1,"cleanup guard accepted fixture_mode="+std::to_string(current));++cases;}
 for(unsigned current:{39u,40u,41u,43u,44u,45u}){setup(current);e.clear();need(!info_default_tool(memory,receipt,e)&&calls==1,"readback accepted fixture_mode="+std::to_string(current));++cases;}
 setup(42);e.clear();need(info_default_tool(memory,receipt,e)&&receipt.at("global_random_unchanged")==false&&receipt.at("region_random_unchanged")==true,
 "UI Global RNG must remain observable without invented equality");++cases;
 for(unsigned i=0;i<21;++i){setup(100+i);e.clear();need(!info_default_tool(memory,receipt,e)&&receipt.at("before")["ledger"][i]!=receipt.at("after")["ledger"][i],"ledger group changed without rejection");++cases;}
 for(unsigned current:{60u,61u,62u,63u}){setup(current);e.clear();const bool expected=current==60||current==63;
 need(info_default_tool(memory,receipt,e)==expected,"existing Elite/default path differs");need(calls==((current==60||current==62)?1u:0u),"existing native call count differs");++cases;}
 std::cout<<cases<<" actual MonsterZone Default switch cases passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
