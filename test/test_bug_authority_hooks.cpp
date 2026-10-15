#include "bug_authority/hooks.cpp"
#include <iostream>
#include <map>
namespace bugs=mt2mp::game::bug_authority;
namespace fixture {
std::map<uintptr_t,unsigned char> bytes;
bool active=false,context_ok=true,owned_writer=true,anchor_handled=false;
int original_calls=0,births=0,removals=0,anchors=0,checks=0;
mt2mp::action_session::Role role=mt2mp::action_session::Role::Host;
constexpr uintptr_t parent=0x1000,collection=0x2000,bug=0x3000,stream=0x4000,context=0x5000,property=0x6000,map=0x7000;
std::vector<uintptr_t> members;
template<class T>void put(uintptr_t p,const T& value){const auto* input=reinterpret_cast<const unsigned char*>(&value);for(size_t n=0;n<sizeof(T);++n)bytes[p+n]=input[n];}
void require(bool yes,const char* cause){++checks;if(!yes)throw std::runtime_error(cause);}
uintptr_t random(uintptr_t){++original_calls;return bug;}
uintptr_t specific(uintptr_t,int,float){++original_calls;return bug;}
void anchor(uintptr_t,uintptr_t){++anchors;}
void remove(uintptr_t,uintptr_t){++original_calls;}
bool save(uintptr_t,uintptr_t,uintptr_t writer,uintptr_t ctx){++original_calls;bugs::saved_child(0xdc19e1,bug,writer,ctx);return true;}
bool load(uintptr_t,uintptr_t,uintptr_t reader,uintptr_t ctx){++original_calls;bugs::original_children(0xbae563,reinterpret_cast<void*>(reader),1);members={bug};bugs::original_registered(0xbae8b5,reinterpret_cast<void*>(ctx),9,reinterpret_cast<void*>(bug));return true;}
void record(uintptr_t,uintptr_t,uintptr_t ctx){++original_calls;members={bug};bugs::original_registered(0xbae470,reinterpret_cast<void*>(ctx),0,reinterpret_cast<void*>(bug));}
void reset(){auto& r=bugs::runtime();bytes.clear();members.clear();active=false;context_ok=owned_writer=true;anchor_handled=false;role=mt2mp::action_session::Role::Host;original_calls=births=removals=anchors=0;
 r.image=0x100000000;r.ready=true;r.locked=false;r.cause.clear();r.snapshot_armed=false;r.snapshot_host=false;r.snapshot_uuid="snapshot";r.saved.clear();r.action.clear();r.binding.native_epoch=7;r.binding.native_generation=9;r.tracker=0x8000;
 put(collection,r.image+0x1705640);put(collection+0x48,parent);put(parent+0x170,map+0x1c0);put(property+0x38,uint64_t(0x18));put(bug,r.image+0x1742200);put(bug+8,context);
 bugs::original_random=random;bugs::original_specific=specific;bugs::original_anchor=anchor;bugs::original_remove=remove;bugs::original_save=save;bugs::original_load=load;bugs::original_record=record;
}
}
namespace placement_ui_probe {bool ProcessMemory::copy(uintptr_t p,void* target,size_t n,std::string& error){auto* output=static_cast<unsigned char*>(target);for(size_t i=0;i<n;++i){auto it=fixture::bytes.find(p+i);if(it==fixture::bytes.end()){error="fixture missing field";return false;}output[i]=it->second;}return true;}}
namespace mt2mp {Logger& Logger::instance(){static Logger logger;return logger;}void Logger::log(LogCategory,LogLevel,const char*,int,const std::string& text){std::cerr<<text<<'\n';}}
namespace mt2mp::game::bug_authority {
Runtime& runtime(){static Runtime value;return value;}
bool reject(const std::string& cause,std::string& error){runtime().locked=true;runtime().cause=error=cause;return false;}
bool active_world(Context& context,Binding& binding,uint64_t& publication,bool& active,std::string& error){active=fixture::active;context.role=fixture::role;binding=runtime().binding;publication=5;if(!fixture::context_ok){error="actual Context unavailable";return false;}return true;}
bool witness_birth(uintptr_t collection,uintptr_t pointer,unsigned lane,std::string& error){if(collection!=fixture::collection||pointer!=fixture::bug||lane<1||lane>3)return fail(error,"fixture incorrect birth");++fixture::births;return true;}
bool witness_removal(uintptr_t,uintptr_t,std::string&){++fixture::removals;return true;}
bool local_members(uintptr_t collection,std::vector<uintptr_t>& out,std::string& error){if(collection!=fixture::collection)return fail(error,"fixture incorrect collection");out=fixture::members;return true;}
}
namespace mt2mp::game::building_capture_identity {bool stream_owner(void*,StreamOwner& out,bool& owned,std::string&){out={};out.capture_uuid=bugs::runtime().snapshot_uuid;owned=fixture::owned_writer;return true;}}
namespace mt2mp::game::native_open_runtime {bool read_current_world(building_mixed_identity::opened_file::CurrentWorld& out,std::string&){out={};out.world_epoch=7;out.generation=9;out.map=reinterpret_cast<void*>(fixture::map);return true;}}
namespace mt2mp::game::developer_staff::bug_creation {
bool NativeServices::dispatch_anchor(uintptr_t,uintptr_t,uintptr_t){return fixture::anchor_handled;}
bool create(Services&,const Lease&,const Value&,Receipt& receipt){receipt.confirmed=true;return true;}
}
extern "C" MH_STATUS WINAPI MH_CreateHook(LPVOID,LPVOID,LPVOID*){return MH_ERROR_UNSUPPORTED_FUNCTION;}
extern "C" MH_STATUS WINAPI MH_EnableHook(LPVOID){return MH_ERROR_NOT_CREATED;}
int main(){using fixture::require;
 fixture::reset();require(bugs::add_random(fixture::collection)==fixture::bug&&fixture::original_calls==1,"standalone original random forwarded");
 fixture::reset();fixture::active=true;fixture::role=mt2mp::action_session::Role::Client;require(bugs::add_random(fixture::collection)==0&&fixture::original_calls==0,"exact bound Client random suppressed");
 fixture::reset();fixture::active=true;require(bugs::add_random(fixture::collection)==fixture::bug&&fixture::original_calls==1&&fixture::births==1,"Host original random forwards and witnesses birth");
 fixture::reset();fixture::active=true;fixture::role=mt2mp::action_session::Role::Client;fixture::put(fixture::parent+0x170,uintptr_t(123));
 require(bugs::add_random(fixture::collection)==fixture::bug&&fixture::original_calls==1&&bugs::runtime().locked,"foreign receiver is not silently suppressed");
 fixture::reset();fixture::context_ok=false;require(bugs::add_random(fixture::collection)==fixture::bug&&bugs::runtime().locked,"unproved context keeps original and visible failure");
 fixture::reset();bugs::set_anchor(fixture::bug,fixture::parent);require(fixture::anchors==1,"unowned anchor uses original");
 fixture::anchor_handled=true;bugs::set_anchor(fixture::bug,fixture::parent);require(fixture::anchors==1,"owned selected anchor never falls through");
 fixture::reset();bugs::remove_tracker(0x8000,fixture::bug);require(fixture::original_calls==1&&fixture::removals==1,"original Tracker removal ends occurrence exactly once");
 fixture::reset();bugs::runtime().snapshot_armed=true;bugs::runtime().snapshot_host=true;fixture::members={fixture::bug};
 require(bugs::save_property(fixture::property,fixture::collection,fixture::stream,fixture::context),"original save returns");
 require(!bugs::runtime().locked&&bugs::runtime().saved.size()==1&&bugs::runtime().saved[0].children==fixture::members,"Host identity comes from original serialized children");
 fixture::reset();fixture::members={fixture::bug};require(bugs::save_property(fixture::property,fixture::collection,fixture::stream,fixture::context)&&bugs::runtime().saved.empty(),"local undistributed save does not arm or replace identities");
 fixture::reset();bugs::runtime().snapshot_armed=true;bugs::runtime().snapshot_host=true;fixture::owned_writer=false;fixture::members={fixture::bug};
 require(bugs::save_property(fixture::property,fixture::collection,fixture::stream,fixture::context)&&bugs::runtime().saved.empty(),"foreign snapshot writer remains unobserved");
 fixture::reset();bugs::runtime().snapshot_armed=true;require(bugs::load_property(fixture::property,fixture::collection,fixture::stream,fixture::context),"original load returns");
 require(!bugs::runtime().locked&&bugs::runtime().saved.size()==1&&bugs::runtime().saved[0].children==fixture::members,"Client identity comes from actual child registration");
 fixture::reset();bugs::Scoped scoped;scoped.value.children={fixture::collection,fixture::stream,fixture::context,-1,{}};
 bugs::original_children(0xbae711,reinterpret_cast<void*>(fixture::stream),9);require(scoped.value.children.declared==-1,"inner Bug property count cannot become collection count");
 bugs::original_children(0xbae563,reinterpret_cast<void*>(fixture::stream),1);require(scoped.value.children.declared==1,"outer original collection count accepted");
 bugs::original_registered(0xbae8b5,reinterpret_cast<void*>(fixture::context+1),9,reinterpret_cast<void*>(fixture::bug));require(bugs::runtime().locked&&scoped.value.children.witnessed.empty(),"foreign original ObjectContext cannot create identity");
 fixture::reset();bugs::runtime().action="ordered-history";bugs::load_record(fixture::collection+0x18,fixture::stream,fixture::context);
 require(fixture::births==1&&!bugs::runtime().locked,"actual original history child obtains action birth");
 std::cout<<fixture::checks<<" actual native hook boundary checks PASS\n";
}
