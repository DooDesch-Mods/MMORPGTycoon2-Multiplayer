#include "direct_monster_fixture.h"
#include <optional>
#include <iostream>
#include <cstring>
namespace a=mt2mp::action_session;using json=nlohmann::json;
struct Memory{bool copy(uintptr_t from,void* to,size_t n,std::string&){std::memcpy(to,reinterpret_cast<void*>(from),n);return true;}};
struct Origin{uintptr_t mode=99;};struct World{Origin origin;};struct Region{uintptr_t region=300;};struct Pin{World world;Region region;};
struct ToolPin{uintptr_t behaviour=100,cursor=200;std::array<int32_t,2> character_id{1,0};int rank=1,level=1;uint8_t loose=0;};
struct LevelState{json output={{"ready",true}};};
namespace d{struct Sample{int state=0;struct{uintptr_t active_graph=0;}undo;};json sample_json(const Sample& s){return s.state;}}
struct Pending{a::MonsterIntent intent;a::Binding binding;std::string snapshot,token;};
struct Runtime{uintptr_t image=0x140000000;uint32_t thread=7;bool replaying=false,ready=true,fault=false;
 std::optional<Pending> pending;uint64_t captures=0;std::string cause,ui_token;std::map<std::string,json> ui_jobs;void(*release)(void*,void*,const float*)=nullptr;}state;
bool connected=true;unsigned thread=7,original_calls=0,read_count=0;std::string fault;
namespace net{std::string current_snapshot_id(){return snapshot_id;}namespace session_wire{bool connected(){return ::connected;}}}
unsigned GetCurrentThreadId(){return thread;}
void* test_return_address(){return reinterpret_cast<void*>(state.image+0x587e6c+(fault=="caller"?1:0));}
#define _ReturnAddress test_return_address
bool reject(const std::string& e){state.cause=e;return false;}
bool gate(const char* name,std::string& e){if(fault==name){e=std::string(name)+" failed";return false;}return true;}
namespace typed_action_runtime{
json controller_status(){return {{"started",true},{"locked",fault=="locked"},{"pending",false}};}
bool network_binding(const std::string&,a::Binding& b,std::string& e){b={3,4};return gate("binding",e);}}
bool code(Memory&,bool,std::string& e){return gate("code",e);}
bool tools(Memory&,uintptr_t,ToolPin& t,json&,std::string& e){t={};return gate("tool",e);}
template<class T>bool field(Memory&,uintptr_t,size_t offset,T& out,std::string& e){
 if(!gate("field",e))return false;
 if constexpr(std::is_same_v<T,std::array<float,4>>)out=intent().rotation;
 else if constexpr(std::is_same_v<T,std::array<float,3>>)out=intent().scale;
 else if constexpr(std::is_same_v<T,uintptr_t>)out=300;
 else if constexpr(std::is_same_v<T,int32_t>)out=63;
 else if constexpr(std::is_same_v<T,uint8_t>)out=(offset==0x3e6&&fault!="pressed")?1:0;
 return true;}
bool pin(Memory&,const a::Binding&,const std::string&,int32_t,Pin& p,std::string& e){p={};if(fault=="exception")throw std::runtime_error("native owner read exception");return gate("pin",e);}
bool selection(Memory&,uintptr_t,uintptr_t,ToolPin&,json&,std::string& e){return gate("selection",e);}
bool pose(Memory&,const ToolPin&,const a::MonsterIntent&,uintptr_t,std::string& e){return gate("pose",e);}
bool region_state(Memory&,uintptr_t,const World&,const Region&,const ToolPin&,json& out,std::string& e){out={{"ready",fault!="region"}};return gate("region_read",e);}
bool level_state(Memory&,uintptr_t,const World&,const Region&,const ToolPin&,LevelState& out,std::string& e){out.output={{"ready",fault!="level"}};return gate("level_read",e);}
bool tutorial_clear(Memory&,uintptr_t,const World&,json&,bool& clear,std::string& e){clear=fault!="tutorial";return gate("tutorial_read",e);}
bool effect(Memory&,const Pin&,d::Sample& sample,a::MonsterState& value,std::string& e){sample.state=fault=="changed"?int(read_count):0;++read_count;sample.undo.active_graph=fault=="undo"?1:0;value=empty();return gate("effect",e);}
#include "monster_capture_body.inc"
void original(void*,void*,const float*){++original_calls;}
void reset(){state={};state.release=original;connected=true;thread=7;original_calls=read_count=0;fault.clear();}
void call(){const auto p=intent().position;release_entry(reinterpret_cast<void*>(100),reinterpret_cast<void*>(200),p.data());}
int main(){try{unsigned count=0;reset();call();need(!original_calls&&state.pending&&state.captures==1&&state.pending->intent==intent(),"valid normal Release did not capture without native effect");++count;
 call();need(!original_calls&&state.captures==1,"pending capture replayed original Release");++count;
 for(const auto* f:{"caller","locked","binding","code","tool","field","pressed","pin","selection","pose","region","level","tutorial","undo","changed","effect"}){reset();fault=f;call();need(!original_calls&&!state.pending&&!state.cause.empty(),std::string("invalid capture reached original: ")+f);++count;}
 reset();thread=8;call();need(!original_calls&&!state.pending,"wrong Mainthread captured");++count;
 reset();fault="exception";call();need(state.fault&&!state.pending&&!original_calls&&state.cause.find("native owner read exception")!=std::string::npos,"capture exception cause hidden");++count;
 reset();connected=false;call();need(original_calls==1&&!state.pending,"offline original Release blocked");++count;
 reset();state.replaying=true;call();need(original_calls==1&&!state.pending,"ordered original Release blocked");++count;
 std::cout<<count<<" actual capture body cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
