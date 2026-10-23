#include "dungeon_gizmo_live_read.h"
#include "dungeon_scene_identity_runtime.h"
#include "native_open_witness.h"
namespace gizmo_live_adopt_fixture {
namespace a {
using Context=mt2mp::action_session::Context;using Binding=mt2mp::action_session::Binding;using Role=mt2mp::action_session::Role;
Context observed;Binding admitted;bool available=true;
struct Session {bool current(Context& out,std::string& error){if(!available){error="fixture Session unavailable";return false;}out=observed;return true;}};
Session& selected_session(){static Session session;return session;}
}
namespace building_mixed_identity=mt2mp::game::building_mixed_identity;
namespace dungeon_gizmo_identity=mt2mp::game::dungeon_gizmo_identity;
namespace gl=mt2mp::game::dungeon_gizmo_live;
namespace dungeon_gizmo_live=mt2mp::game::dungeon_gizmo_live;
alignas(16) std::array<unsigned char,0x400> fixture_object{};
alignas(16) std::array<unsigned char,0x100> fixture_actor{},fixture_shard{},fixture_definition{};
constexpr uintptr_t fixture_image=0x10000000;
std::array<gl::Clip,4> fixture_clips;
bool fixture_owner=true,fixture_clip_failure=false;unsigned fixture_native_clips=0;
uintptr_t object_address(){return uintptr_t(fixture_object.data());}
uintptr_t shard_address(){return uintptr_t(fixture_shard.data());}
template<class T>void put(uintptr_t address,const T& value){std::memcpy(reinterpret_cast<void*>(address),&value,sizeof value);}
struct Memory {
    bool copy(uintptr_t address,void* out,size_t size,std::string& error){
        for(const auto span:{std::pair{uintptr_t(fixture_object.data()),fixture_object.size()},std::pair{uintptr_t(fixture_actor.data()),fixture_actor.size()},
            std::pair{uintptr_t(fixture_shard.data()),fixture_shard.size()},std::pair{uintptr_t(fixture_definition.data()),fixture_definition.size()}})
            if(address>=span.first&&address-span.first<=span.second&&size<=span.second-(address-span.first)){std::memcpy(out,reinterpret_cast<const void*>(address),size);return true;}
        error="Gizmo adoption fixture original byte span unavailable";return false;
    }
};
template<class T>bool field(Memory& memory,uintptr_t object,size_t offset,T& value,std::string& error){return memory.copy(object+offset,&value,sizeof value,error);}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
const std::string fixture_scene="action:11111111-1111-4111-8111-111111111111/template";
const std::string fixture_key="action:22222222-2222-4222-8222-222222222222:dungeon:gizmo:0";
struct {uintptr_t image=fixture_image;DWORD thread=GetCurrentThreadId();}state;
using Physical=building_mixed_identity::opened_file::CurrentWorld;
using Census=mt2mp::game::dungeon_scene_identity_runtime::Census;
Physical fixture_origin;Census fixture_census;
std::optional<Physical> fixture_origin_change;std::optional<mt2mp::game::dungeon_scene_identity_runtime::World> fixture_census_change;
size_t fixture_reads=0,fixture_censuses=0,fixture_code_reads=0,change_at=0,census_change_at=0;bool fixture_read_failure=false,fixture_network=true,fixture_code=true,fixture_session_change=false;
namespace typed_action_runtime {
namespace action=a;
struct ControllerStatus {bool started=true,locked=false,pending=false;}fixture_status;
bool fixture_foreign_controller=false;
struct Controller {
    ControllerStatus status()const{return fixture_status;}
    a::Context bound_context()const{auto value=a::observed;if(fixture_foreign_controller)++value.session;return value;}
    a::Binding bound_binding()const{return a::admitted;}
};
struct Engine {
    bool current(const a::Binding& binding,const std::string& snapshot,std::string& error){
        return fixture_network&&snapshot==a::observed.snapshot_uuid&&binding==a::admitted||
            fail(error,"fixture current shared network snapshot differs");
    }
};
struct Runtime {DWORD thread=GetCurrentThreadId();Controller controller;bool lobby_retiring=false;a::Session session;Engine engine;}fixture_runtime;
std::atomic<Runtime*> published_runtime{&fixture_runtime};
#include "controller_binding_actual.inc"
}
namespace native_open_runtime {
bool read_current_world(Physical& out,std::string& error){
    if(fixture_read_failure)return fail(error,"fixture original NativeOpen read failure");
    out=fixture_origin_change&&fixture_reads>=change_at?*fixture_origin_change:fixture_origin;++fixture_reads;
    if(fixture_session_change&&fixture_reads==2)++a::observed.session;
    return true;
}}
namespace dungeon_scene_identity_runtime {
using Census=mt2mp::game::dungeon_scene_identity_runtime::Census;
bool read_current(Census& out,std::string& error){
    if(!fixture_owner)return fail(error,"Gizmo actual owner changed");
    out=fixture_census;
    if(fixture_census_change&&fixture_censuses>=census_change_at)out.world=*fixture_census_change;
    ++fixture_censuses;return true;
}}
namespace live_host_state::apply_timing {
enum class PageBoundary {OriginalCall};
struct PageReadScope {explicit PageReadScope(PageBoundary){}};
}
bool code(Memory&,std::string& error){++fixture_code_reads;return fixture_code||fail(error,"fixture original code pin differs");}
uint32_t live_clip(uintptr_t actor,int32_t index,gl::Clip& value)noexcept{
    if(actor!=uintptr_t(fixture_actor.data())||index<0||index>3)return 0xe0010003;
    value=fixture_clips[size_t(index)];return 0;
}
#include "gizmo_live_read_actual.inc"
#include "gizmo_live_capture_actual.inc"
void reset_origin(){
    typed_action_runtime::fixture_status={};typed_action_runtime::fixture_foreign_controller=false;
    typed_action_runtime::fixture_runtime.thread=GetCurrentThreadId();typed_action_runtime::fixture_runtime.lobby_retiring=false;
    typed_action_runtime::published_runtime.store(&typed_action_runtime::fixture_runtime);
    a::observed={};a::observed.role=a::Role::Client;a::observed.snapshot_uuid="11111111-1111-4111-8111-111111111111";
    a::admitted={7,9};a::available=true;state.thread=GetCurrentThreadId();
    fixture_origin={};fixture_origin.uuid=a::observed.snapshot_uuid;fixture_origin.sha256=std::string(64,'a');
    fixture_origin.world_epoch=7;fixture_origin.generation=9;
    fixture_origin.game=reinterpret_cast<void*>(0x80000);fixture_origin.mode=reinterpret_cast<void*>(0x81000);
    fixture_origin.state=reinterpret_cast<void*>(0x82000);fixture_origin.world=reinterpret_cast<void*>(0x83000);fixture_origin.map=reinterpret_cast<void*>(0x84000);
    fixture_origin_change.reset();fixture_census_change.reset();fixture_reads=fixture_censuses=fixture_code_reads=change_at=census_change_at=0;
    fixture_read_failure=false;fixture_network=fixture_code=true;fixture_session_change=false;
    fixture_census={};fixture_census.world={fixture_origin.game,fixture_origin.mode,fixture_origin.state,fixture_origin.world,fixture_origin.map,
        fixture_origin.generation,fixture_origin.world_epoch,fixture_origin.uuid,fixture_origin.sha256};
    fixture_census.keys={fixture_key};
    mt2mp::game::dungeon_scene_identity_runtime::Pool pool;
    pool.scene.owner.key=fixture_scene;pool.scene.owner.shard=shard_address();pool.scene.owner.region=0x50000;pool.managers[3]=0x40000;
    pool.local.members.push_back({object_address(),0,0,0x100000077,0});fixture_census.pools.push_back(pool);
}
uint32_t live_clip(uintptr_t actor,int32_t index,const gl::Clip& target)noexcept{
    ++fixture_native_clips;if(fixture_clip_failure)return 0xe0010003;if(actor!=uintptr_t(fixture_actor.data())||index<0||index>3)return 0xe0010003;fixture_clips[size_t(index)]=target;return 0;
}
uint32_t live_on_map(uintptr_t object,bool value)noexcept{if(object!=object_address())return 0xe0010003;put(object+0x300,uintptr_t(value?0x60000:0));return 0;}
#include "gizmo_live_cas_actual.inc"
#include "gizmo_live_apply_actual.inc"
#include "gizmo_live_apply_baseline.inc"
void reset(){
    reset_origin();
    fixture_object.fill(0);fixture_actor.fill(0);fixture_shard.fill(0);fixture_definition.fill(0);fixture_owner=true;fixture_clip_failure=false;fixture_native_clips=0;
    const auto object=object_address();put(object,fixture_image+0x1749ab0);put(object+0x18,uint64_t(0x100000077));put(object+0x170,shard_address());put(object+0x178,uintptr_t(0x50000));
    put(object+0x278,uintptr_t(fixture_definition.data()));put(uintptr_t(fixture_definition.data())+0xa0,uint32_t(2));
    put(object+0x2e0,uintptr_t(fixture_actor.data()));put(uintptr_t(fixture_actor.data())+0x20,int32_t(4));
    for(size_t n=0;n<3;++n)put(object+0x2e8+n*4,int32_t(n));
    // Dormant cache contents deliberately disagree with every portable state.
    put(object+0x2c4,int32_t(0x55555555));put(object+0x2c8,int32_t(0x33333333));put(object+0x2cc,int32_t(0x77777777));
    for(auto& clip:fixture_clips)clip={uint32_t(0x40000000),uint32_t(0),false,uint32_t(0x3f800000)};
}
void origin_cases(){
    for(const bool host:{true,false}){
        reset();a::observed.role=host?a::Role::Host:a::Role::Client;
        if(host){fixture_origin.uuid="33333333-3333-4333-8333-333333333333";fixture_origin.sha256=std::string(64,'b');
            fixture_census.world.uuid=fixture_origin.uuid;fixture_census.world.sha256=fixture_origin.sha256;}
        const auto context=a::observed;const auto binding=a::admitted;std::string error;gl::State canonical;
        need(capture_live(context,binding,canonical,error)&&canonical.size()==1&&canonical[0].rows.size()==1,
             "actual Gizmo live capture must accept shared Host/local origin and loaded Client");
        const auto exact=fixture_origin;const auto census=fixture_census;
        const auto reject=[&]{
            gl::State out=canonical;fixture_reads=fixture_censuses=0;
            need(!capture_live(context,binding,out,error)&&out.empty()&&!error.empty(),"actual Gizmo capture admitted changed origin or context");
            if(!host){gl::Receipt receipt;auto target=canonical;target[0].unlocked=7;fixture_reads=fixture_censuses=0;
                need(!apply_live(context,binding,target,receipt,error)&&!receipt.attempted&&!receipt.confirmed,
                     "actual Gizmo apply admitted changed origin before write");}
        };
        fixture_network=false;reject();fixture_network=true;
        ++a::admitted.native_epoch;reject();a::admitted=binding;
        ++a::admitted.native_generation;reject();a::admitted=binding;
        a::observed.snapshot_uuid="44444444-4444-4444-8444-444444444444";reject();a::observed=context;
        fixture_read_failure=true;reject();need(error=="fixture original NativeOpen read failure","original read cause replaced");fixture_read_failure=false;
        fixture_session_change=true;reject();fixture_session_change=false;a::observed=context;
        for(int phase=0;phase<3;++phase)for(int field=0;field<9;++field){
            auto changed=exact;
            if(field==0)changed.uuid="55555555-5555-4555-8555-555555555555";
            if(field==1)changed.sha256=std::string(64,'c');
            if(field==2)++changed.generation;if(field==3)++changed.world_epoch;
            if(field==4)changed.game=reinterpret_cast<void*>(0x90000);
            if(field==5)changed.mode=reinterpret_cast<void*>(0x91000);
            if(field==6)changed.state=reinterpret_cast<void*>(0x92000);
            if(field==7)changed.world=reinterpret_cast<void*>(0x93000);
            if(field==8)changed.map=reinterpret_cast<void*>(0x94000);
            auto changed_census=census.world;
            changed_census={changed.game,changed.mode,changed.state,changed.world,changed.map,changed.generation,changed.world_epoch,changed.uuid,changed.sha256};
            if(phase==0){fixture_census_change=changed_census;census_change_at=0;}
            if(phase==1){fixture_origin_change=changed;change_at=1;}
            // Both individual reads are coherent, but their physical owners differ.
            if(phase==2){fixture_origin_change=changed;fixture_census_change=changed_census;change_at=2;census_change_at=1;}
            reject();
            fixture_origin=exact;fixture_census=census;fixture_origin_change.reset();fixture_census_change.reset();
        }
        fixture_reads=fixture_censuses=0;gl::State after;
        need(capture_live(context,binding,after,error)&&after==canonical,"rejected origin changed actual Gizmo state");
    }
}
void pending_cases(){
    namespace binding_api=typed_action_runtime;
    for(const bool host:{true,false}){
        reset();a::observed.role=host?a::Role::Host:a::Role::Client;
        if(host){fixture_origin.uuid="33333333-3333-4333-8333-333333333333";fixture_origin.sha256=std::string(64,'b');
            fixture_census.world.uuid=fixture_origin.uuid;fixture_census.world.sha256=fixture_origin.sha256;}
        const auto context=a::observed;const auto binding=a::admitted;std::string error;a::Binding actual;
        binding_api::fixture_status.pending=true;
        need(binding_api::read_binding(context.snapshot_uuid,actual,error)&&actual==binding,
             "actual current binding must permit existing pending action");
        need(!binding_api::network_binding(context.snapshot_uuid,actual,error)&&actual==a::Binding{}&&
             error=="network admission requires idle unlocked Controller",
             "actual new-action admission must still reject pending Controller");
        gl::State captured;
        need(capture_live(context,binding,captured,error)&&captured.size()==1,
             "actual Host before/Client live capture must permit queued action");
        if(!host){
            auto target=captured;target[0].unlocked=7;gl::Receipt receipt;
            need(apply_live(context,binding,target,receipt,error)&&receipt.attempted&&receipt.confirmed&&receipt.stores==1,
                 "actual Client pending-action adoption must confirm owned scalar write");
            need(capture_live(context,binding,captured,error)&&captured==target,"pending-action complete readback differs");
        }
        for(int mode=0;mode<7;++mode){
            if(mode==0)binding_api::fixture_status.locked=true;
            if(mode==1)binding_api::fixture_status.started=false;
            if(mode==2)binding_api::fixture_runtime.lobby_retiring=true;
            if(mode==3)binding_api::fixture_foreign_controller=true;
            if(mode==4)fixture_network=false;
            if(mode==5)binding_api::fixture_runtime.thread=GetCurrentThreadId()+1;
            if(mode==6)binding_api::published_runtime.store(nullptr);
            gl::State out=captured;need(!capture_live(context,binding,out,error)&&out.empty()&&!error.empty(),
                "pending read accepted unavailable or foreign actual Controller binding");
            if(!host){auto target=captured;target[0].unlocked=8;gl::Receipt receipt;
                need(!apply_live(context,binding,target,receipt,error)&&!receipt.attempted&&!receipt.confirmed,
                     "pending adoption wrote after actual binding rejection");}
            binding_api::fixture_status={true,false,true};binding_api::fixture_runtime.lobby_retiring=false;
            binding_api::fixture_foreign_controller=false;fixture_network=true;
            binding_api::fixture_runtime.thread=GetCurrentThreadId();binding_api::published_runtime.store(&binding_api::fixture_runtime);
        }
        need(!binding_api::read_binding("44444444-4444-4444-8444-444444444444",actual,error),
             "pending current binding accepted foreign snapshot");
        need(capture_live(context,binding,captured,error),"failed pending admission damaged retained world");
        binding_api::fixture_status.pending=false;
        need(binding_api::network_binding(context.snapshot_uuid,actual,error)&&actual==binding,
             "actual new-action admission did not recover after pending action ended");
    }
}
void empty_delta_cases(){
    struct Outcome {bool okay=false;gl::Receipt receipt;std::string error;size_t reads=0,censuses=0,codes=0;unsigned clips=0;
        decltype(fixture_object) object;decltype(fixture_shard) shard;decltype(fixture_clips) clip_values;};
    for(int scenario=0;scenario<15;++scenario){
        const auto invoke=[&](bool baseline){
            reset();auto context=a::observed;const auto binding=a::admitted;gl::State target;
            if(scenario<10){fixture_census.keys.clear();fixture_census.pools.clear();}
            else {Memory memory;LiveSample initial;std::string error;
                if(scenario==12){put(uintptr_t(fixture_definition.data())+0xa0,uint32_t(0));put(object_address()+0x2d0,uint8_t(1));put(object_address()+0x2c4,int32_t(-1));put(object_address()+0x2c8,int32_t(0));put(object_address()+0x2cc,int32_t(0));}
                need(live_sample(memory,context,binding,initial,error),"Gizmo differential initial current read");target=initial.value;
                fixture_reads=fixture_censuses=fixture_code_reads=0;
            }
            if(scenario==1)fixture_code=false;
            if(scenario==2)fixture_owner=false;
            if(scenario==3)fixture_read_failure=true;
            if(scenario==4)fixture_network=false;
            if(scenario==5)context.role=a::Role::Host;
            if(scenario==6){fixture_origin_change=fixture_origin;fixture_origin_change->map=reinterpret_cast<void*>(0x99000);change_at=1;}
            if(scenario==7){fixture_census_change=fixture_census.world;fixture_census_change->map=reinterpret_cast<void*>(0x99000);census_change_at=1;}
            if(scenario==8)fixture_session_change=true;
            if(scenario==9)target.push_back({fixture_scene,0,{}});
            if(scenario==11)target.clear();
            if(scenario==13)target[0].unlocked=7;
            if(scenario==14){target[0].rows[0].state=2;target[0].rows[0].clips[0].time=uint32_t(0x3f800000);fixture_clip_failure=true;}
            Outcome result;result.okay=baseline?apply_live_baseline(context,binding,target,result.receipt,result.error):apply_live(context,binding,target,result.receipt,result.error);
            result.reads=fixture_reads;result.censuses=fixture_censuses;result.codes=fixture_code_reads;result.clips=fixture_native_clips;
            result.object=fixture_object;result.shard=fixture_shard;result.clip_values=fixture_clips;return result;
        };
        const auto old=invoke(true),now=invoke(false);
        need(now.okay==old.okay&&now.error==old.error&&now.receipt.attempted==old.receipt.attempted&&now.receipt.confirmed==old.receipt.confirmed&&
            now.receipt.unknown==old.receipt.unknown&&now.receipt.attempted_stores==old.receipt.attempted_stores&&now.receipt.stores==old.receipt.stores&&now.receipt.cause==old.receipt.cause,
            "Gizmo empty delta preserves actual result, receipt and original cause");
        need(now.object==old.object&&now.shard==old.shard&&now.clip_values==old.clip_values&&now.clips==old.clips,"Gizmo empty delta preserves exact writes and native effects");
        if(scenario==0){need(now.okay&&now.receipt.confirmed&&!now.receipt.attempted&&now.codes==1&&old.codes==2&&now.censuses==2&&old.censuses==4&&now.reads==4&&old.reads==8,
            "Gizmo empty delta retains fresh code and paired census, removes only unused post-adoption pair");}
        else need(now.reads==old.reads&&now.censuses==old.censuses&&now.codes==old.codes,"Gizmo failure and nonempty paths retain every native sample");
        if(scenario==12)need(!now.okay&&now.error=="Gizmo live target transition has no current local Door asset","Gizmo nonempty equal values retain feature rejection");
    }
}
void run(){
    empty_delta_cases();
    pending_cases();
    origin_cases();
    reset();const auto context=a::observed;const auto binding=a::admitted;Memory memory;LiveSample initial;std::string error;
    need(live_sample(memory,context,binding,initial,error),"Gizmo fixture complete current read");auto target=initial.value;
    target[0].unlocked=0x80000001;auto& wanted=target[0].rows[0];wanted.triggered=true;wanted.state=2;wanted.immediate=1;wanted.transition=true;wanted.animation=1;wanted.from=1;wanted.to=2;wanted.on_map=true;
    wanted.clips[1].time=uint32_t(0x3f000000);wanted.clips[1].paused=true;wanted.clips[1].speed=uint32_t(0xbf800000);
    gl::Receipt receipt;need(apply_live(context,binding,target,receipt,error)&&receipt.confirmed&&receipt.stores>0&&fixture_native_clips==1,"actual Gizmo adoption installs original scalar cache and semantic clip");
    LiveSample after;need(live_sample(memory,context,binding,after,error)&&after.value==target,"actual Gizmo complete readback equals Host state");
    const auto calls=fixture_native_clips;need(apply_live(context,binding,target,receipt,error)&&!receipt.attempted&&fixture_native_clips==calls,"equal Gizmo Frame performs no native mutation");
    target[0].rows[0].transition=false;target[0].rows[0].animation=-1;target[0].rows[0].from=target[0].rows[0].to=0;
    need(apply_live(context,binding,target,receipt,error)&&receipt.confirmed,"settled animation releases ownership of private transition cache");
    fixture_owner=false;target[0].unlocked=7;need(!apply_live(context,binding,target,receipt,error)&&!receipt.attempted&&error=="Gizmo actual owner changed","changed current lifetime rejects before scalar write");
    reset();need(live_sample(memory,context,binding,initial,error),"Gizmo reset native read");target=initial.value;target[0].rows[0].state=2;target[0].rows[0].clips[0].time=uint32_t(0x3f800000);fixture_clip_failure=true;
    need(!apply_live(context,binding,target,receipt,error)&&receipt.unknown&&!receipt.confirmed&&error.find("original clip adoption")!=std::string::npos,"partial actual Gizmo adoption preserves failed native cause and Unknown");
    reset();need(live_sample(memory,context,binding,initial,error),"Gizmo second reset native read");target=initial.value;target[0].rows[0].clips[0].length=uint32_t(0x40400000);
    need(!apply_live(context,binding,target,receipt,error)&&!receipt.attempted,"foreign local asset length rejects before mutation");
}
}
