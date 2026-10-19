namespace world_path_runtime {
struct Operation {
    action::Binding binding;
    action::Request request;
    action::Evidence evidence;
    Sample before,after;
    uintptr_t original_graph=0;
    uintptr_t inverse_graph=0;
    unsigned purchases=0,successful_purchases=0;
    wall_private_native::Receipt wall_receipt;
    bool attempted=false,confirmed=false,retired=false;
    std::string cause;
};
std::map<std::string,Operation> operations;
Operation* active_wall=nullptr;
bool executing_wall(){return active_wall!=nullptr;}
bool wall_replay_purchase(region::Runtime& r,int32_t type,float length,const void* position,
                          std::string& error){
    if(!active_wall||!position||active_wall->purchases++)
        return fail(error,"Wall private Purchase outside one original action or repeated");
    const auto& intent=active_wall->request.world_path;
    action::PathPoint point{};
    if(!r.memory.copy(uintptr_t(position),point.data(),sizeof(point),error))return false;
    for(size_t i=0;i<3;++i)if(std::bit_cast<uint32_t>(point[i])!=intent.end[i])
        return fail(error,"Wall private Purchase release point differs from captured original");
    if(type!=intent.type||std::bit_cast<uint32_t>(length)!=intent.quoted_length)
        return fail(error,"Wall private Purchase type or quoted length differs");
    const bool paid=path.purchase(type,length,position);
    if(paid)++active_wall->successful_purchases;
    else error="Wall original Purchase returned false";
    return paid;
}
uintptr_t resolve(const mixed_path_native::Census& census,const std::map<uint64_t,std::string>& keys,
                  const std::string& key,bool path){
    if(key.empty())return 0;
    if(path){for(const auto& row:census.paths){const auto found=keys.find(row.member.uid);
        if(found!=keys.end()&&found->second==key)return row.member.object;}}
    else for(const auto& row:census.nodes){const auto found=keys.find(row.member.uid);
        if(found!=keys.end()&&found->second==key)return row.member.object;}
    return 0;
}
uintptr_t region_pointer(const mixed_path_native::Census& census,int32_t id){
    for(const auto& row:census.regions)if(row.id==id)return row.object;
    return 0;
}
using NativeString=region::NativeString;
uint32_t replay_pass(uintptr_t image,const action::WorldPathIntent& intent,
                     const mixed_path_native::Census& census,const std::map<uint64_t,std::string>& keys){
    if(intent.kind!=action::WorldPathKind::Pass||intent.preview.size()!=2)return ERROR_INVALID_PARAMETER;
    const uintptr_t start_region=region_pointer(census,intent.start_region);
    const uintptr_t end_region=region_pointer(census,intent.end_region);
    const uintptr_t start=resolve(census,keys,intent.start_node,false);
    const uintptr_t end=resolve(census,keys,intent.end_node,false);
    if(!start_region||!end_region||(!intent.start_node.empty()&&!start)||
       (!intent.end_node.empty()&&!end))return ERROR_INVALID_DATA;
    std::array<action::PathPoint,2> waypoints{};
    for(size_t i=0;i<2;++i)for(size_t j=0;j<3;++j)
        waypoints[i][j]=std::bit_cast<float>(intent.preview[i][j]);
    const action::PathPoint first{std::bit_cast<float>(intent.start[0]),
        std::bit_cast<float>(intent.start[1]),std::bit_cast<float>(intent.start[2])};
    const action::PathPoint last{std::bit_cast<float>(intent.end[0]),
        std::bit_cast<float>(intent.end[1]),std::bit_cast<float>(intent.end[2])};
    const NativeString name{intent.name.c_str(),intent.name.size(),{}};
    __try {
        auto* receiver=reinterpret_cast<unsigned char*>(reinterpret_cast<void*(*)(size_t)>(image+0xa02670)(0x1f0));
        if(!receiver)return ERROR_OUTOFMEMORY;
        reinterpret_cast<void(*)(void*)>(image+0x5c8900)(receiver);
        auto* ghost=reinterpret_cast<unsigned char*>(reinterpret_cast<void*(*)(size_t)>(image+0xa02670)(0x480));
        if(!ghost)return ERROR_OUTOFMEMORY;
        reinterpret_cast<void(*)(void*)>(image+0x43f840)(ghost);
        ghost[0x46e]=1;
        reinterpret_cast<void(*)(void*,int32_t,const void*,void*)>(image+0x438c70)(ghost,intent.type,&name,reinterpret_cast<void*>(start_region));
        ghost[0x81]=1;
        reinterpret_cast<void(*)(void*,int32_t)>(image+0x97a9d0)(ghost+0x30,0);
        reinterpret_cast<void(*)(void*,const action::PathPoint*,int32_t,bool)>(image+0x444140)(ghost,waypoints.data(),2,true);
        *reinterpret_cast<uintptr_t*>(receiver+0x188)=uintptr_t(ghost);
        *reinterpret_cast<uintptr_t*>(receiver+0x190)=start;
        *reinterpret_cast<uintptr_t*>(receiver+0x198)=end;
        *reinterpret_cast<uintptr_t*>(receiver+0x1c0)=start_region;
        *reinterpret_cast<uintptr_t*>(receiver+0x1c8)=end_region;
        std::memcpy(receiver+0x1d0,first.data(),sizeof first);
        std::memcpy(receiver+0x1dc,last.data(),sizeof last);
        receiver[0x1ea]=1;
        reinterpret_cast<void(*)(void*)>(image+0x5c9280)(receiver);
        reinterpret_cast<void(*)(void*)>(image+0x5c8740)(receiver);
        reinterpret_cast<void(*)(void*,size_t)>(image+0x1264b60)(receiver,0x1f0);
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,
                        const action::Evidence* authority,std::string& error){
    auto* r=runtime(error);
    if(!r||!capture.ready||!action::world_path_valid(request.world_path)||
       (request.action!=action::Action::PlaceWall&&request.action!=action::Action::PlacePass)){
        if(error.empty())error="Wall/Pass original native runtime or intent unavailable";
        return action::Outcome::RejectedNoEffect;}
    if(GetCurrentThreadId()!=r->thread||bool(authority)==Config::instance().is_host){
        error="Wall/Pass Mainthread or authority role differs";return action::Outcome::RejectedNoEffect;}
    uint64_t revision=0;
    if(!ui_move_runtime::contract::pending(typed_action_runtime::controller_status(),request,revision,error))
        return action::Outcome::RejectedNoEffect;
    if(operations.contains(request.action_uuid)){
        error="Wall/Pass original action already attempted; no replay";return action::Outcome::Unknown;}
    const auto& intent=request.world_path;
    Sample before;action::Ledger money;undo::UndoWitness undo_before;
    region::LocalView view;
    if(!region::read(*r,intent.start_region,0,view,error)||
       !region::bound(view,binding,request.snapshot_uuid,error)||
       !world_path_sample(*r,request.snapshot_uuid,intent.start_region,intent.end_region,before,error)||
       !region::ledger(money,error)||!undo::read_undo(r->memory,r->image,undo_before,error))
        return action::Outcome::RejectedNoEffect;
    if(undo_before.active_graph||before.wire.complete_sha256!=intent.graph_before_sha256){
        error="Wall/Pass idle Undo or complete World before witness differs";
        return action::Outcome::RejectedNoEffect;}
    if(authority&&(authority->before!=money||!authority->world_path_before||
                   *authority->world_path_before!=before.wire)){
        error="Wall/Pass Client full World/ledger before differs from Host";
        return action::Outcome::RejectedNoEffect;}
    if(intent.kind==action::WorldPathKind::Wall&&!wall_private_ready(*r,error))
        return action::Outcome::RejectedNoEffect;
    auto& operation=operations[request.action_uuid];
    operation.binding=binding;operation.request=request;operation.before=before;
    operation.evidence.before=money;operation.evidence.world_path_before=before.wire;
    operation.attempted=true;
    if(intent.kind==action::WorldPathKind::Wall){
        wall_private_native::Receipt receipt;
        active_wall=&operation;
        struct ResetWallReplay{~ResetWallReplay(){active_wall=nullptr;}} reset_wall_replay;
        const auto code=replay_wall(r->image,intent,before.mixed.native,owner.keys.keys(),receipt);
        operation.wall_receipt=receipt;
        if(code==wall_private_native::purchase_denied&&receipt.prepared&&
           receipt.purchase_attempted&&receipt.purchase_returned&&!receipt.purchase_allowed&&
           !receipt.finish_attempted&&receipt.cleanup_attempted&&receipt.cleanup_returned&&
           !receipt.cleanup_cause&&operation.purchases==1&&!operation.successful_purchases){
            Sample after;action::Ledger cash;undo::UndoWitness undo_after;
            if(!world_path_sample(*r,request.snapshot_uuid,intent.start_region,intent.end_region,after,error)||
               !region::ledger(cash,error)||!undo::read_undo(r->memory,r->image,undo_after,error)){
                operation.cause=error;return action::Outcome::Unknown;}
            if(after.wire!=before.wire||cash!=money||undo_after!=undo_before){
                error=operation.cause="Wall denied Purchase changed complete World, cash or Undo";
                return action::Outcome::Unknown;}
            error=operation.cause="Wall original Purchase returned false with confirmed no effect";
            return action::Outcome::RejectedNoEffect;
        }
        if(code||!receipt.prepared||!receipt.purchase_attempted||!receipt.purchase_returned||
           !receipt.purchase_allowed||!receipt.finish_attempted||!receipt.finish_returned||
           !receipt.cleanup_attempted||!receipt.cleanup_returned||receipt.cleanup_cause||
           operation.purchases!=1||operation.successful_purchases!=1){
            error=operation.cause="Wall private Purchase/Finish/cleanup incomplete code="+std::to_string(code)+
                " primary="+std::to_string(receipt.cause)+" cleanup="+std::to_string(receipt.cleanup_cause)+
                " purchases="+std::to_string(operation.purchases)+
                " successful="+std::to_string(operation.successful_purchases);
            return action::Outcome::Unknown;}
    }else{
        const auto seh=replay_pass(r->image,intent,before.mixed.native,owner.keys.keys());
        if(seh){error=operation.cause="Pass original private Finish SEH="+std::to_string(seh);
            return action::Outcome::Unknown;}
    }
    uintptr_t map=0;
    if(!region::at(*r,r->image,0x13df8c0,map,error)){
        operation.cause=error;return action::Outcome::Unknown;}
    mixed_path_native::Census after_census;
    if(!mixed_path_native::read_world(r->memory,r->image,map,view.world.root,after_census,error)){
        operation.cause=error;return action::Outcome::Unknown;}
    std::vector<identity::Birth> births;
    if(!world_path_births(request.action_uuid,before,after_census,births,error)||
       !world_path_sample(*r,request.snapshot_uuid,intent.start_region,intent.end_region,operation.after,error)||
       !region::ledger(operation.evidence.after,error)){
        operation.cause=error;return action::Outcome::Unknown;}
    undo::UndoWitness undo_after;undo::GraphWitness graph;
    if(!undo::read_undo(r->memory,r->image,undo_after,error)||undo_after.undo.empty()||
       !undo::read_graph(r->memory,r->image,undo_after.undo.back(),graph,error)||
       !undo::validate_undo_commit(undo_before,undo_after,graph,error)){
        operation.cause=error;return action::Outcome::Unknown;}
    operation.original_graph=undo_after.undo.back();
    operation.evidence.world_path_after=operation.after.wire;
    operation.evidence.undo_confirmed=true;
    if(!action::valid_world_path_effect(request,operation.evidence)){
        error=operation.cause="Pass complete native effect changed outside typed receipt";
        return action::Outcome::Unknown;}
    if(authority&&(authority->after!=operation.evidence.after||
                   authority->world_path_after!=operation.evidence.world_path_after)){
        error=operation.cause="Pass Client full World/ledger after differs from Host";
        return action::Outcome::Unknown;}
    operation.confirmed=true;return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& error){
    auto* r=runtime(error);const auto found=operations.find(request.action_uuid);
    if(!r||GetCurrentThreadId()!=r->thread||found==operations.end()||
       found->second.binding!=binding||found->second.request!=request||!found->second.confirmed){
        error="Wall/Pass observation lacks matching confirmed original action";return false;}
    auto& operation=found->second;
    Sample current;action::Ledger money;
    if(!world_path_sample(*r,request.snapshot_uuid,request.world_path.start_region,
                          request.world_path.end_region,current,error)||!region::ledger(money,error))return false;
    if(current.wire!=operation.after.wire||money!=operation.evidence.after){
        error="Wall/Pass complete native effect changed before ACK";return false;}
    out=operation.evidence;out.retirement_confirmed=operation.retired;return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& error){
    action::Evidence actual;if(!observe(binding,request,actual,error))return false;
    auto& operation=operations.at(request.action_uuid);
    if(operation.retired){error="Wall/Pass native action already retired";return false;}
    if(!operation.original_graph||
       !building_history_slice::remember_path(request.action_uuid,operation.original_graph,error))return false;
    operation.retired=true;return true;
}
}
