namespace district_stroke {
namespace {
struct Capture {
    uintptr_t receiver=0,graph=0;
    action::Binding binding;std::string snapshot,cause;
    action::DistrictStrokeIntent intent;
    Snapshot before;
    bool cleanup_entered=false,cleanup_destroyed=false,cleanup_returned=false;
};
struct Pending {action::Binding binding;std::string snapshot,id;action::DistrictStrokeIntent intent;};
bool installed=false,install_attempted=false,replaying=false;
std::string install_cause,last_cause;
uint64_t captures=0,submissions=0,rejections=0;
std::array<void*,4> originals{};
std::optional<Capture> capture;
std::optional<Pending> queued;
uintptr_t rejected_receiver=0;
bool code(Runtime&,std::string&);
void clear_operations();
json operation_status();
void reject(const std::string& error){++rejections;if(last_cause!=error){last_cause=error;LOG_ERROR("[DistrictStroke] {}",error);}}
bool idle(Runtime& runtime,std::string& error){
    const auto controller=typed_action_runtime::controller_status();
    return (installed&&runtime.ready&&runtime.thread==GetCurrentThreadId()&&!queued&&!runtime.pending&&!district_edit::pending()&&
        !district_text::pending()&&!district_title::pending()&&controller.value("started",false)&&!controller.value("pending",true)&&!controller.value("locked",true))||
        q::fail(error,"District stroke requires idle bound Mainthread Controller");
}
bool capture_source(Runtime& runtime,uintptr_t receiver,size_t kind,Capture& value,std::string& error){
    uintptr_t actual_region=0,selected=0,scope=0;uint8_t editing=0,color=0,water=0;int32_t mode=-1;
    value.receiver=receiver;value.snapshot=net::current_snapshot_id();
    if(!typed_action_runtime::network_binding(value.snapshot,value.binding,error)||
       !q::typed(runtime.memory,runtime.image,receiver,0x173af90,error)||
       !q::at(runtime.memory,receiver,0x1d8,actual_region,error)||!actual_region||
       !q::at(runtime.memory,actual_region,0x834,value.intent.region,error)||
       !q::at(runtime.memory,receiver,0x160,value.intent.geo,error)||!q::at(runtime.memory,receiver,0x1d0,mode,error)||
       !q::at(runtime.memory,receiver,0x760,selected,error)||!q::at(runtime.memory,receiver,0x790,value.graph,error)||!value.graph||
       !q::at(runtime.memory,receiver,0x310,scope,error)||(scope&&!q::at(runtime.memory,scope,0,editing,error))||editing>1||
       !q::at(runtime.memory,receiver,0x1e8,value.intent.press,error)||!q::at(runtime.memory,receiver,0x188,value.intent.color,error)||
       !q::at(runtime.memory,receiver,0x198,value.intent.water_color,error)||!q::at(runtime.memory,receiver,0x1c9,color,error)||
       !q::at(runtime.memory,receiver,0x1ca,water,error)||color>1||water>1)return false;
    if(kind>2||(kind==0&&mode!=0)||(kind==1&&mode!=2)||(kind==2&&!selected))return q::fail(error,"District actual original paint route/mode/selection differs");
    value.intent.kind=kind==1?Kind::Erase:Kind::Paint;value.intent.change_color=color!=0;value.intent.change_water_color=water!=0;
    if(!poly::read(runtime.memory.process,receiver+0x368,value.intent.clip,error)||!snapshot(runtime,value.binding,value.before,error,nullptr,editing))return false;
    uintptr_t bound=0;if(!resolve_region(runtime,value.before,value.intent.region,bound,error)||bound!=actual_region)
        return q::fail(error,error.empty()?"District source Region does not match actual World":error);
    if(selected){
        const auto found=std::find_if(value.before.rows.begin(),value.before.rows.end(),[&](const auto& row){return row.second.row.pointer==selected;});
        if(found==value.before.rows.end()||found->second.saved_region!=value.intent.region)return q::fail(error,"District source selected canonical owner is not current in this Region");
        value.intent.selected=found->first;
    }
    if(value.intent.kind==Kind::Erase&&!value.intent.selected.empty())return q::fail(error,"District erase source retained a selected edit target");
    return true;
}
bool empty_graph(Runtime& runtime,const Capture& value,std::string& error){
    q::Bounds children,nodes,deleting;uint8_t locked=0;
    if(!q::typed(runtime.memory,runtime.image,value.graph,0x16e32d0,error)||!q::bounds(runtime.memory,value.graph,0,children,error)||
       !q::typed(runtime.memory,runtime.image,value.graph+0x18,0x16e2dc0,error)||!q::bounds(runtime.memory,value.graph+0x18,1,nodes,error)||
       !q::typed(runtime.memory,runtime.image,value.graph+0x48,0x1748c50,error)||!q::bounds(runtime.memory,value.graph+0x48,0,deleting,error)||
       !q::at(runtime.memory,value.graph,0x10c,locked,error))return false;
    const auto expected=value.intent.selected.empty()?0:1;
    if(nodes.count!=expected||locked)return q::fail(error,"District suppressed source graph has unexpected effects or is already committed");
    if(expected){uintptr_t node=0,root=0,owner=0;district_edit_history::Stored record;
        const auto& row=value.before.rows.at(value.intent.selected);
        if(!q::at(runtime.memory,nodes.data,0,node,error)||!district_edit_history::read_node(runtime.memory,runtime.image,node,record,error)||
           !q::at(runtime.memory,row.row.pointer,0x170,root,error)||!q::at(runtime.memory,row.row.pointer,0x178,owner,error))return false;
        if(record!=district_edit_history::stored(row.row.uid,owner,root,value.before.values.at(row.key)))return q::fail(error,"District suppressed source original Edit record differs from its unchanged target");
    }
    return q::unchanged(runtime.memory,value.graph,0,children,error)&&q::unchanged(runtime.memory,value.graph+0x18,1,nodes,error)&&
        q::unchanged(runtime.memory,value.graph+0x48,0,deleting,error);
}
void paint_entry(void* receiver,const void* shape,size_t kind){
    auto* runtime=published.load(std::memory_order_acquire);const auto original=reinterpret_cast<calls::Paint>(originals[kind]);
    if(!runtime||!original){reject("District source paint original is unpublished");return;}
    if(!installed||replaying||runtime->thread!=GetCurrentThreadId()||!net::session_wire::connected()||!typed_action_runtime::controller_status().value("started",false)){original(receiver,shape);return;}
    bool forwarded=false;
    try{
        bool owned=false;std::string error;
        if(!scenery_kiss::live_cursor(runtime->memory.process,runtime->image,uintptr_t(receiver),owned,error)){reject(error);return;}
        if(!owned){forwarded=true;original(receiver,shape);return;}
        auto refused=[&](const std::string& why){rejected_receiver=uintptr_t(receiver);if(capture&&capture->cause.empty())capture->cause=why;reject(why);};
        if(rejected_receiver){refused("District source retains a rejected stroke; no replay");return;}
        if(!idle(*runtime,error)||!code(*runtime,error)){refused(error);return;}
        Capture fresh;if(!capture_source(*runtime,uintptr_t(receiver),kind,fresh,error)||!empty_graph(*runtime,fresh,error)){refused(error);return;}
        if(!capture){capture=std::move(fresh);++captures;}
        else{
            auto prior=capture->intent;prior.stamps.clear();
            if(capture->receiver!=fresh.receiver||capture->graph!=fresh.graph||capture->binding!=fresh.binding||capture->snapshot!=fresh.snapshot||
               prior!=fresh.intent||capture->before.values!=fresh.before.values||capture->before.undo!=fresh.before.undo){
                capture->cause="District original source identity/controls/values changed during stroke";reject(capture->cause);return;}
        }
        if(!capture->cause.empty()){reject(capture->cause);return;}
        action::TerrainShape stamp;
        if(!poly::read(runtime->memory.process,uintptr_t(shape),stamp,error)){capture->cause=error;reject(error);return;}
        capture->intent.stamps.push_back({uint8_t(kind),std::move(stamp)});
        if(!state::intent(capture->intent)){capture->cause="District captured stroke exceeds its semantic shape budget";reject(capture->cause);}
    }catch(const std::exception& exception){if(forwarded)throw;rejected_receiver=uintptr_t(receiver);
        const auto error=std::string("District source paint: ")+exception.what();if(capture&&capture->cause.empty())capture->cause=error;reject(error);}
}
void normal_entry(void* receiver,const void* shape){paint_entry(receiver,shape,0);}
void erase_entry(void* receiver,const void* shape){paint_entry(receiver,shape,1);}
void add_entry(void* receiver,const void* shape){paint_entry(receiver,shape,2);}
void finish_entry(void* receiver,const void* point){
    auto* runtime=published.load(std::memory_order_acquire);const auto original=reinterpret_cast<calls::Finish>(originals[3]);
    if(!runtime||!original){reject("District source Finish original is unpublished");return;}
    if(!replaying&&rejected_receiver==uintptr_t(receiver)&&!capture){reject("District rejected source has no proven empty graph for Finish; original remains unexecuted");return;}
    if(replaying||!capture||capture->receiver!=uintptr_t(receiver)||runtime->thread!=GetCurrentThreadId()){original(receiver,point);return;}
    try{
        auto& value=*capture;std::string error;
        if(value.cleanup_entered){reject("District source Finish cleanup already attempted; no replay");return;}
        if(!code(*runtime,error)||!empty_graph(*runtime,value,error)){reject(error);return;}
        uintptr_t scope=0;uint8_t editing=0;
        if(!q::at(runtime->memory,value.receiver,0x310,scope,error)||(scope&&!q::at(runtime->memory,scope,0,editing,error))){reject(error);return;}
        Snapshot before,after;
        if(!snapshot(*runtime,value.binding,before,error,nullptr,editing)||before.values!=value.before.values||before.undo!=value.before.undo||
           !runtime->memory.read(uintptr_t(point),value.intent.finish.data(),12,error)){reject(error.empty()?"District source changed before Finish":error);return;}
        value.cleanup_entered=true;
        if(!fault(calls::discard_capture(runtime->image,value.receiver,value.graph,original,point,value.cleanup_destroyed,value.cleanup_returned),"effectless source Finish",error)||
           !value.cleanup_destroyed||!value.cleanup_returned||!snapshot(*runtime,value.binding,after,error)||!same(before,after)){
            reject(error.empty()?"District suppressed source changed World/Undo/Ledger during cleanup":error);return;}
        if(!value.cause.empty()){const auto failed=value.cause;capture.reset();rejected_receiver=0;reject(failed);return;}
        if(!state::intent(value.intent)||queued){capture.reset();reject("District finished source is invalid or already queued");return;}
        const auto id=uuid(error);if(id.empty()){capture.reset();reject(error);return;}
        queued=Pending{value.binding,value.snapshot,id,std::move(value.intent)};capture.reset();
    }catch(const std::exception& exception){reject(std::string("District source Finish: ")+exception.what());}
}
}
bool pending(){return bool(capture)||bool(queued)||rejected_receiver!=0;}
void retire_session(){
    if(capture&&capture->cleanup_entered&&!capture->cleanup_returned)reject("District session retired with unresolved source cleanup; retained");
    else capture.reset();queued.reset();rejected_receiver=0;clear_operations();
}
void tick(){
    if(!net::session_wire::connected()){retire_session();return;}
    if(!queued)return;auto value=std::move(*queued);queued.reset();std::string error;action::Binding current;
    if(!typed_action_runtime::network_binding(value.snapshot,current,error)||current!=value.binding||
       !typed_action_runtime::submit_district_stroke(value.id,value.intent,error)){reject(error.empty()?"District queued source binding changed":error);return;}++submissions;
}
json status(){return {{"enabled",installed},{"pending",pending()},{"captures",captures},{"submitted",submissions},{"rejections",rejections},{"cause",last_cause},{"operations",operation_status()}};}
}
