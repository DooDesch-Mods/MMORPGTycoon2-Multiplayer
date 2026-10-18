#include "typed_action/action_session.h"
#include "ui_remove/diagnostic/ui_diagnostic.h"
#include <cstring>
#include <iostream>
namespace a=mt2mp::action_session;
using json=nlohmann::json;
void need(bool v,const std::string& e){if(!v)throw std::runtime_error(e);}
bool fail(std::string& e,const std::string& why){e=why;return false;}
constexpr uintptr_t image=0x10000000,manager=1000,fixture_object=2000,handler=3000,fixture_window=4000,fixture_proxy=5000;
constexpr const char* birth_id="33333333-3333-4333-8333-333333333333";
unsigned mode=0;
struct Memory{};
struct Pin{struct{uintptr_t selection=manager;}world;};
namespace d{struct Sample{};}
namespace monster_zone_read{struct Monster{uintptr_t pointer=fixture_object;uint64_t uid=0x100000001ull;};}
struct Operation{bool confirmed=true,retired=true;struct{std::string snapshot_uuid="snapshot";struct{int region=63;}monster;}request;uintptr_t pointer=fixture_object;uint64_t uid=0x100000001ull;};
struct Runtime{uintptr_t image=::image;unsigned thread=1;bool ready=true;std::map<std::string,Operation> operations{{birth_id,{}}};}state;
unsigned GetCurrentThreadId(){return 1;}
namespace net{std::string current_snapshot_id(){return "snapshot";}}
namespace typed_action_runtime{bool network_binding(const std::string&,a::Binding& out,std::string& e){out={3,4};return mode!=15||fail(e,"world binding changed");}}
bool move_target(Memory&,const a::Binding&,const std::string&,int,const std::string&,Pin&,d::Sample&,monster_zone_read::Monster& row,std::string& e){
 if(mode==16)return fail(e,"full UID high word/current manager mismatch");if(mode==17)row.uid=1;if(mode==18)++row.pointer;return true;}
bool effect(Memory&,const Pin&,d::Sample&,a::MonsterState&,std::string& e,uint64_t){return mode!=19||fail(e,"mixed canonical admission failed");}
template<class T>bool field(Memory&,uintptr_t base,size_t off,T& out,std::string& e){
 std::map<std::pair<uintptr_t,size_t>,uintptr_t> fields{{{manager,0x30},1},{{manager,0x80},fixture_object},{{manager,0xb8},handler},
  {{handler,0},image+0x1731660},{{handler,0x10},fixture_window},{{fixture_window,0},image+0x1724f40},{{fixture_window,0x45f},1},
  {{image+0x1724f40,0x168},image+0x7490c0},{{fixture_window,0x570},fixture_object},{{fixture_window,0x578},fixture_proxy},{{fixture_object,0x28},fixture_proxy},{{fixture_proxy,0},fixture_object+0x20}};
 const std::array<std::pair<uintptr_t,size_t>,10> broken{{{manager,0x30},{manager,0x80},{handler,0},{handler,0x10},{fixture_window,0},{fixture_window,0x45f},{image+0x1724f40,0x168},{fixture_window,0x570},{fixture_window,0x578},{fixture_proxy,0}}};
 if(mode>=1&&mode<=10)fields[broken[mode-1]]=0;
 auto f=fields.find({base,off});if(f==fields.end())return fail(e,"unreadable actual fixture_window field");out=T(f->second);return true;
}
#include "monster_info_window_body.inc"
#include "monster_info_target_body.inc"
int main(){try{unsigned n=0;std::string e;
 for(unsigned current=0;current<=10;++current){mode=current;Memory m;a::ui_capture::LocalBinding local{manager,fixture_object,0x100000001ull,3,4,"snapshot",std::string("action:")+birth_id};a::ui_diagnostic::Window out;json receipt;
  need(info_window(m,local,out,receipt,e)==(current==0),"actual Monster fixture_window ownership guard missed mode="+std::to_string(current));++n;}
 for(unsigned current=11;current<=19;++current){mode=current;state=Runtime{};
  if(current==11)state.operations.clear();if(current==12)state.operations.at(birth_id).confirmed=false;
  if(current==13)state.operations.at(birth_id).retired=false;if(current==14)state.operations.at(birth_id).request.snapshot_uuid="old";
  a::Binding binding;Pin p;d::Sample sample;a::MonsterState value;a::ui_capture::LocalBinding local;
  need(!info_target(birth_id,binding,p,sample,value,local,e),"unbound/stale/replaced birth_id exposed to native UI");++n;}
 mode=0;state=Runtime{};a::Binding binding;Pin p;d::Sample sample;a::MonsterState value;a::ui_capture::LocalBinding local;
 need(info_target(birth_id,binding,p,sample,value,local,e)&&local.uid==0x100000001ull&&local.object==fixture_object&&local.manager==manager&&local.native_epoch==3&&local.native_generation==4,"valid original UI birth_id binding differs");++n;
 std::cout<<n<<" actual Monster birth_id and InfoWindow owner cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
