namespace zone_edit_runtime {
namespace {
using Command=bool(*)(void*,void*);
struct Capture {
    uintptr_t brush=0,cursor=0,graph=0;
    a::Binding binding;std::string snapshot,id,cause,ui_token;
    a::ZoneEditIntent intent;Sample before;
    bool cleanup_entered=false,cleanup_destroyed=false,cleanup_returned=false;
};
struct Pending {a::Binding binding;std::string snapshot,id,ui_token;a::ZoneEditIntent intent;};
bool ready=false,install_attempted=false,replaying=false,capture_fault=false;
Command original_command=nullptr;
zone_edit_calls::Press original_right=nullptr;
std::optional<Capture> captured;
std::optional<Pending> queued;
std::string capture_cause,install_cause,last_action;
uint64_t capture_count=0,submit_count=0,reject_count=0;
bool code(Memory&,std::string&);
void clear_operations();
json operation_status();
void reject(const std::string& error){
    ++reject_count;if(capture_cause!=error){capture_cause=error;LOG_ERROR("[ZoneEdit] {}",error);}
}
bool idle(std::string& error){
    const auto controller=typed_action_runtime::controller_status();
    return (ready&&za::state.ready&&za::state.thread==GetCurrentThreadId()&&!capture_fault&&!captured&&!queued&&
        !za::pending_ui()&&controller.value("started",false)&&!controller.value("pending",true)&&!controller.value("locked",true))||
        fail(error,"Zone edit requires idle bound Mainthread Controller/capture");
}
bool tool(Memory& memory,uintptr_t expected,uintptr_t cursor,std::string& error){
    uintptr_t player=0,inventory=0,actual_cursor=0;const auto image=za::state.image;
    npc_census::InventoryHeader before,after;
    if(!field(memory,image,0x13df9f0,player,error)||!q::typed(memory,image,player,0x17873b0,error)||
       !field(memory,player,0x1d0,inventory,error)||!q::typed(memory,image,inventory,0x16df0f0,error)||
       !field(memory,image,0x13df9d0,actual_cursor,error)||actual_cursor!=cursor||
       !q::typed(memory,image,cursor,0x1786dd0,error)||!npc_census::read_inventory_header(memory,inventory,before,error))return false;
    size_t matches=0;std::set<uintptr_t> items;
    for(int32_t index=0;index<before.count;++index){
        uintptr_t item=0,type=0,weak=0,target=0,owned=0,ap=0,brush=0;
        if(!field(memory,before.items,size_t(index)*8,item,error))return false;
        if(!item)continue;
        if(!items.insert(item).second||!q::typed(memory,image,item,0x17437f0,error)||
           !field(memory,item,0x68,type,error)||!field(memory,item,0x70,weak,error)||
           !field(memory,weak,0,target,error)||!field(memory,type,0x40,owned,error)||target!=type+0x38||owned!=weak||
           !field(memory,type,0,ap,error))return fail(error,error.empty()?"Zone edit inventory reciprocal weak owner differs":error);
        if(ap!=image+0x173b7b0)continue;
        if(!field(memory,type,0x160,brush,error))return false;
        if(brush==expected)++matches;
    }
    return npc_census::read_inventory_header(memory,inventory,after,error)&&
        ((before==after&&matches==1&&q::typed(memory,image,expected,0x172fa80,error))||
         fail(error,error.empty()?"Zone edit actual inventory brush membership differs":error));
}
bool initial_graph(Memory& memory,const Capture& capture,std::string& error){
    q::Bounds children,nodes,deleting;uint8_t locked=0;uintptr_t current=0;
    if(!field(memory,capture.brush,0x740,current,error)||current!=capture.graph||!current||
       !q::typed(memory,za::state.image,current,0x16e32d0,error)||!q::bounds(memory,current,0,children,error)||
       !q::typed(memory,za::state.image,current+0x18,0x16e2dc0,error)||!q::bounds(memory,current+0x18,1,nodes,error)||
       !q::typed(memory,za::state.image,current+0x48,0x1748c50,error)||!q::bounds(memory,current+0x48,0,deleting,error)||
       !field(memory,current,0x10c,locked,error))return false;
    const size_t expected=capture.intent.selected.empty()?0:1;
    if(size_t(nodes.count)!=expected||locked)return fail(error,"Zone suppressed source graph contains an unexpected effect");
    if(expected){
        const auto row=std::find_if(capture.before.rows.begin(),capture.before.rows.end(),[&](const auto& value){return value.owner.key==capture.intent.selected;});
        uintptr_t node=0;zone_edit_stored::Stored stored;
        if(row==capture.before.rows.end()||!field(memory,nodes.data,0,node,error)||
           !q::typed(memory,za::state.image,node,0x16e4980,error)||
           !zone_edit_stored::read(memory,za::state.image,node,stored,error))return false;
        if(stored.uid!=row->owner.uid||stored.root!=row->owner.root||stored.region!=row->owner.region||
           stored.type!=row->content.type||stored.density!=row->content.density||stored.monster_type!=row->content.monster_type||
           stored.level!=row->content.level||stored.shape!=row->shape)
            return fail(error,"Zone suppressed initial Edit differs from exact current authored target");
    }
    return true;
}
bool press_capture(void* brush,void* cursor,const float* point,bool add,std::string& error){
    if(!idle(error))return false;
    Memory memory;if(!code(memory,error)||!tool(memory,uintptr_t(brush),uintptr_t(cursor),error))return false;
    Capture capture;capture.brush=uintptr_t(brush);capture.cursor=uintptr_t(cursor);
    capture.snapshot=net::current_snapshot_id();capture.id=action_runtime::uuid(error);capture.ui_token=za::state.ui_token;
    capture.intent.kind=Kind::Stroke;capture.intent.add=add;
    uintptr_t region=0,window=0,graph=0;int32_t painting=0;std::vector<Option> options;json selected;
    if(capture.id.empty()||!typed_action_runtime::network_binding(capture.snapshot,capture.binding,error)||
       !field(memory,capture.brush,0x170,region,error)||!field(memory,region,0x834,capture.intent.region,error)||
       !field(memory,capture.brush,0x178,capture.intent.zone_type,error)||!field(memory,capture.brush,0x17c,capture.intent.level,error)||
       !field(memory,capture.brush,0x6fc,painting,error)||painting||!field(memory,capture.brush,0x740,graph,error)||graph||
       !memory.copy(uintptr_t(point),capture.intent.press.data(),12,error)||
       !d::options(memory,za::state.image,capture.brush,window,options,selected,error)||
       !selected.is_array()||selected.size()!=2||selected[0]!=1)return fail(error,error.empty()?"Zone press actual brush/selection/idle state differs":error);
    capture.intent.character_type=selected[1].get<int32_t>();
    if(capture.intent.zone_type<0||capture.intent.zone_type>3||
       !sample(memory,capture.binding,capture.snapshot,capture.intent.region,capture.id,capture.before,error)||
       capture.before.edit.world.region!=region)return fail(error,error.empty()?"Zone press complete current Region differs":error);
    captured=std::move(capture);
    const auto exception=add?za::press_call(brush,cursor,point):[&]()noexcept->uint32_t{
        __try{original_right(brush,cursor,point);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}();
    if(!za::native_ok(exception,"edit capture Press",error)){capture_fault=true;captured->cause=error;return false;}
    auto& value=*captured;uintptr_t owner=0;
    if(!field(memory,value.brush,0x710,owner,error)||!field(memory,value.brush,0x740,value.graph,error)){
        capture_fault=true;value.cause=error;return false;}
    if(owner){
        const auto found=value.before.objects.find(owner);
        if(!add||found==value.before.objects.end()){
            capture_fault=true;value.cause="Zone press selected target is outside complete current ownership";return fail(error,value.cause);}
        value.intent.selected=found->second;
    }
    Sample after;
    if(!initial_graph(memory,value,error)||!sample(memory,value.binding,value.snapshot,value.intent.region,value.id,after,error)||
       !same(value.before,after)){capture_fault=true;value.cause=error.empty()?"Zone suppressed Press changed complete world state":error;error=value.cause;return false;}
    return true;
}
void right_entry(void* brush,void* cursor,const float* point)try{
    if(!net::session_wire::connected()||replaying){original_right(brush,cursor,point);return;}
    std::string error;if(!press_capture(brush,cursor,point,false,error))reject(error);
}catch(const std::exception& exception){capture_fault=true;reject(std::string("Zone right Press boundary: ")+exception.what());}
catch(...){capture_fault=true;reject("Zone right Press non-standard boundary exception");}
bool capture_command(uintptr_t window,const std::string& command,std::string& error){
    if(!idle(error))return false;
    Memory memory;Pending pending;uintptr_t zone=0,proxy=0,target=0,region=0;
    pending.snapshot=net::current_snapshot_id();pending.id=action_runtime::uuid(error);
    if(pending.id.empty()||!code(memory,error)||!typed_action_runtime::network_binding(pending.snapshot,pending.binding,error)||
       !q::typed(memory,za::state.image,window,0x1716b10,error)||!field(memory,window,0x510,zone,error)||
       !field(memory,window,0x518,proxy,error)||!proxy||!field(memory,proxy,0,target,error)||target!=zone+0x20||
       !field(memory,zone,0x178,region,error)||!field(memory,region,0x834,pending.intent.region,error))return false;
    Sample before;if(!sample(memory,pending.binding,pending.snapshot,pending.intent.region,pending.id,before,error))return false;
    const auto selected=before.objects.find(zone);
    if(selected==before.objects.end())return fail(error,"Zone UI selected weak target is not a current canonical owner");
    pending.intent.selected=selected->second;
    if(command=="ZoneApply"){
        pending.intent.kind=Kind::Apply;pending.intent.zone_type=1;
        zone_edit_calls::Choices controls;std::string text;int32_t slot=-1;
        if(!za::native_ok(zone_edit_calls::choices(za::state.image,window,controls),"UI choices",error)||
           !field(memory,controls.character,0x2c,slot,error)||slot!=1||
           !field(memory,controls.character,0x30,pending.intent.character_type,error)||!name(memory,controls.level_text,text,error))return false;
        const auto result=std::from_chars(text.data(),text.data()+text.size(),pending.intent.level);
        if(result.ec!=std::errc{}||result.ptr!=text.data()+text.size())return fail(error,"Zone UI level is not its exact numeric control value");
        pending.intent.density=controls.density;
    }else pending.intent.kind=command=="ZoneReset"?Kind::Reset:command=="ZoneRespawn"?Kind::Respawn:Kind::Delete;
    if(!a::valid_zone_edit_intent(pending.intent))return fail(error,"Zone UI actual command values exceed typed input domain");
    queued=std::move(pending);++capture_count;return true;
}
bool command_entry(void* receiver,void* record)try{
    if(!net::session_wire::connected()||replaying)return original_command(receiver,record);
    uintptr_t token=0;const auto exception=[&]()noexcept->uint32_t{
        __try{token=reinterpret_cast<uintptr_t(*)(const void*)>(za::state.image+0x96d880)(record);return 0;}
        __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}();
    Memory memory;std::string text,error;
    if(!za::native_ok(exception,"UI command token",error)||!name(memory,token,text,error)){reject(error);return false;}
    if(text!="ZoneApply"&&text!="ZoneReset"&&text!="ZoneRespawn"&&text!="Destroy")return original_command(receiver,record);
    if(!capture_command(uintptr_t(receiver),text,error))reject(error);
    return true;
}catch(const std::exception& exception){capture_fault=true;reject(std::string("Zone UI command boundary: ")+exception.what());return false;}
catch(...){capture_fault=true;reject("Zone UI command non-standard boundary exception");return false;}
}
bool capture_press(void* brush,void* cursor,const float* point,bool& handled){
    handled=net::session_wire::connected()&&!replaying;if(!handled)return true;
    try{std::string error;if(!press_capture(brush,cursor,point,true,error)){reject(error);return false;}return true;}
    catch(const std::exception& exception){capture_fault=true;reject(std::string("Zone left Press boundary: ")+exception.what());return false;}
    catch(...){capture_fault=true;reject("Zone left Press non-standard boundary exception");return false;}
}
bool capture_paint(void* brush,const void* shape,bool& handled){
    handled=net::session_wire::connected()&&!replaying;if(!handled)return true;
    try{
        if(!captured||captured->brush!=uintptr_t(brush)){reject("Zone AddPaint lacks its owned suppressed press");return false;}
        auto& value=*captured;if(!value.cause.empty())return false;
        Memory memory;std::string error;a::ZoneStamp stamp;
        if(GetCurrentThreadId()!=za::state.thread||!field(memory,value.brush,0x5cc,stamp.position,error)||
           !poly::read(memory,uintptr_t(shape),stamp.shape,error)){value.cause=error.empty()?"Zone AddPaint thread/shape differs":error;reject(value.cause);return false;}
        value.intent.stamps.push_back(std::move(stamp));
        if(!a::valid_zone_edit_intent(value.intent)){value.cause="Zone captured geometry exceeds typed input domain";reject(value.cause);return false;}
        return true;
    }catch(const std::exception& exception){capture_fault=true;reject(std::string("Zone AddPaint boundary: ")+exception.what());return false;}
    catch(...){capture_fault=true;reject("Zone AddPaint non-standard boundary exception");return false;}
}
bool capture_finished(void* brush,const void* shape,bool& handled){
    handled=net::session_wire::connected()&&!replaying;if(!handled)return true;
    try{
        if(!captured||captured->brush!=uintptr_t(brush)){reject("Zone Finish lacks its owned suppressed press");return false;}
        auto& value=*captured;Memory memory;std::string error;
        if(capture_fault||value.cleanup_entered||GetCurrentThreadId()!=za::state.thread||!initial_graph(memory,value,error)){
            reject(error.empty()?"Zone Finish graph ownership/lifetime differs; no cleanup replay":error);return false;}
        Sample before,after;
        if(!sample(memory,value.binding,value.snapshot,value.intent.region,value.id,before,error)){reject(error);return false;}
        value.cleanup_entered=true;
        if(!za::native_ok(zone_edit_calls::discard_capture(za::state.image,value.brush,value.graph,za::state.finished,shape,
            value.cleanup_destroyed,value.cleanup_returned),"suppressed graph cleanup",error)||
           !sample(memory,value.binding,value.snapshot,value.intent.region,value.id,after,error)||!same(before,after)){
            capture_fault=true;value.cause=error.empty()?"Zone suppressed Finish changed complete state":error;reject(value.cause);return false;}
        auto complete=std::move(value);captured.reset();
        if(!complete.cause.empty()||!a::valid_zone_edit_intent(complete.intent)){
            reject(complete.cause.empty()?"Zone completed stroke has no admitted original stamps":complete.cause);return false;}
        queued=Pending{complete.binding,complete.snapshot,complete.id,complete.ui_token,std::move(complete.intent)};
        if(!complete.ui_token.empty())za::state.ui_jobs.at(complete.ui_token)["captured"]=true;
        ++capture_count;return true;
    }catch(const std::exception& exception){capture_fault=true;reject(std::string("Zone Finish boundary: ")+exception.what());return false;}
    catch(...){capture_fault=true;reject("Zone Finish non-standard boundary exception");return false;}
}
bool pending(){return bool(captured)||bool(queued);}
void tick(){
    if(!queued)return;
    auto value=std::move(*queued);queued.reset();std::string error;a::Binding binding;
    if(!ready||GetCurrentThreadId()!=za::state.thread||!typed_action_runtime::network_binding(value.snapshot,binding,error)||
       binding!=value.binding||!typed_action_runtime::submit_zone_edit(value.id,value.intent,error)){
        const auto why=error.empty()?"Zone captured request lost its exact bound Controller":error;reject(why);
        if(!value.ui_token.empty()){auto& job=za::state.ui_jobs.at(value.ui_token);job["terminal"]=true;job["confirmed"]=false;job["cause"]=why;job["outcome"]=int(a::Outcome::RejectedNoEffect);}
        return;
    }
    last_action=value.id;++submit_count;
    if(!value.ui_token.empty()){auto& job=za::state.ui_jobs.at(value.ui_token);job["submitted"]=true;job["action_uuid"]=value.id;}
}
bool retire_session(std::string& error){
    if(captured||queued||capture_fault){error="Zone session retired with unresolved input lifetime";reject(error);return false;}
    clear_operations();capture_cause.clear();last_action.clear();return true;
}
json status(){return {{"enabled",ready},{"pending",pending()},{"capture_active",bool(captured)},{"capture_fault",capture_fault},{"captured",capture_count},
    {"submitted",submit_count},{"rejected",reject_count},{"cause",capture_cause},{"last_action",last_action},{"operations",operation_status()}};}
}
