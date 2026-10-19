#include "typed_action/action_session.h"
#include "npc_quest_runtime_apply.h"
#include <atomic>
#include <functional>
#include <iostream>
#include <map>
#include <cstring>
#include <stdexcept>
namespace a=mt2mp::action_session;namespace action=a;
using Context=a::Context;using Binding=a::Binding;
namespace q=mt2mp::npc_quest_runtime;
namespace npc_quest_runtime=q;
using json=nlohmann::json;
namespace {
unsigned checks=0,thread=7;void need(bool b,const std::string& e){++checks;if(!b)throw std::runtime_error(e);}
bool fail(std::string& e,const std::string& why){e=why;return false;}
unsigned GetCurrentThreadId(){return thread;}
struct Session:a::Session{a::Context value;bool current(a::Context& out,std::string&)override{out=value;return true;}}session;
struct Controller{a::Context c;a::Binding b;a::Status s;a::Status status(){return s;}a::Context bound_context(){return c;}a::Binding bound_binding(){return b;}};
struct Engine{unsigned reads=0;bool current(const a::Binding&,const std::string&,std::string&){++reads;return true;}};
struct Runtime{uint32_t thread=7;Controller controller;Engine engine;Session& session=::session;bool lobby_retiring=false;}fixture_runtime;
std::atomic<Runtime*> published_runtime{&fixture_runtime};
namespace typed_action_runtime{
#include "quest_rating_idle.inc"
}
struct Pin{a::Context context;a::Binding binding;uintptr_t image=0x100000;};
struct QuestOwner{std::string key;uintptr_t npc;std::vector<std::pair<int32_t,uintptr_t>> quests;};
struct Memory:mt2mp::npc_census::Memory{
 inline static std::map<uintptr_t,uint8_t> bytes;
 template<class T>void put(uintptr_t p,T v){const auto* b=reinterpret_cast<const uint8_t*>(&v);for(size_t i=0;i<sizeof v;++i)bytes[p+i]=b[i];}
 bool copy(uintptr_t p,void* out,size_t size,std::string& e)override{auto* b=static_cast<uint8_t*>(out);for(size_t i=0;i<size;++i){auto f=bytes.find(p+i);if(f==bytes.end())return fail(e,"native exact field missing");b[i]=f->second;}return true;}
}memory;
using QuestMemory=Memory;
template<class T>bool field(Memory&,uintptr_t p,size_t n,T& out,std::string& e){return memory.copy(p+n,&out,sizeof out,e);}
struct{uintptr_t image=0x100000;uint32_t thread=7;}state;
struct Fence{
 a::Context context_;a::Binding binding_;bool armed_=true,suspended_=false,locked_=false;std::string cause_;
#include "quest_rating_binding.inc"
 bool locked(){return locked_;}const std::string& cause(){return cause_;}
}fence;
namespace live_host_state{Fence& economic_fence(){return fence;}}
bool owner_ok=true,code_ok=true;unsigned owner_reads=0,originals=0,ads=0;uintptr_t fixture_quest=1000;QuestOwner canonical;
std::string native_origin="aaaaaaaa-aaaa-4aaa-8aaa-aaaaaaaaaaaa";
namespace action_runtime{
using StateOwner=QuestOwner;
bool state_owners(const a::Context& context,const a::Binding& binding,std::vector<QuestOwner>& rows,std::string& e){
 ++owner_reads;if(!owner_ok||context!=session.value||binding!=fixture_runtime.controller.b||native_origin==context.snapshot_uuid)return fail(e,"native current origin/Context/Binding differs");rows={canonical};return true;
}}
namespace npc_probe{namespace action_runtime=::action_runtime;}
bool code(Memory&,bool,std::string& e){return code_ok||fail(e,"exact rating code changed");}
uint32_t original_call(uintptr_t p,uintptr_t path){need(p==fixture_quest&&path==123,"original callback ABI differs");++originals;return 0;}
uint32_t advertise_call(uintptr_t p){need(p==fixture_quest,"original advertisement ABI differs");++ads;return 0;}
namespace native_calls{
#include "quest_rating_calls.inc"
}
bool compare_fixed_i32(uintptr_t p,int32_t old,int32_t next,std::string& e){int32_t value=0;if(!memory.copy(p,&value,4,e)||value!=old)return fail(e,"exact native CAS prior differs");memory.put(p,next);return true;}
uint32_t quest_runtime_byte(uintptr_t p,uint8_t old,uint8_t next,bool& exchanged){std::string e;uint8_t value=0;exchanged=memory.copy(p,&value,1,e)&&value==old;if(exchanged)memory.put(p,next);return 0;}
uint32_t quest_runtime_ads(uintptr_t image,uintptr_t p){need(image==state.image&&p==fixture_quest,"scalar native advertisement ABI differs");++ads;return 0;}
}
namespace mt2mp{std::string sha256(const std::vector<unsigned char>&){return "b2b4ef433836335fa6de391315d0398860a1bd3c20ff1e70a62ea55feade575c";}}
namespace {
#include "quest_rating_scalar.inc"
void reset(){
 session.value={4,1,2,3,a::Role::Client,"bbbbbbbb-bbbb-4bbb-8bbb-bbbbbbbbbbbb"};fixture_runtime.controller.c=session.value;fixture_runtime.controller.b={99,101};fixture_runtime.controller.s.started=true;fixture_runtime.controller.s.pending=false;
 fence.context_=session.value;fence.binding_=fixture_runtime.controller.b;fence.armed_=true;fence.suspended_=false;fence.locked_=false;
 canonical={"action:11111111-1111-4111-8111-111111111111",2000,{{0,fixture_quest}}};owner_ok=code_ok=true;thread=7;memory.bytes.clear();
 memory.put(fixture_quest,state.image+0x174ac10);memory.put(fixture_quest+0x1e0,state.image+0x1787870);
 for(size_t i=0;i<3;++i)memory.put(fixture_quest+0x1f8+i*4,uint32_t(0));memory.put(fixture_quest+0x204,uint8_t(0));memory.put(fixture_quest+0x88,int32_t(-1));memory.put(fixture_quest+0x8c,int32_t(-1));
 memory.put(fixture_quest+0x218,uintptr_t(3000));memory.put(3000,state.image+0x1714ac0);memory.put(3000+0xe,uint8_t(1));
 for(size_t i=0;i<192;++i)memory.put(state.image+0x4d6c00+i,uint8_t(0));
}
}
namespace mt2mp::action_session {Session& selected_session(){return ::session;}}
int main(){try{reset();std::string e;a::Binding binding;
 need(typed_action_runtime::network_binding(session.value.snapshot_uuid,binding,e),"actual idle baseline failed");
 fixture_runtime.controller.s.pending=true;fixture_runtime.controller.s.action_uuid="pending";
 need(!typed_action_runtime::network_binding(session.value.snapshot_uuid,binding,e)&&e=="network admission requires idle unlocked Controller","real idle guard did not reject pending action");
 Pin pin{session.value,fixture_runtime.controller.b};q::Value old;need(q::read(memory,state.image,fixture_quest,old,e),e);auto target=old;target.rating_bits[1]=std::bit_cast<uint32_t>(0.75f);target.rating_set=true;
 need(store_quest_runtime(pin,canonical,fixture_quest,0,old,target,[](std::string&){return true;},e),"actual scalar callback re-entered idle admission: "+e);
 need(fixture_runtime.engine.reads==1&&owner_reads>2&&ads==1,"saved binding path skipped native owner proof or called public idle admission");
 for(uintptr_t caller:{state.image+0x4d8f13,state.image+0x4d92a4}){native_calls::Calls calls;const auto result=q::complete(calls,fixture_quest,123,caller,state.image);need(result.confirmed&&result.client&&result.suppressed&&!originals,"actual completion Calls failed saved pending Context/Binding: "+result.cause);}
 for(unsigned mode=0;mode<6;++mode){reset();fixture_runtime.controller.s.pending=true;if(mode==0)owner_ok=false;if(mode==1)thread=8;if(mode==2)code_ok=false;if(mode==3)++fence.binding_.native_generation;if(mode==4)canonical.quests[0].second=4000;if(mode==5)fence.context_.snapshot_uuid=native_origin;
  native_calls::Calls calls;auto result=q::complete(calls,fixture_quest,123,state.image+0x4d92a4,state.image);need(!result.confirmed&&!result.cause.empty(),"actual current generation/owner/classified binding failure hidden");}
 reset();fence.suspended_=true;native_calls::Calls suspended;auto result=q::complete(suspended,fixture_quest,123,0,state.image);need(result.confirmed&&!result.owned&&originals==1,"snapshot suspension did not preserve original lifecycle");
 std::cout<<checks<<" actual pending binding/scalar/completion callback cases passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
