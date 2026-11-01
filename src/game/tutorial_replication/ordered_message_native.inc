namespace ordered_message_native {
namespace a=action_session;
ordered_message::Gate gate;
std::optional<World> world;
a::Context context;
a::Binding binding;
Call original_clear=nullptr;
tutorial_sync::OwnedSequenceEntry clear_owner;
thread_local const ordered_message::Current* updating=nullptr;
using Release=ordered_message::Release;
Release release;
ordered_message::Readback release_readback;
struct Receipt {a::Request request;a::TutorialProof before,after;bool retired=false;};
std::map<std::string,Receipt> receipts;
std::map<uintptr_t,NativeCursor> owners_cache;
void reset(){gate.reset();world.reset();context={};binding={};release={};release_readback.reset();receipts.clear();owners_cache.clear();}
bool current_world(World& out,a::Context& c,a::Binding& b,std::string& e,bool held_tutorial=false){
    if(load.armed||!fault.empty())return fail(e,fault.empty()?"Ordered tutorial requires inactive legacy load":fault);
    if(!(held_tutorial?typed_action_runtime::held_tutorial_session_read(a::selected_session(),c,e):a::selected_session().current(c,e))||
       !(held_tutorial?typed_action_runtime::held_tutorial_read_binding(c.snapshot_uuid,b,e):typed_action_runtime::read_binding(c.snapshot_uuid,b,e))||
       !native_open_runtime::read_current_world(out,e))return false;
    uintptr_t actual_image=0;uint32_t thread=0;std::string sha;
    if(!native::read_resolver_receipt(actual_image,thread,sha,e)||actual_image!=image||thread!=GetCurrentThreadId()||
       out.world_epoch!=b.native_epoch||out.generation!=b.native_generation)
        return fail(e,e.empty()?"Ordered tutorial actual image/thread/Binding differs":e);
    if(world&&(!same_world(*world,out)||context!=c||binding!=b))return fail(e,"Ordered tutorial retained World/Context changed before retirement");
    if(!world){world=out;context=c;binding=b;}
    if(!mainthread)mainthread=thread;
    return mainthread==thread||fail(e,"Ordered tutorial Mainthread differs");
}
bool find(const World& w,uintptr_t sequence,NativeCursor& cursor,std::string& e){
    uintptr_t log=0,data=0;int32_t count=0;cursor={};
    if(!at(reinterpret_cast<uintptr_t>(w.state),0x1e8,log,e)||!type(log,0x16fd7b0,e)||
       !lifecycle_native::array(log,0x20,64,data,count,e))return false;
    for(int32_t i=0;i<count;++i){
        uintptr_t instance=0,vtable=0,actual=0;
        if(!at(data,size_t(i)*8,instance,e)||!at(instance,0,vtable,e))return false;
        if(vtable!=image+0x173ce70)continue;
        if(!at(instance,0x58,actual,e))return false;if(actual!=sequence)continue;
        const auto cached=owners_cache.find(sequence);
        if(cached!=owners_cache.end()){
            if(cached->second.instance!=instance)return fail(e,"Ordered tutorial Sequence was rebound to another native instance");
            cursor=cached->second;return true;
        }
        std::string id;if(!str(instance,0x18,id,e))return false;if(!supported(id))return true;
        lifecycle_native::Owners owners;NativeCursor current;bool present=false;
        if(!lifecycle_native::owners(w,id,owners,e)||!lifecycle_native::owned_cursor(owners,id,current,present,e)||
           !present||current.sequence!=sequence||current.instance!=instance)return fail(e,e.empty()?"Ordered tutorial native asset owner differs":e);
        if(owners_cache.size()>=2)return fail(e,"Ordered tutorial nonrepeatable native owner count exceeds two");
        owners_cache.emplace(sequence,current);cursor=std::move(current);return true;
    }
    return true;
}
bool read(const NativeCursor& owner,ordered_message::Current& out,std::string& e){
    out={};uintptr_t steps=0,member=0;int32_t next=0,count=0;uint8_t done=0,complete=0;
    if(!type(owner.sequence,0x16dbd80,e)||!at(owner.sequence,0x20,steps,e)||!at(owner.sequence,0x28,count,e)||
       !at(owner.sequence,0x48,out.step,e)||!at(owner.sequence,0x50,next,e)||!at(owner.sequence,0x54,done,e))return false;
    if(done>1||count!=owner.value.count||next<0||next>count)return fail(e,"Ordered tutorial actual Sequence count/index/complete differs");
    out.sequence_complete=done!=0;if(done||!out.step)return true;
    if(!next||!at(steps,size_t(next-1)*8,member,e)||member!=out.step)return fail(e,e.empty()?"Ordered tutorial current Step is not its indexed member":e);
    uintptr_t vtable=0;if(!at(out.step,0,vtable,e)||!at(out.step,0x18,complete,e))return false;
    if(complete>1)return fail(e,"Ordered tutorial Step complete byte invalid");
    out.step_complete=complete!=0;out.message=vtable==image+0x16fda00;
    if(out.message){
        out.intent.asset="scenarios/"+owner.value.scenario;out.intent.index=next-1;
        if(!str(out.step,0x20,out.intent.step_id,e)||!at(out.step,0xa0,out.intent.message_type,e))return false;
        if(!a::tutorial_observed_identity(out.intent))return fail(e,"Ordered tutorial current Message identity invalid asset="+out.intent.asset+" index="+std::to_string(out.intent.index)+" type="+std::to_string(out.intent.message_type)+" step_id="+out.intent.step_id);
    }
    return true;
}
void clear(void* value){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    if(!updating||!updating->message||caller!=image+0x50538d||!typed_action_runtime::ordered_mode_selected()){original_clear(value);return;}
    std::string e;uintptr_t singleton=0;bool admitted=false;
    if(!typed_action_runtime::original_go_admission(admitted,e)||!admitted||
       GetCurrentThreadId()!=mainthread||!at(image,0x13df380,singleton,e)||singleton!=reinterpret_cast<uintptr_t>(value)||
       !gate.observed(updating->intent,true,e)){
        block(e.empty()?"Ordered tutorial Clear lacks exact native Message/caller/Go/Undo owner":e);return;
    }
    // Native Message Update may continue its local window/meter effects. Its
    // destructive Undo clear is admitted only by the shared message grant.
}
class Port final:public ordered_message::Port {
    NativeCursor owner_;
public:
    explicit Port(NativeCursor owner):owner_(std::move(owner)){}
    bool current(ordered_message::Current& out,std::string& e)override{return read(owner_,out,e);}
    bool update(uintptr_t step,float delta,std::string& e)override{
        ordered_message::Current current;uintptr_t vtable=0,fn=0;
        if(!read(owner_,current,e)||current.step!=step||!at(step,0,vtable,e)||!at(vtable,0xa0,fn,e))
            return fail(e,e.empty()?"Ordered tutorial Step Update owner changed":e);
        if(!fn)return fail(e,"Ordered tutorial Step Update pointer is null");
        const auto* previous=updating;updating=&current;
        struct Restore {const ordered_message::Current* previous;~Restore(){updating=previous;}} restore{previous};
        reinterpret_cast<Update>(fn)(reinterpret_cast<void*>(step),delta);
        return fault.empty()||fail(e,fault);
    }
    bool advance(uintptr_t step,bool clear_undo,std::string& e)override{
        ordered_message::Current current;
        if(!read(owner_,current,e)||current.step!=step)return fail(e,e.empty()?"Ordered tutorial release Step changed":e);
        if(current.message){
            release={};release.step=current.intent;release.step.clear_undo=clear_undo;
            if(clear_undo){
                uintptr_t undo=0;
                if(!at(image,0x13df380,undo,e)||!undo||!at(undo,0x18,release.undo_before,e)||!at(undo,0x30,release.redo_before,e)||
                   release.undo_before<0||release.redo_before<0)return fail(e,e.empty()?"Ordered tutorial release Undo owner/count invalid":e);
                release.clear_attempted=true;original_clear(reinterpret_cast<void*>(undo));release.clear_returned=true;
                if(!at(undo,0x18,release.undo_after,e)||!at(undo,0x30,release.redo_after,e)||release.undo_after||release.redo_after)
                    return fail(e,e.empty()?"Ordered tutorial original Clear left Undo/Redo entries":e);
            }
            release.advance_attempted=true;
        }
        original_advance(reinterpret_cast<void*>(owner_.sequence));
        if(current.message){
            release.advance_returned=true;
            if(fault.empty()&&!release_readback.released(release,e))return false;
        }
        return fault.empty()||fail(e,fault);
    }
};
bool frame(void* value,float delta){
    if(!typed_action_runtime::ordered_mode_selected())return false;
    bool admitted=false;std::string e;
    if(!typed_action_runtime::original_go_admission(admitted,e)){block(e);return true;}
    if(!admitted)return false;
    try{
        World w;a::Context c;a::Binding b;NativeCursor cursor;
        if(!current_world(w,c,b,e)||!find(w,reinterpret_cast<uintptr_t>(value),cursor,e)){block(e);return true;}
        if(!cursor.sequence)return false;
        Port port(cursor);
        if(!gate.frame(port,delta,e)){release.cause=e;block(e);}
    }catch(const std::exception& cause){release.cause=std::string("Ordered tutorial native release: ")+cause.what();block(release.cause);}
    return true;
}
bool locate(const a::TutorialIntent& intent,ordered_message::Current& current,std::string& e){
    World w;a::Context c;a::Binding b;if(!current_world(w,c,b,e))return false;
    const auto id=intent.asset.substr(10);lifecycle_native::Owners owners;NativeCursor cursor;bool present=false;
    if(!lifecycle_native::owners(w,id,owners,e)||!lifecycle_native::owned_cursor(owners,id,cursor,present,e)||!present||
       !read(cursor,current,e)||!current.message||!a::tutorial_same_step(intent,current.intent))
        return fail(e,e.empty()?"Ordered tutorial requested asset/index/type/ID is not current":e);
    return true;
}
a::Outcome perform(const a::Binding& b,const a::Request& request,std::string& e){
    if(request.action!=a::Action::TutorialAcknowledge||!a::tutorial_valid(request.tutorial)){
        e="Ordered tutorial acknowledgement action/identity invalid";return a::Outcome::RejectedNoEffect;
    }
    if(!a::tick_commands::authority::matches(b,request,e))return a::Outcome::RejectedNoEffect;
    if(receipts.contains(request.action_uuid)){e="Ordered tutorial command was already executed";return a::Outcome::RejectedNoEffect;}
    if(receipts.size()>=a::history_limit){e="Ordered tutorial command receipts exceed one bounded capture interval";return a::Outcome::RejectedNoEffect;}
    try{
        World w;a::Context c;a::Binding actual;
        if(!current_world(w,c,actual,e)||actual!=b){if(e.empty())e="Ordered tutorial consumed Binding differs from native World";return a::Outcome::RejectedNoEffect;}
        auto* existing=gate.find(request.tutorial);
        if(existing&&!a::tutorial_same_step(existing->intent,request.tutorial)){e="Ordered tutorial retained acknowledgement identity differs asset="+request.tutorial.asset+" index="+std::to_string(request.tutorial.index)+" retained_type="+std::to_string(existing->intent.message_type)+" requested_type="+std::to_string(request.tutorial.message_type)+" retained_id="+existing->intent.step_id+" requested_id="+request.tutorial.step_id;return a::Outcome::RejectedNoEffect;}
        const bool duplicate=existing&&existing->granted;
        if(!duplicate){ordered_message::Current current;if(!locate(request.tutorial,current,e))return a::Outcome::RejectedNoEffect;}
        // Two peers may independently complete the same current prompt. The
        // first ordered grant owns the native release; later grants do not replay it.
        Receipt out;out.request=request;out.before={request.tutorial,duplicate};
        if(!gate.grant(request.tutorial,e))return a::Outcome::RejectedNoEffect;
        if(!release_readback.grant(request,duplicate,e))return a::Outcome::Unknown;
        out.after={request.tutorial,true};receipts.emplace(request.action_uuid,std::move(out));e.clear();return a::Outcome::Confirmed;
    }catch(const std::exception& cause){e=std::string("Ordered tutorial grant: ")+cause.what();return a::Outcome::Unknown;}
}
bool observe(const a::Binding& b,const a::Request& request,a::Evidence& out,std::string& e){
    const auto found=receipts.find(request.action_uuid);
    if(!a::tick_commands::authority::matches(b,request,e)||found==receipts.end()||found->second.request!=request)
        return fail(e,e.empty()?"Ordered tutorial exact grant receipt unavailable":e);
    out.tutorial_before=found->second.before;out.tutorial_after=found->second.after;out.retirement_confirmed=found->second.retired;e.clear();return true;
}
bool retire(const a::Binding& b,const a::Request& request,std::string& e){
    const auto found=receipts.find(request.action_uuid);
    if(!a::tick_commands::authority::matches(b,request,e)||found==receipts.end()||found->second.request!=request)
        return fail(e,e.empty()?"Ordered tutorial exact grant retirement unavailable":e);
    if(!found->second.retired&&!release_readback.retire(request,e))return false;
    found->second.retired=true;e.clear();return true;
}
void tick(bool held_tutorial){
    if(!world)return;
    try{
        std::string e;World w;a::Context c;a::Binding b;
        a::Context capture_context;a::Binding capture_binding;bool ready=false;
        if(!(held_tutorial?typed_action_runtime::held_tutorial_capture(capture_context,capture_binding,ready,e):typed_action_runtime::read_ordered_tutorial_capture(capture_context,capture_binding,ready,e))){block(e);return;}
        if(!ready)return;
        if(!current_world(w,c,b,e,held_tutorial)){block(e);return;}
        if(capture_context!=c||capture_binding!=b){block("Ordered tutorial capture Context/Binding changed after readiness");return;}
        // Execution observation calls retire and then observes again. A receipt
        // can be discarded only at this later quiescent capture boundary.
        receipts.clear();
        for(const auto& [key,value]:gate.entries()){
            if(!value.observed||value.submitted||value.granted)continue;
            std::string id;if(!intent_uuid(id,e)){block(e);return;}
            bool attempted=false;
            const bool accepted=typed_action_runtime::submit_tutorial(id,value.intent,attempted,e);
            auto* entry=gate.find(value.intent);entry->submitted=attempted;
            if(!accepted)block(e);
            return;
        }
    }catch(const a::tick_commands::HeldMembershipInterruption&){throw;}
    catch(const std::exception& cause){block(std::string("Ordered tutorial capture: ")+cause.what());}
}
 nlohmann::json status(){
    size_t observed=0,submitted=0,granted=0,released=0;
    for(const auto& [key,value]:gate.entries()){observed+=value.observed;submitted+=value.submitted;granted+=value.granted;released+=value.released;}
    return {{"messages",gate.entries().size()},{"observed",observed},{"submitted",submitted},{"granted",granted},{"release_attempted",released},
        {"fault",fault},{"last_release",ordered_message::release_json(release)},{"readback",release_readback.status()}};
}
}
