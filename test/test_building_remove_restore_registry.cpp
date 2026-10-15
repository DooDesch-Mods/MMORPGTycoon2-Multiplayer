#include "remove_diagnostics.h"
#include "snapshot_building_key.h"
#include "object_identity/remove_runtime/copy/prop_lease.h"
#include "mt2mp/game/native_engine.h"
#include <atomic>
#include <iostream>
#include <map>
#include <stdexcept>
using json=nlohmann::json;
using namespace mt2mp::game;
namespace lease=mt2mp::prop_lease;
namespace prop_lease=mt2mp::prop_lease;
namespace {
int checks=0,lookups=0,reads=0;void need(bool b,const std::string& e){++checks;if(!b)throw std::runtime_error(e);}
const std::string snap="11111111-1111-4111-8111-111111111111",fixture_token="22222222-2222-4222-8222-222222222222",fixture_key=std::string(64,'a')+":world:building:44";
uint32_t GetCurrentThreadId(){return 7;}
bool world_ok=true,lookup_ok=true,reader_ok=true,revoke_ok=true;uintptr_t live_pointer=0;uint32_t fixture_thread=7;
const lease::World actual_world{2,3,reinterpret_cast<void*>(0x1000),reinterpret_cast<void*>(0x2000)};
namespace host {
namespace opened {struct CurrentWorld {uint64_t world_epoch=2,generation=3;void* world=reinterpret_cast<void*>(0x1000);void* map=reinterpret_cast<void*>(0x2000);};}
struct ProcessMemory{};struct LookupApi{};
bool resolve_lookup(ProcessMemory&,uintptr_t,const std::string&,LookupApi&,std::string&){return true;}
}
namespace native {
using BuildingState=mt2mp::game::native::BuildingState;
bool read_resolver_receipt(uintptr_t& image,uint32_t& thread,std::string& hash,std::string& e){image=0x4000;thread=fixture_thread;hash="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";e.clear();return true;}
}
namespace net {std::string current_snapshot_id(){return snap;}}
namespace rt {
struct Request {uint64_t full_uid=3176;lease::World world=actual_world;uintptr_t object=0x3000;std::string snapshot_binding=snap+":"+fixture_key;};
struct Root {uint64_t entry_id=4,registry_generation=1;};
struct Commit {bool confirmed=true,unknown_locked=false;};
struct Record {Request request;Root root;Commit commit;uintptr_t committed_graph=0x7000;};
struct Effects {bool confirmed=true,unknown=false;};
struct Journal {Record saved;Effects effects_;const Record* record()const{return &saved;}const Effects& effects()const{return effects_;}};
struct Runtime {Journal journal;bool exists=true;static Runtime& instance(){static Runtime r;return r;}const Journal* private_retirement_status(uint64_t)const{return exists?&journal:nullptr;}};
}
#include "registry_binding.inc"
struct Attempt {json arguments,receipt;};
std::map<std::string,Bound> registry;std::map<std::string,Attempt> attempts,retirements;uint64_t registry_generation=1;std::atomic<uint32_t> main_thread{7};
std::string binding_id(const std::string& s,const std::string& k){return s+":"+k;}
std::string registry_id(const std::string& s,const std::string& k){return std::to_string(registry_generation)+":"+binding_id(s,k);}
struct Provider {bool verify_actual_revocation(const rt::Request&,const rt::Root&,std::string& e){if(!revoke_ok){e="original sealed revocation differs";return false;}return true;}}retirement_provider;
bool current(host::opened::CurrentWorld& out,std::string& e){out={};if(!world_ok){e="original NativeOpen owner failure";return false;}return true;}
bool lookup(host::LookupApi&,uint64_t,void*& out,uint32_t& seh){++lookups;seh=0;out=reinterpret_cast<void*>(live_pointer);return lookup_ok;}
json fixture_metadata={{"type","SmallInn"},{"state","exact"}};
json building_metadata(const native::BuildingState&){return fixture_metadata;}
bool read_registered_building(uint64_t,host::opened::CurrentWorld& out,native::BuildingState&,uintptr_t& object,uintptr_t& ap,std::string& e){++reads;out={};object=0x9000;ap=0xa000;if(!reader_ok){e="original fresh registry read failure";return false;}return true;}
#include "registry_history.inc"
void reset(){world_ok=lookup_ok=reader_ok=revoke_ok=true;fixture_thread=7;live_pointer=0;lookups=reads=0;main_thread=7;registry.clear();attempts.clear();retirements.clear();rt::Runtime::instance()={};
Bound b;b.snapshot=snap;b.key=fixture_key;b.uid=3176;b.entry=4;b.generation=1;b.sequence=9;b.world=actual_world;b.revoked=true;b.object=0x3000;b.class_addresspoint=0xa000;b.metadata=fixture_metadata;b.revocation_key="sealed";registry[registry_id(snap,fixture_key)]=b;
attempts[fixture_token]={{{"snapshot_id",snap},{"record_key",fixture_key}},{{"confirmed",true},{"sequence",uint64_t(9)}}};retirements[fixture_token]={json::object(),{{"confirmed",true}}};}
}
int main(){try{std::string e;uintptr_t graph=0;
reset();need(confirmed_remove_graph(fixture_token,snap,fixture_key,3176,graph,e)&&graph==0x7000&&lookups==1,"confirmed original graph not bound: "+e);
need(restore_redone_building(snap,fixture_key,3176,e)&&reads==1,"fresh original receiver not admitted: "+e);auto& saved=registry.at(registry_id(snap,fixture_key));need(saved.object==0x9000&&!saved.revoked&&saved.sequence==0&&saved.revocation_key.empty(),"restored receiver retained expired Remove authority");need(attempts.size()==1&&rt::Runtime::instance().journal.saved.request.object==0x3000,"restoration rewrote original retained receipt");
need(retire_undone_building(snap,fixture_key,3176,e),"Remove Redo baseline tombstone");need(history_target_absent(snap,fixture_key,3176,e),"second inverse falsely requires retired old holder");
for(int kind=0;kind<9;++kind){reset();auto& j=rt::Runtime::instance().journal;if(kind==0)j.effects_.confirmed=false;if(kind==1)j.effects_.unknown=true;if(kind==2)j.saved.request.object++;if(kind==3)j.saved.request.full_uid++;if(kind==4)j.saved.root.entry_id++;if(kind==5)j.saved.root.registry_generation++;if(kind==6)j.saved.request.snapshot_binding="foreign";if(kind==7)j.saved.request.world.epoch++;if(kind==8)rt::Runtime::instance().exists=false;need(!history_target_absent(snap,fixture_key,3176,e)&&lookups==0&&e.find("holder retirement")!=std::string::npos,"unretired/mismatched old lifetime queried native UID");}
reset();live_pointer=0x3000;need(!history_target_absent(snap,fixture_key,3176,e)&&e.find("already live")!=std::string::npos,"cached original lifetime treated as absent");
reset();world_ok=false;need(!history_target_absent(snap,fixture_key,3176,e)&&e=="original NativeOpen owner failure","NativeOpen cause overwritten");
reset();reader_ok=false;need(!restore_redone_building(snap,fixture_key,3176,e)&&e=="original fresh registry read failure"&&registry.begin()->second.revoked,"failed fresh read changed binding");
reset();fixture_metadata["state"]="different";need(!restore_redone_building(snap,fixture_key,3176,e)&&registry.begin()->second.revoked,"changed native state admitted");fixture_metadata["state"]="exact";
reset();revoke_ok=false;need(!confirmed_remove_graph(fixture_token,snap,fixture_key,3176,graph,e)&&graph==0&&e=="original sealed revocation differs","sealed revocation cause lost");
for(int kind=0;kind<5;++kind){reset();if(kind==0)attempts[fixture_token].receipt["confirmed"]=false;if(kind==1)retirements[fixture_token].receipt["confirmed"]=false;if(kind==2)attempts[fixture_token].arguments["record_key"]="foreign";if(kind==3)rt::Runtime::instance().journal.saved.committed_graph=0;if(kind==4)rt::Runtime::instance().journal.saved.commit.unknown_locked=true;need(!confirmed_remove_graph(fixture_token,snap,fixture_key,3176,graph,e)&&graph==0&&!e.empty(),"unconfirmed graph published");}
reset();main_thread=8;need(!history_target_absent(snap,fixture_key,3176,e)&&lookups==0,"foreign thread queried native UID");
std::cout<<"PASS "<<checks<<" actual Root binding/history guard assertions; native reads are controlled test doubles\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
