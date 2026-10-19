namespace world_path_runtime {
using NativePress=void(*)(void*,void*,const void*);
struct Pending {
    action::WorldPathIntent intent;
    action::Binding binding;
    std::string snapshot;
};
struct Capture {
    bool attempted=false,ready=false;
    NativePress wall=nullptr,pass=nullptr;
    uintptr_t local_wall=0;
    std::optional<Sample> before;
    std::optional<Pending> captured,pending;
    uint64_t accepted=0,rejected_count=0;
    std::string cause,last_action;
};
Capture capture;
bool capturing_wall(){return capture.local_wall!=0;}
bool pending(){return capture.local_wall||capture.captured||capture.pending;}
void rejected(const std::string& cause){
    ++capture.rejected_count;
    if(capture.cause!=cause){capture.cause=cause;LOG_ERROR("[UIWorldPath] Rejected cause={}",cause);}
}
bool idle(region::Runtime& r,std::string& error){
    const auto controller=typed_action_runtime::status();
    if(GetCurrentThreadId()!=r.thread||!net::session_wire::connected()||
       !controller.value("started",false)||controller.value("locked",true)||
       controller.value("pending",true)||pending()||
       path.pending||path.captured||path.delete_pending||path.replay||path.ui_receiver)
        return fail(error,"Wall/Pass UI requires an idle bound Mainthread Controller");
    return true;
}
bool wall_purchase(region::Runtime& r,int32_t type,float length,const void* position,
                   uintptr_t caller,std::string& error){
    if(!capture.local_wall||!capture.before)return fail(error,"Wall purchase lacks pending local capture");
    if(caller!=r.image+0x5ec3c0&&caller!=r.image+0x5ec63b)
        return fail(error,"Wall purchase call site differs from original Press");
    if(capture.captured)return fail(error,"Wall purchase captured twice");
    uintptr_t cursor=0;
    if(!region::at(r,r.image,0x13df9d0,cursor,error))return false;
    wall_pass_cursor::WallSource source;
    if(!wall_pass_cursor::wall(r.memory,r.image,capture.local_wall,cursor,uintptr_t(position),
                              capture.before->mixed.native,owner.keys.keys(),source,error))return false;
    if(type!=source.intent.type||std::bit_cast<uint32_t>(length)!=source.intent.quoted_length||
       !action::world_path_valid(portable(source.intent,capture.before->wire.complete_sha256)))
        return fail(error,"Wall original Purchase arguments or complete semantic intent differ");
    region::LocalView view;
    if(!region::identity(r,source.intent.region,source.region,view,error))return false;
    capture.captured=Pending{portable(source.intent,capture.before->wire.complete_sha256),
        {view.world.origin.world_epoch,view.world.origin.generation},net::current_snapshot_id()};
    return true;
}
uint32_t call_press(NativePress original,void* receiver,void* cursor,const void* point){
    __try{original(receiver,cursor,point);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t cleanup(uintptr_t image,bool wall,void* receiver){
    __try{
        reinterpret_cast<void(*)(void*)>(image+(wall?0x5e8d00:0x5c8a60))(receiver);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool unchanged(region::Runtime& r,const action::Ledger& money,const undo::UndoWitness& undo_before,
               const Pending& pending,std::string& error){
    action::Ledger after;undo::UndoWitness undo_after;Sample sample;
    if(!region::ledger(after,error)||!undo::read_undo(r.memory,r.image,undo_after,error)||
       !world_path_sample(r,pending.snapshot,pending.intent.start_region,pending.intent.end_region,sample,error))return false;
    if(after!=money||undo_after!=undo_before||!capture.before||sample.wire!=capture.before->wire)
        return fail(error,"Wall/Pass local capture changed native World, cash or Undo");
    return true;
}
void wall_press_entry(void* receiver,void* cursor,const void* point){
    if(!capture.ready||!net::session_wire::connected()){
        capture.wall(receiver,cursor,point);return;
    }
    std::string error;auto* r=runtime(error);
    if(!r||!idle(*r,error)){rejected(error);return;}
    int32_t id=-1;uintptr_t region_pointer=0;
    if(!region::at(*r,uintptr_t(receiver),0x288,region_pointer,error)||
       !region_pointer||!region::at(*r,region_pointer,0x834,id,error)){
        rejected(error.empty()?"Wall current Region absent":error);return;}
    action::Ledger money;undo::UndoWitness undo_before;Sample before;
    if(!region::ledger(money,error)||!undo::read_undo(r->memory,r->image,undo_before,error)||
       undo_before.active_graph||
       !world_path_sample(*r,net::current_snapshot_id(),id,id,before,error)){
        rejected(error.empty()?"Wall UI requires idle original Undo":error);return;}
    capture.local_wall=uintptr_t(receiver);capture.before=std::move(before);
    const auto seh=call_press(capture.wall,receiver,cursor,point);
    capture.local_wall=0;
    if(seh){capture.before.reset();capture.captured.reset();rejected("Wall original Press SEH="+std::to_string(seh));return;}
    if(!capture.captured){capture.before.reset();return;}
    const auto pending=*capture.captured;
    const auto cleanup_seh=cleanup(r->image,true,receiver);
    if(cleanup_seh||!unchanged(*r,money,undo_before,pending,error)){
        rejected(cleanup_seh?"Wall original Deactivate SEH="+std::to_string(cleanup_seh):error);
        return;
    }
    capture.pending=pending;capture.captured.reset();capture.before.reset();++capture.accepted;
}
void pass_press_entry(void* receiver,void* cursor,const void* point){
    if(!capture.ready||!net::session_wire::connected()){
        capture.pass(receiver,cursor,point);return;
    }
    std::string error;auto* r=runtime(error);
    if(!r||!idle(*r,error)){rejected(error);return;}
    uintptr_t start=0,end=0;int32_t start_id=-1,end_id=-1;
    if(!region::at(*r,uintptr_t(receiver),0x1c0,start,error)||
       !region::at(*r,uintptr_t(receiver),0x1c8,end,error)||!start||!end||
       !region::at(*r,start,0x834,start_id,error)||!region::at(*r,end,0x834,end_id,error)){
        rejected(error.empty()?"Pass current Regions absent":error);return;}
    action::Ledger money;undo::UndoWitness undo_before;Sample before;
    if(!region::ledger(money,error)||!undo::read_undo(r->memory,r->image,undo_before,error)||
       undo_before.active_graph||
       !world_path_sample(*r,net::current_snapshot_id(),start_id,end_id,before,error)){
        rejected(error.empty()?"Pass UI requires idle original Undo":error);return;}
    wall_pass_cursor::PassSource source;
    if(!wall_pass_cursor::pass(r->memory,r->image,uintptr_t(receiver),uintptr_t(cursor),
                               before.mixed.native,owner.keys.keys(),source,error)){
        rejected(error);return;}
    auto intent=portable(source.intent,before.wire.complete_sha256);
    if(!action::world_path_valid(intent)){rejected("Pass complete semantic intent invalid");return;}
    region::LocalView view;
    if(!region::identity(*r,start_id,start,view,error)){rejected(error);return;}
    Pending pending{std::move(intent),{view.world.origin.world_epoch,view.world.origin.generation},net::current_snapshot_id()};
    capture.before=std::move(before);
    const auto cleanup_seh=cleanup(r->image,false,receiver);
    if(cleanup_seh||!unchanged(*r,money,undo_before,pending,error)){
        rejected(cleanup_seh?"Pass original Deactivate SEH="+std::to_string(cleanup_seh):error);return;}
    capture.pending=std::move(pending);capture.before.reset();++capture.accepted;
    (void)point;
}
bool enable(region::Runtime& r,std::string& error){
    if(capture.attempted)return fail(error,"Wall/Pass hook installation already attempted");
    capture.attempted=true;
    struct Guard{uintptr_t rva;std::array<uint8_t,16> prefix;NativePress* original;void* entry;};
    const std::array<Guard,2> guards{{
        {0x5ec320,{0x57,0x56,0x53,0x48,0x81,0xec,0xa0,0,0,0,0x48,0x8b,0x81,0x68,0x02,0},&capture.wall,reinterpret_cast<void*>(&wall_press_entry)},
        {0x5c97e0,{0x80,0xb9,0xea,0x01,0,0,0,0x74,0x17,0x48,0x83,0xb9,0x88,0x01,0,0},&capture.pass,reinterpret_cast<void*>(&pass_press_entry)}}};
    for(const auto& guard:guards){std::array<uint8_t,16> actual{};
        if(!r.memory.copy(r.image+guard.rva,actual.data(),actual.size(),error)||actual!=guard.prefix)
            return fail(error,"Wall/Pass original Press bytes differ RVA="+std::to_string(guard.rva));
    }
    for(const auto& guard:guards){
        const auto status=MH_CreateHook(reinterpret_cast<void*>(r.image+guard.rva),guard.entry,
                                        reinterpret_cast<void**>(guard.original));
        if(status!=MH_OK||!*guard.original)
            return fail(error,"Wall/Pass MH_CreateHook RVA="+std::to_string(guard.rva)+" status="+std::to_string(status));
    }
    for(const auto& guard:guards){const auto status=MH_EnableHook(reinterpret_cast<void*>(r.image+guard.rva));
        if(status!=MH_OK)return fail(error,"Wall/Pass MH_EnableHook RVA="+std::to_string(guard.rva)+" status="+std::to_string(status));}
    capture.ready=true;return true;
}
void tick(){
    if(!capture.pending)return;
    auto pending=std::move(*capture.pending);capture.pending.reset();
    std::string error;auto* r=runtime(error);
    if(!r||!capture.ready||!net::session_wire::connected()){
        rejected(error.empty()?"Wall/Pass submit lacks bound native runtime":error);return;}
    region::LocalView view;
    if(!region::identity(*r,pending.intent.start_region,0,view,error)||
       !region::bound(view,pending.binding,pending.snapshot,error)){
        rejected(error);return;}
    const auto id=region::uuid(error);
    if(id.empty()||!typed_action_runtime::submit_world_path(id,pending.intent,error)){
        rejected(error.empty()?"Wall/Pass typed Controller rejected captured action":error);return;}
    capture.last_action=id;
}
}
