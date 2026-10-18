#include "time_runtime.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <deque>
#include <set>
#include <stdexcept>

namespace mt2mp::game::time_runtime {
namespace {
wire::Report failure(wire::Code code,const std::string& cause) {
    wire::Report r{};r.code=code;r.cause=cause;return r;
}
template<class F> bool call(F&& f,std::string& e) {
    try {return f();} catch(const std::exception& x) {e=std::string("callback_exception: ")+x.what();}
    catch(...) {e="callback_exception: unknown C++ exception";} return false;
}
struct Busy {
    bool& value; explicit Busy(bool& v):value(v){value=true;} ~Busy(){value=false;}
};
bool same(const Session& a,const Session& b) {
    return a.epoch==b.epoch && a.role==b.role && a.local_peer==b.local_peer && a.peer_mask==b.peer_mask;
}
bool clock_domain(const wire::Clock& c) {
    return std::isfinite(c.selected)&&c.selected>=0&&std::isfinite(c.pause)&&c.pause>=0;
}
bool ui_domain(float s) {return s==0||s==1||s==3||s==9;}
}
struct Runtime::Impl {
    ui::Config config{};Services services{};Native native{};ui::Bridge bridge;
    wire::Controller controller;Status state{};Session bound{};
    bool initialized=false,busy=false,bridge_bound=false,controller_started=false;
    bool pending=false,packet_ready=false,intent_pending=false;wire::Intent intent{};wire::Outbound outbound{};
    std::array<uint8_t,64> packet{};
    std::set<uint64_t> retired;
    std::deque<wire::Message> early,receipts;
    struct Lease {uintptr_t game=0,mode=0,world=0,clock=0,player=0,inventory=0;
        bool operator==(const Lease&)const=default;} lease{};
    std::array<std::pair<uintptr_t,uintptr_t>,4> items{};
    uint64_t repeats=0;std::string logged;
    bool thread(std::string& e) const {
        if(!initialized){e="runtime_not_initialized";return false;}
        if(native.memory.thread_id(native.memory.user)!=config.game_thread){e="wrong_game_thread";return false;}
        if(busy){e="runtime_reentrant";return false;}return true;
    }
    void event(const char* operation,const std::string& e,const char* level="error") {
        state.last_operation=operation;state.last_error=e;
        const std::string key=std::string(operation)+":"+e;
        if(key==logged){++repeats;state.last_reason_repeats=repeats;return;}
        if(services.log){
            try {services.log(services.user,level,operation,e,1);}
            catch(const std::exception& x){state.last_error=e+"; logger_exception: "+x.what();}
            catch(...){state.last_error=e+"; logger_exception: unknown C++ exception";}
        }
        logged=key;repeats=1;state.last_reason_repeats=1;
    }
    void recovered(const char* operation) {
        if(!state.last_error.empty())event(operation,"recovered", "info");
        state.last_error.clear();
    }
    bool session(Session& s,std::string& e) {
        if(!call([&]{return services.session(services.user,s,e);},e))return false;
        if(s.local_peer<1||s.local_peer>4||!(s.peer_mask&1)||s.peer_mask>15||
           !(s.peer_mask&(1u<<(s.local_peer-1)))||
           ((s.role==wire::Role::Host)!=(s.local_peer==1))){e="authenticated_session_membership_invalid";return false;}
        return true;
    }
    template<class T> bool get(uintptr_t address,T& out,std::string& e) {
        if(!address||address>UINTPTR_MAX-sizeof(T)){e="native_address_invalid";return false;}
        return call([&]{return native.memory.read(native.memory.user,address,&out,sizeof(out),e);},e);
    }
    bool vt(uintptr_t p,uintptr_t rva,const char* kind,std::string& e) {
        uintptr_t actual=0;if(!get(p,actual,e)){e=std::string(kind)+": "+e;return false;}
        if(actual!=config.image+rva){e=std::string(kind)+"_vtable_mismatch";return false;}return true;
    }
    bool witness(Lease& l,std::string& e) {
        uintptr_t active=0;int32_t mode_state=-1;uint8_t quitting=0;
        if(!get(config.image+0x13df8f0,l.game,e)||!vt(l.game,0x1743740,"game",e)||
           !get(config.image+0x13df320,l.mode,e)||!vt(l.mode,0x16e6730,"mode",e)||
           !get(l.game+0x28,active,e)||!get(l.mode+0x2c4,mode_state,e)||!get(l.mode+0x304,quitting,e)||
           !get(l.mode+0x58,l.world,e))return false;
        if(active!=l.mode||mode_state!=4||quitting||!l.world){e="active_world_state4_required actual_state="+std::to_string(mode_state);return false;}
        return get(config.image+0x13df9a0,l.clock,e)&&vt(l.clock,0x17499d0,"clock",e)&&
            get(config.image+0x13df9f0,l.player,e)&&vt(l.player,0x17873b0,"player",e)&&
            get(l.player+0x1d0,l.inventory,e)&&vt(l.inventory,0x16df0f0,"inventory",e);
    }
    bool lease_guard(std::string& e) {
        Session s{};Lease current{};
        if(!session(s,e)||!s.snapshot_active||!same(s,bound)){if(e.empty())e="session_binding_changed";return false;}
        if(!witness(current,e)||!(current==lease)){if(e.empty())e="native_world_or_actor_changed";return false;}
        if(bound.role==wire::Role::Client){
            uint64_t epoch=0;
            if(!call([&]{return native.client_epoch(native.user,epoch,e);},e))return false;
            if(epoch!=bound.epoch){e="software_epoch_mismatch";return false;}
        }
        return true;
    }
    static uint32_t tid(void* p){auto& s=*static_cast<Impl*>(p);return s.native.memory.thread_id(s.native.memory.user);}
    static bool context(void* p,ui::Context& c,std::string& e){
        auto& s=*static_cast<Impl*>(p);Session session{};if(!s.session(session,e))return false;
        if(!session.snapshot_active||!session.epoch){e="snapshot_not_active";return false;}
        c={session.epoch,session.role==wire::Role::Client,session.role==wire::Role::Host};return true;
    }
    static bool read(void* p,uintptr_t at,void* out,size_t count,std::string& e){
        auto& s=*static_cast<Impl*>(p);return call([&]{return s.native.memory.read(s.native.memory.user,at,out,count,e);},e);
    }
    static bool write(void* p,uintptr_t at,int32_t before,int32_t target,std::string& e){
        auto& s=*static_cast<Impl*>(p);++s.state.item_stores_attempted;
        if(!call([&]{return s.native.memory.compare_write(s.native.memory.user,at,before,target,e);},e)){
            s.state.presentation_uncertain=true;return false;}
        ++s.state.item_stores_completed;return true;
    }
    static bool host_read(void* p,uint64_t epoch,wire::Clock& c,std::string& e){
        auto& s=*static_cast<Impl*>(p);
        if(epoch!=s.bound.epoch||s.bound.role!=wire::Role::Host||!s.lease_guard(e)){if(e.empty())e="host_epoch_role_mismatch";return false;}
        if(!call([&]{return s.native.read_clock(s.native.user,c,e);},e))return false;
        if(!clock_domain(c)){e="native_clock_nonfinite_or_negative";return false;}return true;
    }
    static wire::Effect host_set(void* p,uint64_t epoch,float target,std::string& e){
        auto& s=*static_cast<Impl*>(p);wire::Clock before{};
        if(!host_read(p,epoch,before,e))return wire::Effect::RejectedNoEffect;
        ++s.state.native_set_attempted;
        if(!call([&]{return s.native.set_clock(s.native.user,target,e);},e)){
            ++s.state.native_set_failed;s.event("set_host_clock",e);return wire::Effect::Unknown;}
        wire::Clock after{};
        if(!host_read(p,epoch,after,e)||after.selected!=target){
            if(e.empty())e="native_set_clock_readback_mismatch";
            ++s.state.native_set_failed;s.event("set_host_clock",e);return wire::Effect::Unknown;}
        ++s.state.native_set_confirmed;return wire::Effect::Confirmed;
    }
    static wire::Effect presentation(void* p,uint64_t epoch,const wire::Snapshot& desired,wire::PresentationReadback& out,std::string& e){
        auto& s=*static_cast<Impl*>(p);
        if(s.state.presentation_uncertain){e="presentation_uncertain_requires_new_epoch";return wire::Effect::Unknown;}
        if(epoch!=s.bound.epoch||s.bound.role!=wire::Role::Client||!s.lease_guard(e)){
            if(e.empty())e="client_epoch_role_mismatch";return wire::Effect::RejectedNoEffect;}
        if(s.state.hook_mask!=3){e="client_hooks_not_ready";return wire::Effect::RejectedNoEffect;}
        wire::Clock clock{};ui::Presentation before{},after{};
        if(!call([&]{return s.native.read_clock(s.native.user,clock,e);},e)||clock.selected!=0||
           !s.bridge.read(before,e)||!ui_domain(desired.clock.selected)){
            if(e.empty())e=clock.selected!=0?"client_native_clock_not_paused":"host_scale_outside_ui_domain";
            return wire::Effect::RejectedNoEffect;}
        if(!s.bridge.set_host_state(epoch,desired.revision,desired.clock.selected,desired.clock.pause,e))return wire::Effect::RejectedNoEffect;
        const auto attempts=s.state.item_stores_attempted;
        if(!call([&]{return s.bridge.enable_capture(3,e)&&s.bridge.read(after,e);},e)||
           !call([&]{return s.native.read_clock(s.native.user,clock,e);},e)||clock.selected!=0){
            if(e.empty())e="presentation_native_clock_readback_not_paused";
            if(s.state.item_stores_attempted!=attempts||s.state.presentation_uncertain){s.state.presentation_uncertain=true;return wire::Effect::Unknown;}
            return wire::Effect::RejectedNoEffect;
        }
        for(size_t i=0;i<4;++i){
            const int32_t wanted=before.items[i].scale==desired.clock.selected*desired.clock.pause?0:
                before.items[i].native_state==0?1:before.items[i].native_state;
            if(after.items[i].native_state!=wanted){e="presentation_item_readback_mismatch index="+std::to_string(i);s.state.presentation_uncertain=true;return wire::Effect::Unknown;}
        }
        if(after.epoch!=epoch||after.revision!=desired.revision||after.selected_scale!=desired.clock.selected||after.pause_scale!=desired.clock.pause){
            e="presentation_revision_readback_mismatch";s.state.presentation_uncertain=true;return wire::Effect::Unknown;}
        out={desired.revision,desired.clock,0,true};return wire::Effect::Confirmed;
    }
    bool capture_pairs(std::string& e){
        uintptr_t array=0;int32_t count=0;std::array<bool,4> found{};
        if(!get(lease.inventory+8,array,e)||!get(lease.inventory+0x10,count,e)||count<4||count>4096){if(e.empty())e="inventory_count_invalid";return false;}
        for(int32_t n=0;n<count;++n){uintptr_t item=0,type=0,vptr=0;float scale=0;
            if(!get(array+static_cast<uintptr_t>(n)*8,item,e))return false;
            if(!item)continue;
            if(!get(item+0x68,type,e)||!get(type,vptr,e))return false;
            if(vptr!=config.image+0x1723f50)continue;
            if(!get(type+0x15c,scale,e))return false;
            const std::array<float,4> scales{0,1,3,9};
            for(size_t i=0;i<4;++i)if(scale==scales[i]){if(found[i]){e="duplicate_time_pair";return false;}items[i]={type,item};found[i]=true;}
        }
        if(std::find(found.begin(),found.end(),false)!=found.end()){e="missing_time_pair";return false;}return true;
    }
    bool match(void* type,void* item)const{
        return std::find(items.begin(),items.end(),std::pair{reinterpret_cast<uintptr_t>(type),reinterpret_cast<uintptr_t>(item)})!=items.end();
    }
    void remember(const wire::Message& m){
        for(auto& saved:receipts)if(saved.epoch==m.epoch&&saved.peer==m.peer&&saved.sequence==m.sequence)return;
        if(receipts.size()==64)receipts.pop_front();receipts.push_back(m);
    }
    bool flush(std::string& e){
        if(!controller_started)return true;
        for(size_t i=0;i<wire::outbox_size+1;++i){
            if(!pending){bool available=false;auto r=controller.pop(outbound,available);
                if(!r.okay()){e=r.cause;return false;}if(!available)return true;
                if(outbound.message.kind==wire::Kind::Receipt)remember(outbound.message);
                if(bound.role==wire::Role::Host&&outbound.target==1)continue;
                pending=true;packet_ready=false;
            }
            if(!packet_ready){if(!wire::encode(outbound.message,packet,e)){event("encode_outbound",e);return false;}packet_ready=true;}
            SendResult result=SendResult::Failed;
            bool called=call([&]{result=services.send(services.user,outbound.target,packet,e);return true;},e);
            if(called&&result==SendResult::Accepted){pending=false;++state.sends_accepted;continue;}
            if(called&&result==SendResult::Backpressure){++state.sends_waiting;if(e.empty())e="transport_backpressure";event("send_time_packet",e,"info");return false;}
            ++state.sends_failed;if(e.empty())e="transport_rejected_time_packet";event("send_time_packet",e);return false;
        }
        e="outbox_drain_bound_reached";event("send_time_packet",e,"warn");return false;
    }
    bool retire(const std::string& reason,std::string& e){
        if(bound.epoch)retired.insert(bound.epoch);
        if(intent_pending){++state.cancelled_intents;intent_pending=false;}
        if(bridge_bound){ui::Counters counters{};std::string detail;
            if(!bridge.counters(counters,detail)){e=detail;event("suspend_bridge_counters",detail);return false;}
            const auto before=counters.cancelled_on_unbind;
            if(!bridge.unbind(detail)){e=detail;event("suspend_bridge",detail);return false;}bridge_bound=false;
            if(!bridge.counters(counters,detail)){e=detail;event("suspend_bridge_counters",detail);return false;}
            state.cancelled_intents+=counters.cancelled_on_unbind-before;}
        state.phase=Phase::Retiring;event("suspend",reason,"warn");
        return true;
    }
    bool finish_retire(std::string& e){
        if(!flush(e))return false;
        if(controller_started){auto r=controller.stop();if(!r.okay()){e=r.cause;return false;}controller_started=false;}
        bound={};lease={};items={};state.epoch=0;state.phase=Phase::Waiting;state.presentation_uncertain=false;
        return true;
    }
    bool ensure(std::string& e){
        if(state.phase==Phase::Retiring&&!finish_retire(e))return false;
        Session s{};if(!session(s,e))return false;
        if(bridge_bound){if(!lease_guard(e)){const auto cause=e;std::string ignored;if(!retire(cause,ignored))e+="; "+ignored;return false;}return true;}
        if(!s.snapshot_active||!s.epoch){e="snapshot_not_active";return false;}
        if(retired.count(s.epoch)){e="retired_epoch_requires_new_snapshot";return false;}
        Lease current{};if(!witness(current,e))return false;
        if(s.role==wire::Role::Client){uint64_t epoch=0;if(!call([&]{return native.client_epoch(native.user,epoch,e);},e))return false;
            if(epoch!=s.epoch){e="software_epoch_mismatch";return false;}}
        if(!bridge.bind(s.epoch,e))return false;
        bridge_bound=true;bound=s;lease=current;state.epoch=s.epoch;state.phase=Phase::ReadOnly;
        if(!capture_pairs(e)){std::string detail;if(!retire(e,detail))e+="; "+detail;return false;}return true;
    }
    bool start(std::string& e){
        if(controller_started)return true;
        const uint32_t required=bound.role==wire::Role::Host?1:3;
        if((state.hook_mask&required)!=required){e="time_hooks_not_ready";return false;}
        const wire::Config c{bound.role,bound.epoch,bound.local_peer,bound.peer_mask,config.game_thread};
        auto r=controller.start(c,{this,tid,host_read,host_set,presentation});
        if(!r.okay()){e=r.cause;return false;}controller_started=true;state.phase=Phase::Active;
        if(bound.role==wire::Role::Client){
            for(auto it=early.begin();it!=early.end();++it)if(it->epoch==bound.epoch){auto m=*it;early.erase(it);r=controller.receive(1,m);if(!r.okay()){e=r.cause;return false;}break;}
        }
        return true;
    }
    bool host_bridge(std::string& e){
        if(bound.role!=wire::Role::Host)return true;
        wire::Status s{};auto r=controller.status(s);if(!r.okay()){e=r.cause;return false;}
        if(!s.observed.present){e="host_clock_state_absent";return false;}
        if(s.write_locked){e="host_time_write_locked";return false;}
        if(!ui_domain(s.observed.clock.selected)){
            if(!bridge.disable_capture(e))return false;
            e="host_scale_outside_capture_domain_original_forward";return false;}
        return bridge.set_host_state_and_enable_capture(bound.epoch,s.observed.revision,s.observed.clock.selected,s.observed.clock.pause,state.hook_mask,e);
    }
};
Runtime::Runtime():impl_(std::make_unique<Impl>()){}
Runtime::~Runtime()=default;
bool Runtime::initialize(const ui::Config& c,const Services& s,std::string& e){Native n{};if(!production_backend(c,n,e))return false;return initialize_with_backend(c,s,n,e);}
bool Runtime::initialize_with_backend(const ui::Config& c,const Services& s,const Native& n,std::string& e){
    auto& x=*impl_;if(x.initialized){e="runtime_already_initialized";return false;}
    if(!s.session||!s.send||!n.memory.read||!n.memory.compare_write||!n.memory.thread_id||!n.read_clock||!n.set_clock||!n.client_epoch){e="runtime_callbacks_missing";return false;}
    x.config=c;x.services=s;x.native=n;
    if(!x.bridge.initialize(c,{&x,Impl::read,Impl::write,Impl::tid,Impl::context},e))return false;
    x.initialized=true;x.state.phase=Phase::Waiting;return true;
}
bool Runtime::set_hook_mask(uint32_t mask,std::string& e){
    auto& x=*impl_;if(!x.thread(e))return false;Busy b(x.busy);
    if(mask>3){e="time_hook_mask_invalid";return false;}
    x.state.hook_mask=mask;
    const uint32_t required=x.bound.role==wire::Role::Host?1u:3u;
    if(x.controller_started&&(mask&required)!=required)return x.retire("time_hooks_removed",e);
    return true;
}
bool Runtime::tick(std::string& e){
    auto& x=*impl_;if(!x.thread(e))return false;Busy b(x.busy);
    if(!x.ensure(e)||!x.start(e)){x.event("tick_time",e,"warn");return false;}
    if(!x.flush(e))return false;
    if(x.bound.role==wire::Role::Host){auto r=x.controller.poll_host();if(!r.okay()){e=r.cause;x.event("poll_host_clock",e);return false;}
        if(!x.host_bridge(e)){x.event("host_ui_capture",e,"warn");std::string send_error;if(!x.flush(send_error))e+="; "+send_error;return false;}}
    if(!x.intent_pending){ui::Request request{};bool available=false;
        if(!x.bridge.pop_request(request,available,e)){x.event("pop_ui_intent",e);return false;}
        if(available){x.intent=static_cast<wire::Intent>(request.intent);x.intent_pending=true;}}
    if(x.intent_pending){auto r=x.controller.submit(x.intent);
        if(r.sequence||r.okay())x.intent_pending=false;
        if(!r.okay()){e=r.cause;x.event("submit_ui_intent",e);return false;}}
    if(!x.flush(e))return false;x.recovered("tick_time");return true;
}
wire::Report Runtime::submit(wire::Intent i){
    auto& x=*impl_;std::string e;if(!x.thread(e))return failure(wire::Code::WrongThread,e);Busy b(x.busy);
    if(!x.ensure(e)||!x.start(e))return failure(wire::Code::NotStarted,e);
    auto r=x.controller.submit(i);if(!r.okay())x.event("submit_time",r.cause);
    return r;
}
wire::Report Runtime::receive(uint8_t sender,const uint8_t* bytes,size_t size){
    auto& x=*impl_;std::string e;if(!x.thread(e))return failure(wire::Code::WrongThread,e);Busy b(x.busy);
    wire::Message m{};if(!wire::decode(bytes,size,m,e))return failure(wire::Code::Malformed,e);
    Session s{};if(!x.session(s,e))return failure(wire::Code::UntrustedSender,e);
    if(sender<1||sender>4||!(s.peer_mask&(1u<<(sender-1))))return failure(wire::Code::UntrustedSender,"authenticated_sender_not_member");
    const bool client_origin=sender==1&&((m.kind==wire::Kind::State&&m.peer==1)||
        (m.kind==wire::Kind::Receipt&&m.peer==s.local_peer));
    const bool host_origin=(m.kind==wire::Kind::Request||m.kind==wire::Kind::PresentationAck)&&m.peer==sender&&sender!=1;
    if(!(s.role==wire::Role::Client?client_origin:host_origin))return failure(wire::Code::UntrustedSender,"time_packet_role_or_target_invalid");
    if(s.role==wire::Role::Client&&m.kind==wire::Kind::State&&sender==1&&m.peer==1&&
       (!x.controller_started||!s.snapshot_active||m.epoch!=x.bound.epoch)){
        if(x.retired.count(m.epoch))return failure(wire::Code::WrongEpoch,"early_state_epoch_retired");
        if(s.snapshot_active&&s.epoch&&m.epoch!=s.epoch)return failure(wire::Code::WrongEpoch,"early_state_not_current_epoch");
        for(auto& old:x.early)if(old.epoch==m.epoch){
            if(m.state.revision<old.state.revision){wire::Report r{};r.disposition=wire::Disposition::Ignored;return r;}
            if(m.state.revision==old.state.revision){std::array<uint8_t,64> prior{};if(!wire::encode(old,prior,e))return failure(wire::Code::Malformed,e);
                if(std::memcmp(prior.data(),bytes,64))return failure(wire::Code::ConflictingDuplicate,"early_state_revision_conflict");
                wire::Report r{};r.disposition=wire::Disposition::Duplicate;return r;}
            old=m;++x.state.early_states;wire::Report r{};r.disposition=wire::Disposition::Accepted;return r;
        }
        if(x.early.size()==4)return failure(wire::Code::Backpressure,"early_state_epoch_capacity");
        x.early.push_back(m);++x.state.early_states;wire::Report r{};r.disposition=wire::Disposition::Accepted;return r;
    }
    if(!x.ensure(e)||!x.start(e))return failure(wire::Code::NotStarted,e);
    auto r=x.controller.receive(sender,m);
    if(s.role==wire::Role::Client&&m.kind==wire::Kind::Receipt&&sender==1&&m.peer==s.local_peer&&
       r.code!=wire::Code::Backpressure&&r.code!=wire::Code::UnexpectedReceipt&&r.code!=wire::Code::ConflictingDuplicate&&r.code!=wire::Code::WrongEpoch&&r.code!=wire::Code::UntrustedSender&&r.code!=wire::Code::Malformed)x.remember(m);
    if(!r.okay())x.event("receive_time",r.cause);return r;
}
bool Runtime::read_presentation(ui::Presentation& p,std::string& e){auto& x=*impl_;if(!x.thread(e))return false;Busy b(x.busy);return x.ensure(e)&&x.bridge.read(p,e);}
ui::HookResult Runtime::on_activate(void* type,void* item,uintptr_t caller){
    auto& x=*impl_;ui::HookResult r{};std::string e;
    if(!x.thread(e)){r.error=e;if(x.bridge_bound&&x.bound.role==wire::Role::Client&&x.match(type,item)){r.matched=true;r.decision=ui::Decision::Suppress;}return r;}Busy b(x.busy);
    if(!x.bridge_bound)return r;
    if(!x.lease_guard(e)){std::string detail;if(!x.retire(e,detail))e+="; "+detail;r.error=e;return r;}
    if(x.bound.role==wire::Role::Host&&x.match(type,item)&&x.controller_started){wire::Status status{};auto result=x.controller.status(status);
        if(!result.okay()){r.error=result.cause;r.matched=true;r.decision=ui::Decision::Suppress;return r;}
        if(status.write_locked){r.matched=true;r.decision=ui::Decision::Suppress;r.error="host_time_write_locked";return r;}}
    if(x.bound.role==wire::Role::Client&&x.match(type,item)&&x.state.presentation_uncertain){r.matched=true;r.decision=ui::Decision::Suppress;r.error="presentation_uncertain_requires_new_epoch";return r;}
    if(caller>UINTPTR_MAX-x.config.image){r.error="caller_rva_overflow";return r;}
    if(!call([&]{r=x.bridge.on_activate(type,item,x.config.image+caller);return true;},e)){
        r.error=e;r.matched=x.match(type,item);if(r.matched)r.decision=ui::Decision::Suppress;
    }
    if(x.bound.role==wire::Role::Client&&r.matched){r.decision=ui::Decision::Suppress;
        if(x.state.presentation_uncertain){r.request_queued=false;r.error="presentation_uncertain_requires_new_epoch";}
        else if(!r.request_queued&&r.error.empty()){ui::Presentation p{};if(!x.bridge.read(p,e))r.error=e;else if(!p.host_state_available)r.error="waiting_for_initial_host_state";}}
    if(!r.error.empty())x.event("ui_activate",r.error,"warn");return r;
}
ui::HookResult Runtime::on_update(void* type,void* item){
    auto& x=*impl_;ui::HookResult r{};std::string e;if(!x.thread(e)){r.error=e;if(x.bridge_bound&&x.bound.role==wire::Role::Client&&x.match(type,item)){r.matched=true;r.decision=ui::Decision::Suppress;}return r;}Busy b(x.busy);
    if(!x.bridge_bound)return r;
    if(!x.lease_guard(e)){std::string detail;if(!x.retire(e,detail))e+="; "+detail;r.error=e;return r;}
    bool controller_locked=false;
    if(x.controller_started){wire::Status status{};auto report=x.controller.status(status);if(!report.okay()){e=report.cause;controller_locked=true;}else controller_locked=status.write_locked;}
    if(x.bound.role==wire::Role::Client&&x.match(type,item)&&(x.state.presentation_uncertain||controller_locked)){r.matched=true;r.decision=ui::Decision::Suppress;r.error=e.empty()?"presentation_uncertain_requires_new_epoch":e;return r;}
    const auto attempts=x.state.item_stores_attempted;
    if(!call([&]{r=x.bridge.on_update(type,item);return true;},e)){r.error=e;r.matched=x.match(type,item);if(r.matched&&x.bound.role==wire::Role::Client)r.decision=ui::Decision::Suppress;}
    if(!r.error.empty()&&x.state.item_stores_attempted!=attempts)x.state.presentation_uncertain=true;
    if(!r.error.empty())x.event("ui_update",r.error);
    return r;
}
bool Runtime::suspend(const std::string& reason,std::string& e){auto& x=*impl_;if(!x.thread(e))return false;Busy b(x.busy);return x.retire(reason,e);}
bool Runtime::observe_receipt(uint8_t peer,uint64_t seq,wire::Message& m,bool& found,std::string& e){
    auto& x=*impl_;found=false;if(!x.thread(e))return false;
    for(auto it=x.receipts.rbegin();it!=x.receipts.rend();++it)if(it->epoch==x.state.epoch&&it->peer==peer&&it->sequence==seq){m=*it;found=true;return true;}return true;
}
bool Runtime::status(Status& s,std::string& e){auto& x=*impl_;if(!x.thread(e))return false;s=x.state;s.pending_outbound=x.pending;
    if(x.controller_started){auto r=x.controller.status(s.controller);if(!r.okay()){e=r.cause;return false;}}return true;}
}
