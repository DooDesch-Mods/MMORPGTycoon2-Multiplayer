#include "typed_action/action_session.h"
#include "live_host_state/monster_apply.h"
#include "live_host_state/state.h"
#include <atomic>
#include <bit>
#include <iostream>
#include <map>
#include <stdexcept>
namespace a=mt2mp::action_session;
namespace action=a;
namespace live_host_state=mt2mp::game::live_host_state;
namespace ml=live_host_state::monsters;
using json=nlohmann::json;
namespace {
size_t checks=0;void need(bool v,const std::string& why){++checks;if(!v)throw std::runtime_error(why);}
uint32_t GetCurrentThreadId(){return 7;}
struct Controller{a::Status s;a::Context c;a::Binding b;a::Status status(){return s;}a::Context bound_context(){return c;}a::Binding bound_binding(){return b;}};
struct Session:a::Session{a::Context c;bool current(a::Context& out,std::string&)override{out=c;return true;}}session;
struct Engine{int reads=0;bool current(const a::Binding&,const std::string&,std::string&){++reads;return true;}};
struct Runtime{uint32_t thread=7;Controller controller;Engine engine;Session& session=::session;bool lobby_retiring=false;}fixture_runtime;
std::atomic<Runtime*> published_runtime{&fixture_runtime};
namespace typed_action_runtime {
#include "monster_live_idle_binding.inc"
}
bool fail(std::string& e,const std::string& why){e=why;return false;}
struct Memory{int process=1;};
struct {uint32_t thread=7;uintptr_t image=0x100000;}fixture_state;
const char* exe_sha="exact-test-binary";
bool owner_available=true;uint64_t native_epoch=8,native_generation=9;int32_t map_state=7;float actual_scale=0;
namespace host{
struct WorldState{int32_t saves=0,edits=0;uintptr_t active_graph=0;struct{uint64_t world_epoch=8,generation=9,map=100;}origin;};struct LookupApi{};struct NativeOrigin{};
bool resolve_lookup(int,uintptr_t,const char*,LookupApi&,std::string&){return true;}
struct Reader{Reader(int,NativeOrigin&,uintptr_t,uint32_t,LookupApi&){}bool current(WorldState& out,bool,std::string& e){if(!owner_available)return fail(e,"original native owner absent");out.origin.world_epoch=native_epoch;out.origin.generation=native_generation;return true;}};
}
namespace net{std::string current_snapshot_id(){return session.c.snapshot_uuid;}}
namespace action_runtime{
#include "monster_live_world_binding.inc"
}
namespace native{struct ClockState{float time_scale=0;};bool read_clock(ClockState& out,std::string&){out.time_scale=actual_scale;return true;}}
bool field(Memory&,uintptr_t,size_t,int32_t& out,std::string&){out=map_state;return true;}
bool code_okay=true;bool live_code(Memory&,bool,std::string& e){return code_okay||fail(e,"original code seal differs");}
struct LiveWorld{host::WorldState world;host::LookupApi api;};
struct Local{std::array<int32_t,ml::animation_slots>animation_index{};std::array<float,ml::animation_slots>animation_length{};};
class MonsterLivePort{
public:
    a::Context context_;a::Binding binding_{8,9};uint64_t epoch_=7,revision_=0;bool bound_=true,suspended_=false,locked_=false;std::atomic<bool>wrong_thread_{false};std::string cause_;json last_;std::map<std::string,Local> locals_;
    bool reject(std::string& e,const std::string& why,bool lock=false){locked_=lock;return fail(e,why);}
    bool world(Memory&,LiveWorld&,std::string&,bool idle=true);
    std::optional<ml::State> traced_;
    void trace(const ml::State&,const ml::State&,const ml::State&,uint64_t){}
    ml::State sampled;
    bool read(ml::State& out,std::string& e){Memory memory;LiveWorld current;if(!world(memory,current,e))return false;out=sampled;return true;}
    bool set(const ml::Row&,const ml::Row&,bool&,std::string&){throw std::runtime_error("empty complete set must not mutate");}
    bool capture(const a::Context&,const a::Binding&,const live_host_state::Frame&,ml::State&,std::string&);
    bool preflight(const a::Context&,const a::Binding&,const live_host_state::Frame&,std::string&);
    bool apply(const a::Context&,const a::Binding&,const live_host_state::Frame&,ml::Receipt&,std::string&);
    bool after_action(const a::Context&,const a::Binding&,std::string&);
};
}
namespace mt2mp::action_session {Session& selected_session(){return ::session;}}
namespace {
#define state fixture_state
#include "monster_live_pending_bodies.inc"
#undef state
}
int main(){try{
    fixture_runtime.controller.c={4,1,1,3,a::Role::Host,"common"};fixture_runtime.controller.b={8,9};fixture_runtime.controller.s.started=true;session.c=fixture_runtime.controller.c;
    std::string e;a::Binding admitted;
    need(typed_action_runtime::network_binding("common",admitted,e)&&admitted==fixture_runtime.controller.b,"real idle baseline rejected");
    fixture_runtime.controller.s.pending=true;fixture_runtime.controller.s.action_uuid="pending-action";
    need(!typed_action_runtime::network_binding("common",admitted,e)&&e=="network admission requires idle unlocked Controller","real pending idle rejection absent");
    MonsterLivePort port;port.context_=session.c;live_host_state::Frame frame;frame.epoch=7;frame.revision=5;frame.monsters=ml::State{};ml::State captured;
    need(port.capture(session.c,port.binding_,frame,captured,e),"actual Host capture -> saved world binding failed during pending: "+e);
    session.c.role=a::Role::Client;port.context_=session.c;
    need(port.preflight(session.c,port.binding_,frame,e),"actual Client preflight still used idle admission: "+e);
    ml::Receipt receipt;need(port.apply(session.c,port.binding_,frame,receipt,e)&&receipt.confirmed&&!receipt.attempted&&port.revision_==5,"actual Client apply -> motion -> saved binding failed: "+e);
    need(port.after_action(session.c,port.binding_,e),"actual after-action callback uses idle admission");
    need(fixture_runtime.engine.reads==1,"pending callback accidentally invoked public idle engine admission");
    for(int failure=0;failure<6;++failure){owner_available=true;map_state=7;native_generation=9;actual_scale=0;code_okay=true;session.c=port.context_;
        if(failure==0)owner_available=false;if(failure==1)native_generation=10;if(failure==2)session.c.snapshot_uuid="foreign";if(failure==3)map_state=6;if(failure==4)actual_scale=1;if(failure==5)code_okay=false;
        need(!port.preflight(port.context_,port.binding_,frame,e)&&!e.empty(),"actual pending owner/code/Clock failure hidden "+std::to_string(failure));}
    owner_available=true;map_state=7;native_generation=9;actual_scale=0;code_okay=true;session.c=port.context_;
    ml::Row row;row.birth="11111111-1111-4111-8111-111111111111";row.name="Airg";row.rank=1;row.slot=1;row.type=0;row.level=1;row.pose.region=67;row.pose.orientation={0,0,0,1};row.pose.scale={1,1,1};row.on_map=true;
    row.animation_length[0]=std::bit_cast<uint32_t>(1.0f);port.locals_[row.birth].animation_index={4,-1,-1};port.locals_[row.birth].animation_length={1,0,0};
    for(float raw:{-0.25f,1.25f,1.75f}){
        row.animation[0]=std::bit_cast<uint32_t>(raw);port.sampled={{row},{}};frame.monsters=port.sampled;e.clear();
        need(port.preflight(session.c,port.binding_,frame,e),"actual nonempty preflight rejected native signed/overlength phase: "+e);
        port.locals_[row.birth].animation_index[0]=-1;e.clear();need(!port.preflight(session.c,port.binding_,frame,e)&&e.find("matching local clip")!=std::string::npos,"missing native semantic clip accepted");port.locals_[row.birth].animation_index[0]=4;
        frame.monsters->rows[0].animation_length[0]=std::bit_cast<uint32_t>(2.0f);e.clear();need(!port.preflight(session.c,port.binding_,frame,e)&&e.find("clip lengths differ")!=std::string::npos,"actual preflight length equality guard lost");
    }
    row.animation[0]=std::bit_cast<uint32_t>(0.5f);row.animation_length[1]=row.animation_length[0];row.animation[1]=row.animation[0];
    row.animation_speed[0]=row.animation_speed[1]=std::bit_cast<uint32_t>(1.0f);
    port.sampled={{row},{}};frame.monsters=port.sampled;port.locals_[row.birth].animation_index.fill(-1);port.locals_[row.birth].animation_index[0]=port.locals_[row.birth].animation_index[1]=4;
    need(port.preflight(session.c,port.binding_,frame,e),"consistent semantic alias rejected: "+e);
    for(int conflict=0;conflict<3;++conflict){frame.monsters=port.sampled;auto& value=frame.monsters->rows[0];
        if(conflict==0)value.animation[1]=std::bit_cast<uint32_t>(0.75f);if(conflict==1)value.animation_paused[1]=true;if(conflict==2)value.animation_speed[1]=std::bit_cast<uint32_t>(2.0f);
        need(!port.preflight(session.c,port.binding_,frame,e)&&e.find("semantic aliases disagree")!=std::string::npos,"actual preflight admitted alias phase conflict");}
    std::cout<<checks<<" actual pending Frame callback and idle admission checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
