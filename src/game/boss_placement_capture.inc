namespace boss_placement {
namespace {
using Release=void(*)(void*,void*,const void*);
Release original_release=nullptr;bool installed=false,install_attempted=false;
struct Pending {action::Binding binding;std::string snapshot,id;boss_placement_reader::Source source;};
std::optional<Pending> captured;
uint64_t captures=0,submissions=0,rejections=0,forwarded=0;std::string capture_cause;
void reject(const std::string& error){++rejections;if(capture_cause!=error){capture_cause=error;LOG_ERROR("[BossPlacement] {}",error);}}
bool idle(Runtime& r,action::Binding& binding,std::string& error){
    const auto controller=typed_action_runtime::controller_status();
    if(!r.ready||r.thread!=GetCurrentThreadId()||captured||r.pending||duplicate_edit::pending()||group_edit::pending()||
       group_object::pending()||selection_move::pending()||scenery_stroke::pending()||
       !controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))
        return q::fail(error,"Boss placement requires the idle bound multiplayer Mainthread");
    return typed_action_runtime::network_binding(net::current_snapshot_id(),binding,error);
}
bool capture(Runtime& r,uintptr_t receiver,uintptr_t cursor,uintptr_t point,std::string& error){
    action::Binding binding;if(!idle(r,binding,error))return false;
    action::Context context;if(!action::selected_session().current(context,error))return false;
    auto resolve=[&](action::BossPlacementIntent& intent,std::string& cause){return source_intent(r,context,binding,intent,cause);};
    boss_placement_reader::Source source;
    if(!boss_placement_reader::read(r.memory,r.image,receiver,cursor,point,resolve,source,error))return false;
    const auto id=uuid(error);if(id.empty())return false;
    captured=Pending{binding,net::current_snapshot_id(),id,std::move(source)};++captures;return true;
}
void release_entry(void* receiver,void* cursor,const void* point){
    auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||r->thread!=GetCurrentThreadId()||!net::session_wire::connected()){
        ++forwarded;original_release(receiver,cursor,point);return;
    }
    std::string error;uint8_t valid=0;
    if(!q::typed(r->memory,r->image,uintptr_t(receiver),boss_placement_reader::addresspoint,error)||
       !q::at(r->memory,uintptr_t(receiver),0x3b1,valid,error)){reject(error);return;}
    if(!valid){++forwarded;original_release(receiver,cursor,point);return;}
    try{if(!capture(*r,uintptr_t(receiver),uintptr_t(cursor),uintptr_t(point),error))reject(error);}
    catch(const std::exception& exception){reject(std::string("Boss placement source capture: ")+exception.what());}
}
struct Cleanup {
    Runtime& r;
    bool current(uintptr_t cursor,uintptr_t& active,std::string& error){return boss_placement_reader::current(r.memory,r.image,cursor,active,error);}
    bool source(const boss_placement_reader::Source& value,std::string& error){uintptr_t visual=0;
        return code(r,true,error)&&q::typed(r.memory,r.image,value.receiver,boss_placement_reader::addresspoint,error)&&
            q::at(r.memory,value.receiver,0x330,visual,error)&&(visual==value.visual||q::fail(error,"Boss captured source visual lifetime changed before cleanup"));}
    bool cancel(uintptr_t cursor,std::string& error){const auto code=boss_placement_calls::cancel(r.image,cursor);return !code||q::fail(error,"Boss native local Cursor cleanup SEH="+std::to_string(code));}
    bool clear(uintptr_t receiver,std::string& error){const auto code=boss_placement_calls::clear(r.image,receiver);uint8_t valid=1;
        return (!code||q::fail(error,"Boss exact source valid flag cleanup SEH="+std::to_string(code)))&&q::at(r.memory,receiver,0x3b1,valid,error)&&
            (!valid||q::fail(error,"Boss exact source valid flag did not clear"));}
};
}
bool pending(){return bool(captured);}
void tick(){
    if(!net::session_wire::connected()){retire_session();return;}
    if(!captured)return;auto value=std::move(*captured);captured.reset();auto* r=published.load(std::memory_order_acquire);std::string error;
    action::Binding binding;action::Context context;
    if(!r||value.snapshot!=net::current_snapshot_id()||!idle(*r,binding,error)||binding!=value.binding||!action::selected_session().current(context,error)){
        reject(error.empty()?"Boss source session changed before local cleanup":error);return;}
    auto intent=value.source.intent;
    if(!source_intent(*r,context,binding,intent,error)||intent!=value.source.intent){reject(error.empty()?"Boss selected Character or Region changed before queued submit":error);return;}
    Cleanup cleanup{*r};
    if(!boss_placement_reader::cleanup(cleanup,value.source,error)||!typed_action_runtime::submit_boss_placement(value.id,value.source.intent,error)){reject(error);return;}
    ++submissions;
}
}
