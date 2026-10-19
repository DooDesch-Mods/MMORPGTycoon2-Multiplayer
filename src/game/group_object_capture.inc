namespace group_object {
namespace {
struct Pending {action::Binding binding;std::string id,snapshot;group_object_move_reader::Capture source;};
std::optional<Pending> captured;
uint64_t captures=0,submissions=0,rejections=0;std::string capture_cause;
void reject(const std::string& e){++rejections;if(capture_cause!=e){capture_cause=e;LOG_ERROR("[GroupObject] {}",e);}}
bool intent_for(const group_edit::Sample& sample,uintptr_t object,uint64_t uid,action::GroupObjectIntent& out,std::string& e){
    out={};const auto handle=std::find_if(sample.local.handles.begin(),sample.local.handles.end(),[&](const auto& row){return row.second.object==object&&row.second.uid==uid&&group_object_key::valid(row.first);});
    if(handle==sample.local.handles.end())return q::fail(e,"Group original current canonical full UID is absent");
    const auto group=std::find_if(sample.local.state.groups.begin(),sample.local.state.groups.end(),[&](const auto& row){return row.key==handle->first;});
    if(group==sample.local.state.groups.end())return q::fail(e,"Group original canonical identity has no complete manager row");
    out.key=group->key;out.before=group->pose;for(const auto& member:group->members)out.members.push_back(member.key);return true;
}
bool source_idle(Runtime& r,action::Binding& binding,std::string& e){
    const auto controller=typed_action_runtime::controller_status();
    if(!r.ready||r.thread!=GetCurrentThreadId()||!net::session_wire::connected()||captured||r.pending||
       duplicate_edit::pending()||group_edit::pending()||selection_move::pending()||scenery_stroke::pending()||
       !controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))
        return q::fail(e,"Group action requires an idle bound multiplayer session");
    return typed_action_runtime::network_binding(net::current_snapshot_id(),binding,e);
}
}
bool pending(){return bool(captured);}
bool capture_purchase(uintptr_t behaviour,uintptr_t caller,bool& handled,std::string& e){
    handled=false;auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||caller!=r->image+0x5c2a20||r->thread!=GetCurrentThreadId()||!net::session_wire::connected())return true;
    bool group=false;if(!group_object_move_reader::target(r->memory,r->image,behaviour,group,e))return false;
    if(!group)return true;handled=true;action::Binding binding;
    if(!source_idle(*r,binding,e)){reject(e);return false;}
    move_kiss::Calls calls;if(!selection_move::calls(*r,calls,e)){reject(e);return false;}
    group_edit::Sample before,repeat;
    if(!group_edit::sample(*r,binding,{},before,e)){reject(e);return false;}
    auto bind=[&](uintptr_t object,uint64_t uid,action::GroupObjectIntent& intent,std::string& why){return intent_for(before,object,uid,intent,why);};
    group_object_move_reader::Capture first,last;
    if(!group_object_move_reader::read(r->memory,r->image,before.world.root,behaviour,bind,action::max_payload,first,e)||
       !group_edit::sample(*r,binding,{},repeat,e)||!group_edit::same_binding(before,repeat)||before.local.state!=repeat.local.state||before.money!=repeat.money||before.undo!=repeat.undo||
       !group_object_move_reader::read(r->memory,r->image,before.world.root,behaviour,bind,action::max_payload,last,e)||first!=last){
        if(e.empty())e="Group Move complete native source changed during capture";reject(e);return false;}
    const auto id=uuid(e);if(id.empty()){reject(e);return false;}
    captured=Pending{binding,id,net::current_snapshot_id(),std::move(first)};++captures;return true;
}
void tick(){
    if(!net::session_wire::connected()){retire_session();return;}
    if(!captured)return;auto value=std::move(*captured);captured.reset();std::string e;auto* r=published.load(std::memory_order_acquire);
    action::Binding binding;group_edit::Sample current;action::GroupObjectIntent actual;
    if(!r||value.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(value.snapshot,binding,e)||binding!=value.binding||
       !group_object_move_reader::cleaned(r->memory,r->image,value.source,e)||!group_edit::sample(*r,binding,{},current,e)||
       !intent_for(current,value.source.target,value.source.uid,actual,e)||actual.key!=value.source.intent.key||actual.before!=value.source.intent.before||actual.members!=value.source.intent.members){
        reject(e.empty()?"Group Move original cleanup or complete source changed before submit":e);return;}
    if(!typed_action_runtime::submit_group_object(true,value.id,value.source.intent,e)){reject(e);return;}++submissions;
}
bool submit_remove(const ui::Intent& source,std::string& e){
    auto* r=published.load(std::memory_order_acquire);action::Binding binding;group_edit::Sample current;
    if(!r||source.snapshot_uuid!=net::current_snapshot_id()||!group_object_key::valid(source.canonical_record_key)||!source_idle(*r,binding,e)||
       !group_edit::sample(*r,binding,{},current,e))return q::fail(e,e.empty()?"Group Destroy current session or canonical target is unavailable":e);
    const auto handle=current.local.handles.find(source.canonical_record_key);action::GroupObjectIntent intent;
    if(handle==current.local.handles.end()||!intent_for(current,handle->second.object,handle->second.uid,intent,e))return q::fail(e,e.empty()?"Group Destroy current complete binding is absent":e);
    if(!typed_action_runtime::submit_group_object(false,source.action_uuid,intent,e))return false;++captures;++submissions;return true;
}
}
