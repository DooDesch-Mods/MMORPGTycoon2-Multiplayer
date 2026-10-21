#include "native_open_witness.h"
#include "vendor/nlohmann/json.hpp"
#include <map>
#include <vector>
#include <set>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game;
using World=building_mixed_identity::opened_file::CurrentWorld;
constexpr uintptr_t image=0x140000000,game_addr=0x1000,mode_addr=0x2000,state_addr=0x3000,log_addr=0x4000,library_addr=0x5000,asset_addr=0x6000,entries_addr=0x7000;
uint32_t mainthread=7,thread_id=7;
uint32_t GetCurrentThreadId(){return thread_id;}
std::string fault;
struct Load {std::string uuid="world";uint64_t generation=1;bool armed=true,ready=false;std::set<uintptr_t> sequences;bool waiting_postload=false;} load;
struct Creation {World world;uintptr_t log=log_addr,asset=asset_addr;std::string id="initial_tutorial";bool entered=false;};
Creation* creation=nullptr;
std::map<uintptr_t,unsigned char> bytes;
std::map<uintptr_t,std::string> strings;
bool origin_available=false,resolver_available=true;
unsigned origin_reads=0,original_calls=0,checks=0;
World actual_world;
building_mixed_identity::opened_file::Status request_status;
bool fail(std::string& e,const std::string& s){e=s;return false;}
void block(const std::string& s){if(fault.empty())fault=s;}
void need(bool value,const char* text){if(!value)throw std::runtime_error(text);++checks;}
template<class T>void seed(uintptr_t address,T value){auto b=reinterpret_cast<unsigned char*>(&value);for(size_t i=0;i<sizeof(T);++i)bytes[address+i]=b[i];}
template<class T>bool at(uintptr_t address,size_t offset,T& value,std::string& e){auto b=reinterpret_cast<unsigned char*>(&value);for(size_t i=0;i<sizeof(T);++i){auto f=bytes.find(address+offset+i);if(f==bytes.end())return fail(e,"actual fixture memory missing");b[i]=f->second;}return true;}
bool str(uintptr_t address,size_t offset,std::string& value,std::string& e){auto f=strings.find(address+offset);if(f==strings.end())return fail(e,"actual fixture string missing");value=f->second;return true;}
bool type(uintptr_t address,uintptr_t rva,std::string& e){uintptr_t v=0;return at(address,0,v,e)&&(v==image+rva||fail(e,"actual fixture type differs"));}
bool supported(const std::string& s){return s=="initial_tutorial"||s=="leveltwo_tutorial";}
bool same_world(const World& a,const World& b){return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&a.uuid==b.uuid&&a.generation==b.generation&&a.world_epoch==b.world_epoch&&a.sha256==b.sha256&&a.bytes==b.bytes;}
namespace mt2mp::game::native {bool read_resolver_receipt(uintptr_t& base,uint32_t& thread,std::string& sha,std::string& e){if(!resolver_available)return fail(e,"resolver original cause");base=image;thread=mainthread;sha="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";return true;}}
namespace mt2mp::game::building_mixed_identity::opened_file {Status status(){return request_status;}}
namespace native_open_runtime {bool read_current_world(World& w,std::string& e){++origin_reads;if(!origin_available)return fail(e,"current_world_read_origin_unavailable");w=actual_world;return true;}}
namespace tutorial_observer {bool read(nlohmann::json& out,std::string&){out=nlohmann::json::object();return true;}}
template<class... Args>void fixture_log(const char*,const Args&...){}
#define LOG_INFO fixture_log
void original_scenario_start(void*,void*,void*){++original_calls;}
#include "load_start_owners.inc"
namespace intro_native {void start(void*,void*,void*){throw std::runtime_error("unexpected Intro in fixed Tutorial load fixture");}}
#include "load_start_hook.inc"
#include "load_start_finish.inc"
void array_seed(uintptr_t owner,size_t offset,uintptr_t data,int32_t count){seed(owner+offset,data);seed(owner+offset+8,count);seed(owner+offset+12,count);}
void reset(){bytes.clear();strings.clear();fault.clear();load={};creation=nullptr;origin_available=false;resolver_available=true;origin_reads=original_calls=0;thread_id=mainthread;
 actual_world={reinterpret_cast<void*>(game_addr),reinterpret_cast<void*>(mode_addr),reinterpret_cast<void*>(state_addr),reinterpret_cast<void*>(0x8000),reinterpret_cast<void*>(0x9000),1,1,99,"world","hash"};
 request_status={};request_status.uuid="world";request_status.generation=1;request_status.game_thread=mainthread;request_status.initialized=true;request_status.hook_mask=15;request_status.native_request_attempted=true;request_status.state="loading";
 seed(image+0x13df8f0,game_addr);seed(game_addr,image+0x1743740);seed(image+0x13df320,mode_addr);seed(mode_addr,image+0x16e6730);seed(game_addr+0x28,mode_addr);seed(mode_addr+0x2c4,int32_t(3));seed(mode_addr+0x58,state_addr);seed(state_addr,image+0x16de920);
 seed(state_addr+0x1e8,log_addr);seed(image+0x13df330,log_addr);seed(log_addr,image+0x16fd7b0);array_seed(log_addr,0x20,0,0);array_seed(log_addr,0x50,0,0);seed(log_addr+0x60,uint8_t(1));
 seed(mode_addr+0x48,library_addr);seed(library_addr,image+0x171bd80);seed(image+0x13df560,library_addr);array_seed(library_addr,0x20,entries_addr,1);seed(entries_addr,asset_addr);seed(asset_addr,image+0x1720510);seed(asset_addr+0x58,uint8_t(0));strings[asset_addr+0x18]="initial_tutorial";strings[asset_addr+0x60]="scenarios/initial_tutorial";
}
void start(){scenario_start(reinterpret_cast<void*>(log_addr),reinterpret_cast<void*>(asset_addr),nullptr);}
int main(){try{
 reset();start();need(fault.empty()&&!original_calls&&!origin_reads,"early unscoped Initial touched completed Origin or original");
 bool pending=true;std::string e;origin_available=true;need(finish_client_load(pending,e)&&!pending&&load.ready&&fault.empty(),"subsequent real load completion failed after early suppression");
 auto reject=[](auto change,const char* label){reset();change();start();need(!fault.empty()&&!original_calls&&!origin_reads,label);};
 reject([]{thread_id=8;},"wrong thread accepted");reject([]{request_status.uuid="foreign";},"foreign UUID accepted");reject([]{request_status.generation=2;},"foreign generation accepted");reject([]{request_status.native_request_attempted=false;},"unattempted request accepted");
 reject([]{seed(game_addr+0x28,uintptr_t(0x9999));},"foreign active Mode accepted");reject([]{seed(mode_addr+0x2c4,int32_t(1));},"Title Mode state accepted");reject([]{seed(state_addr,image+0x999);},"wrong GameState type accepted");
 reject([]{seed(image+0x13df330,uintptr_t(0x9999));},"foreign Log singleton accepted");reject([]{seed(image+0x13df560,uintptr_t(0x9999));},"foreign library accepted");reject([]{strings[asset_addr+0x60]="scenarios/foreign";},"foreign asset filename accepted");reject([]{seed(asset_addr+0x58,uint8_t(1));},"repeatable asset accepted");
 reject([]{array_seed(library_addr,0x20,entries_addr,2);seed(entries_addr+8,asset_addr);},"duplicate library asset accepted");
 reset();request_status.error="retained native reader failure";start();need(fault==request_status.error,"native request cause lost");
 reset();scenario_start(reinterpret_cast<void*>(log_addr),reinterpret_cast<void*>(asset_addr),reinterpret_cast<void*>(1));need(!fault.empty()&&!original_calls,"bootstrap parameters accepted");
 reset();Creation scope{actual_world};creation=&scope;start();need(fault=="current_world_read_origin_unavailable"&&origin_reads==1&&!original_calls,"scoped creation weakened completed Origin");
 reset();scope={actual_world};creation=&scope;origin_available=true;start();need(fault.empty()&&original_calls==1&&origin_reads==1,"valid scoped original did not run once");
 reset();scope={actual_world};creation=&scope;origin_available=true;scope.world.sha256="foreign";start();need(!fault.empty()&&!original_calls,"scoped full Origin mismatch accepted");
 reset();load.ready=true;start();need(fault=="current_world_read_origin_unavailable"&&origin_reads==1&&!original_calls,"ready Client Origin guard weakened");
 reset();strings[asset_addr+0x18]="streamer";start();need(fault.empty()&&original_calls==1&&!origin_reads,"unsupported original route changed");
 reset();strings[asset_addr+0x18]="intro_cutscene";strings.erase(asset_addr+0x60);seed(asset_addr,image+0x1708d60);Owners intro;
 need(bootstrap_owners("intro_cutscene",intro,e)&&intro.asset==asset_addr&&intro.log==log_addr,"actual Intro catalogue owner incorrectly requires a Sequence filename");
 seed(asset_addr,image+0x1720510);need(!bootstrap_owners("intro_cutscene",intro,e),"Sequence Scenario class accepted as Intro");
 seed(asset_addr,image+0x1708d60);seed(asset_addr+0x58,uint8_t(1));need(!bootstrap_owners("intro_cutscene",intro,e),"repeatable Intro catalogue owner accepted");
 std::cout<<"PASS "<<checks<<" actual bootstrap/Start/load-completion cases\n";return 0;
 }catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
