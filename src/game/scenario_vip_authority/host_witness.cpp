#include "host_witness.h"
#include "host_sample.h"
#include "alert_probe_reader.h"
#include "disconnect_read.h"
#include "../native_open_runtime.h"
#include "../session_provider.h"
#include "../live_host_state/memory_query_timing.h"
#include "mt2mp/game/native_engine.h"
#include "mt2mp/net/snapshot_sync.h"
#include <Windows.h>
#include <atomic>

namespace mt2mp::game::scenario_vip_authority {
namespace {
using World=building_mixed_identity::opened_file::CurrentWorld;
struct Producer {
    HostTransitions transitions;
    action_session::Context context;action_session::Binding binding;
    World world;uintptr_t image=0;uint32_t thread=0,image_size=0;
    bool busy=false,updating=false;std::string failure;
};
Producer producer;
std::atomic<uint32_t> observation_thread=0;
std::atomic<bool> wrong_thread=false;
placement_ui_probe::ProcessMemory host_memory;
bool same_world(const World& a,const World& b){return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&
    a.map==b.map&&a.uuid==b.uuid&&a.sha256==b.sha256&&a.bytes==b.bytes&&a.world_epoch==b.world_epoch&&a.generation==b.generation;}
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
void latch(Producer& p,const std::string& e){if(p.failure.empty())p.failure=e.empty()?"VIP Host observation failed without cause":e;}
bool healthy(Producer& p,std::string& e){
    if(wrong_thread.load(std::memory_order_acquire))latch(p,"VIP Host Update observed off bound Mainthread");
    return p.failure.empty()||fail(e,p.failure);
}
bool pin(const Producer& p,std::string& e){
    uintptr_t image=0;uint32_t thread=0;std::string sha;action_session::Context actual;World world;
    if(GetCurrentThreadId()!=p.thread)return fail(e,"VIP Host witness requires bound Mainthread");
    if(!native::read_resolver_receipt(image,thread,sha,e)||!action_session::selected_session().current(actual,e)||
       !native_open_runtime::read_current_world(world,e))return false;
    if(image!=p.image||thread!=p.thread||sha!="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6"||
       actual!=p.context||!same_world(world,p.world)||world.world_epoch!=p.binding.native_epoch||
       world.generation!=p.binding.native_generation||net::current_world_epoch()!=p.transitions.origin().epoch)
        return fail(e,"VIP Host current resolver/Session/World binding differs");
    return true;
}
bool collect(Producer& p,uintptr_t selected,std::string& e,const std::optional<std::string>& message=std::nullopt){
    if(!pin(p,e))return false;
    std::vector<uintptr_t> before,after;
    {
    live_host_state::apply_timing::PageReadScope pages;
    if(!host_read::scenarios(host_memory,p.image,uintptr_t(p.world.state),before,e))return false;
    for(auto instance:before){
        if(selected&&instance!=selected)continue;
        HostSample sample;if(!host_read::sample(host_memory,p.image,instance,sample,e))return false;
        if(p.transitions.needs_arrival(sample)){
            alert_probe::Reader reader(host_memory,p.image,p.image_size);alert_probe::Graph graph;
            if(!reader.read(uintptr_t(p.world.mode),uintptr_t(p.world.state),graph,e))return false;
            if(!graph.present)return fail(e,"VIP Host arrival UI owner unavailable: "+graph.absent);
            const auto found=std::find_if(graph.alerts.begin(),graph.alerts.end(),[&](const auto& row){return row.scenario==sample.scenario;});
            if(found==graph.alerts.end()||found->object!=sample.alert||found->toon!=sample.toon||
               found->subscriber_id!=sample.subscriber||found->name!=sample.name)
                return fail(e,"VIP Host arrival does not match complete shown alert graph");
        }
        HostSample final;if(!host_read::sample(host_memory,p.image,instance,final,e))return false;
        if(final!=sample)return fail(e,"VIP Host selected arrival state changed during pure read");
        if(!p.transitions.observe(p.transitions.origin(),sample,e,selected==instance?message:std::nullopt))return false;
    }
    if(!host_read::scenarios(host_memory,p.image,uintptr_t(p.world.state),after,e))return false;
    if(before!=after)return fail(e,"VIP Host ScenarioLog membership changed during pure read");
    // Never read a retained receiver after it leaves the current ScenarioLog.
    for(auto tracked:p.transitions.tracked()){
        if(selected&&tracked!=selected)continue;
        if(std::find(after.begin(),after.end(),tracked)==after.end()){
            HostSample retired;retired.scenario=tracked;
            if(!p.transitions.observe(p.transitions.origin(),retired,e))return false;
        }
    }
    p.transitions.forget_removed(after);
    }
    return pin(p,e);
}
struct EndBefore {bool observed=false;uint8_t completed=0;alert_probe::Graph graph;};
bool before_end(Producer& p,uintptr_t receiver,EndBefore& out,std::string& e){
    out={};if(!p.transitions.known(receiver)){e.clear();return true;}
    if(!pin(p,e))return false;
    {
        live_host_state::apply_timing::PageReadScope pages;
        std::vector<uintptr_t> members;
        if(!host_read::scenarios(host_memory,p.image,uintptr_t(p.world.state),members,e))return false;
        if(std::find(members.begin(),members.end(),receiver)==members.end())return fail(e,"VIP EndEvent receiver absent before original Update");
        if(!host_read::at(host_memory,receiver,0x38,out.completed,e))return false;
        if(out.completed>1)return fail(e,"VIP EndEvent prior completed flag is not boolean");
        if(out.completed){e.clear();return true;}
        alert_probe::Reader reader(host_memory,p.image,p.image_size);
        if(!reader.read_ui(uintptr_t(p.world.mode),uintptr_t(p.world.state),out.graph,e))return false;
        if(!out.graph.present)return fail(e,"VIP EndEvent prior UI owner absent: "+out.graph.absent);
        out.observed=true;
    }
    return pin(p,e);
}
bool after_end(Producer& p,uintptr_t receiver,const EndBefore& before,std::optional<std::string>& message,std::string& e){
    message.reset();if(!before.observed){e.clear();return true;}
    if(!pin(p,e))return false;
    {
        live_host_state::apply_timing::PageReadScope pages;
        std::vector<uintptr_t> members;uint8_t completed=0;
        if(!host_read::scenarios(host_memory,p.image,uintptr_t(p.world.state),members,e))return false;
        if(std::find(members.begin(),members.end(),receiver)==members.end()){e.clear();return true;}
        if(!host_read::at(host_memory,receiver,0x38,completed,e))return false;
        if(completed>1)return fail(e,"VIP EndEvent final completed flag is not boolean");
        if(!completed){e.clear();return true;}
        alert_probe::Graph now;alert_probe::Reader reader(host_memory,p.image,p.image_size);
        if(!reader.read_ui(uintptr_t(p.world.mode),uintptr_t(p.world.state),now,e)||
           !disconnect_read::ended(host_memory,p.image,receiver,before.completed,before.graph,now,message,e))return false;
    }
    return pin(p,e);
}
bool enter(Producer& p,std::string& e,bool within_update=false){
    if(GetCurrentThreadId()!=p.thread)return fail(e,"VIP Host API requires bound Mainthread");
    if(p.busy||(p.updating&&!within_update))return fail(e,"VIP Host observation is reentrant");
    return healthy(p,e);
}
struct Busy {Producer& p;explicit Busy(Producer& value):p(value){p.busy=true;}~Busy(){p.busy=false;}};
}
bool host_bind(const action_session::Context& context,const action_session::Binding& binding,uint64_t epoch,std::string& e){
    if(producer.busy||producer.updating)return fail(e,"VIP Host bind is reentrant");
    const wire::Origin origin{context.session,epoch,binding.native_generation,context.snapshot_uuid};
    if(context.role!=action_session::Role::Host||context.local_peer!=context.host_peer||context.local_peer<1||context.local_peer>4)
        return fail(e,"VIP Host bind requires actual Host peer");
    const auto peers=uint32_t(context.peer_mask&~(1u<<(context.local_peer-1)));
    if(producer.transitions.same_binding(origin,peers)&&producer.transitions.armed()){
        if(producer.context!=context||producer.binding!=binding)return fail(e,"VIP Host same-Origin binding changed");
        return enter(producer,e)&&pin(producer,e);
    }
    Producer next;next.transitions=producer.transitions;
    if(!next.transitions.bind(origin,peers,e))return false;
    next.context=context;next.binding=binding;
    std::string sha;
    if(!native::read_resolver_receipt(next.image,next.thread,sha,e)||!native_open_runtime::read_current_world(next.world,e))return false;
    if(!pin(next,e))return false;
    int32_t pe=0;
    if(!host_read::at(host_memory,next.image,0x3c,pe,e))return false;
    if(pe<0||pe>4096)return fail(e,"VIP Host PE offset outside bound");
    if(!host_read::at(host_memory,next.image,size_t(pe)+80,next.image_size,e))return false;
    // Seed only currently active arrivals in the new snapshot, never old history.
    if(!collect(next,0,e))return false;
    producer=std::move(next);wrong_thread.store(false,std::memory_order_release);
    observation_thread.store(producer.thread,std::memory_order_release);e.clear();return true;
}
bool host_suspend(const wire::Origin& origin,std::string& e){
    if(GetCurrentThreadId()!=producer.thread||producer.busy||producer.updating)return fail(e,"VIP Host suspend requires idle Mainthread");
    if(!producer.transitions.suspend(origin,e))return false;
    observation_thread.store(0,std::memory_order_release);
    return healthy(producer,e);
}
bool host_reset(const wire::Origin& origin,std::string& e){
    if(GetCurrentThreadId()!=producer.thread||producer.busy||producer.updating)return fail(e,"VIP Host reset requires idle Mainthread");
    if(!producer.transitions.reset(origin,e))return false;
    producer.failure.clear();producer.world={};producer.context={};producer.binding={};
    wrong_thread.store(false,std::memory_order_release);observation_thread.store(0,std::memory_order_release);e.clear();return true;
}
bool host_capture(const wire::Origin& origin,wire::Batch& out,std::string& e){
    out={};if(!enter(producer,e)||origin!=producer.transitions.origin()||!producer.transitions.armed())
        return fail(e,e.empty()?"VIP Host capture Origin or armed state differs":e);
    Busy busy(producer);
    if(!collect(producer,0,e)){latch(producer,e);return false;}
    return producer.transitions.capture(origin,out,e);
}
bool host_sent(const wire::Origin& origin,uint64_t frame,const wire::Batch& batch,std::string& e){
    return enter(producer,e)&&producer.transitions.sent(origin,frame,batch,e);
}
bool host_acknowledge(const wire::Origin& origin,uint32_t peer,uint64_t frame,std::string& e){
    return enter(producer,e)&&producer.transitions.acknowledge(origin,peer,frame,e);
}
void host_update(uintptr_t receiver,float dt,uintptr_t caller,HostUpdate original){
    const auto thread=observation_thread.load(std::memory_order_acquire);
    if(!thread){original(receiver,dt);return;}
    if(thread!=GetCurrentThreadId()){wrong_thread.store(true,std::memory_order_release);original(receiver,dt);return;}
    if(producer.updating){latch(producer,"VIP Host Update is reentrant");original(receiver,dt);return;}
    struct Updating {Updating(){producer.updating=true;}~Updating(){producer.updating=false;}} updating;
    EndBefore before;
    const auto observe=[&](bool pre,const std::optional<std::string>& message){
        std::string e;
        try{
            if(caller!=producer.image+0x4f43f5){latch(producer,"VIP Host Update caller differs from original ScenarioLog");return;}
            if(!enter(producer,e,true)){latch(producer,e);return;}
            Busy busy(producer);
            if(!collect(producer,receiver,e,message)){latch(producer,e);return;}
            if(pre&&!before_end(producer,receiver,before,e))latch(producer,e);
        }catch(const std::exception& exception){latch(producer,std::string("VIP Host observation exception: ")+exception.what());}
        catch(...){latch(producer,"VIP Host observation nonstandard exception");}
    };
    observe(true,std::nullopt);
    try{original(receiver,dt);}catch(...){latch(producer,"VIP Host original Update threw; event outcome unconfirmed");throw;}
    std::optional<std::string> message;
    if(producer.failure.empty()){
        std::string e;
        try{if(!after_end(producer,receiver,before,message,e))latch(producer,e);}
        catch(const std::exception& exception){latch(producer,std::string("VIP EndEvent observation exception: ")+exception.what());}
        catch(...){latch(producer,"VIP EndEvent observation nonstandard exception");}
    }
    observe(false,message);
}
}
