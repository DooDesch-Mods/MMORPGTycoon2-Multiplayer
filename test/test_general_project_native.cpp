#include "general_project_state.h"
#include "typed_action/action_session.h"
#include <atomic>
#include <memory>
#include <iostream>
#include <stdexcept>
namespace p=mt2mp::game::general_project;namespace a=mt2mp::action_session;
using DWORD=uint32_t;DWORD thread=17;DWORD GetCurrentThreadId(){return thread;}
#define LOG_ERROR(...) ((void)0)
namespace {
int checks=0;void need(bool v,const std::string& e){++checks;if(!v)throw std::runtime_error(e);}
const std::string id="20000000-0000-0000-0000-000000000002";a::Context context{77,2,1,3,a::Role::Client,"10000000-0000-0000-0000-000000000001"};a::Binding binding{31,39};
struct Session:a::Session {bool current(a::Context& out,std::string&)override{out=context;return true;}} session;
}
namespace mt2mp::action_session {Session& selected_session(){return ::session;}}
namespace typed_action_runtime {nlohmann::json controller={{"pending",true},{"action_uuid",id}};nlohmann::json controller_status(){return controller;}}
namespace building_mixed_identity::opened_file {struct CurrentWorld{uint64_t world_epoch=31,generation=39;};}
namespace native_open_runtime {building_mixed_identity::opened_file::CurrentWorld world;bool read_current_world(building_mixed_identity::opened_file::CurrentWorld& out,std::string&){out=world;return true;}}
namespace test {
struct Replay {std::string id;a::Context context;a::Binding binding;p::Intent intent;uintptr_t source=0,lounge=0,software=0;uint32_t entered=0;std::string cause;};
struct Runtime {DWORD thread=17;uintptr_t image=0x100000;bool ready=true;std::optional<Replay> replay;};Runtime runtime;std::atomic<Runtime*> published{&runtime};
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
p::Progress native;uintptr_t owner=0x200000,clone=0x300000;int writes=0,fail_at=0,reads=0,change_owner_at=0;
bool progress_once(Runtime&,const a::Context&,const a::Binding&,p::Progress& out,uintptr_t& lounge,uintptr_t& current,std::string& e,bool validate=true){++reads;out=native;lounge=owner+(change_owner_at==reads?8:0);current=clone;return !validate||p::valid(out)||fail(e,"fixture complete progress invalid");}
namespace live {bool compare_fixed_i32(uintptr_t address,int32_t before,int32_t after,std::string& e){++writes;if(writes==fail_at)return fail(e,"fixture actual CAS interrupted");
    int32_t actual=0;switch(address-owner){case 0xb0:actual=std::bit_cast<int32_t>(native.work);break;case 0xb4:actual=native.lines;break;case 0xb8:actual=native.bugs_total;break;case 0xbc:actual=native.bugs_known;break;case 0xc4:actual=native.workers;break;default:return fail(e,"fixture unexpected offset");}
    if(actual!=before)return fail(e,"fixture actual CAS before differs");switch(address-owner){case 0xb0:native.work=std::bit_cast<uint32_t>(after);break;case 0xb4:native.lines=after;break;case 0xb8:native.bugs_total=after;break;case 0xbc:native.bugs_known=after;break;case 0xc4:native.workers=after;break;}return true;}}
#include "general_project_actual.inc"
void reset(p::Command command){runtime.replay=Replay{id,context,binding,{command,{2,1,"",""},0},10,20,30};typed_action_runtime::controller={{"pending",true},{"action_uuid",id}};thread=17;native_open_runtime::world={31,39};}
void routes(){std::string e;bool handled=false,owned=false;reset(p::Command::Start);
    need(!original_route(0x2de2d0,reinterpret_cast<void*>(30),1,handled)&&handled,"nested purchase admitted before Pay");need(!runtime.replay->cause.empty(),"private first cause absent");reset(p::Command::Start);
    need(original_route(0x262ec0,reinterpret_cast<void*>(10),0,handled)&&handled,"original source Pay not admitted");need(original_route(0x2de2d0,reinterpret_cast<void*>(30),1,handled)&&handled,"nested original statistic purchase not admitted");
    need(!original_route(0x262ec0,reinterpret_cast<void*>(10),0,handled)&&handled,"original replay entered twice");
    reset(p::Command::Workers);runtime.replay->intent.workers=4;need(!original_route(0x25ec40,reinterpret_cast<void*>(20),3,handled)&&handled,"wrong worker value admitted");reset(p::Command::Commit);
    typed_action_runtime::controller["action_uuid"]="foreign";need(!original_route(0x25fb10,reinterpret_cast<void*>(20),0,handled)&&handled,"foreign pending action fell through");reset(p::Command::Commit);native_open_runtime::world.generation=40;
    need(!original_route(0x25fb10,reinterpret_cast<void*>(20),0,handled)&&handled,"wrong NativeOrigin admitted");reset(p::Command::Commit);
    need(!original_route(0x262ec0,reinterpret_cast<void*>(10),0,handled)&&!handled,"unrelated existing owner route stolen");
    need(!admit_bug_birth(context,binding,id,runtime.image+0x354e8d,owned,e)&&owned,"Bug admitted before original Commit entry");
    need(original_route(0x25fb10,reinterpret_cast<void*>(20),0,handled),"Commit not admitted");need(admit_bug_birth(context,binding,id,runtime.image+0x354e8d,owned,e)&&owned,e);
    need(!admit_bug_birth(context,binding,id,runtime.image+0x354e8c,owned,e)&&!owned,"foreign Bug caller admitted");auto wrong=binding;++wrong.native_generation;need(!admit_bug_birth(context,wrong,id,runtime.image+0x354e8d,owned,e)&&owned,"wrong saved Bug binding admitted");
    reset(p::Command::Start);need(!admit_bug_birth(context,binding,id,runtime.image+0x354e8d,owned,e)&&!owned,"ordinary Start opened Bug authority");runtime.replay.reset();need(!original_route(0x25fb10,reinterpret_cast<void*>(20),0,handled)&&!handled,"idle original route stolen");
}
void scalar(){std::string e;native={p::Source{0,10,"Tech",""},std::bit_cast<uint32_t>(1.f),2,4,3,1};auto target=native;target.work=std::bit_cast<uint32_t>(2.f);target.lines=5;target.bugs_total=1;target.bugs_known=1;target.workers=-1;bool attempted=false;uint32_t entered=0,stored=0;
    need(apply_progress(context,binding,target,attempted,entered,stored,e)&&attempted&&entered==5&&stored==5&&native==target,e);writes=reads=0;
    need(apply_progress(context,binding,target,attempted,entered,stored,e)&&!attempted&&!writes,"equal progress mutated");
    auto wrong=target;wrong.current->tech="Other";need(!apply_progress(context,binding,wrong,attempted,entered,stored,e)&&!attempted,"scalar replaced owning clone membership");
    target.lines=6;change_owner_at=reads+2;need(!apply_progress(context,binding,target,attempted,entered,stored,e)&&!attempted,"owner changed before CAS ignored");change_owner_at=0;
    fail_at=writes+1;need(!apply_progress(context,binding,target,attempted,entered,stored,e)&&attempted&&entered==1&&stored==0&&e=="fixture actual CAS interrupted","partial CAS lost first cause/attempt count");fail_at=0;
}
}
int main(){try{test::routes();test::scalar();std::cout<<checks<<" actual private original/Bug/pending/progress checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
