namespace selection_move {
namespace {
struct Pending {action::Binding binding;std::string id;selection_move_reader::Selection source;action::SelectionMoveIntent intent;};
struct Operation {action::Binding binding;action::Request request;action::Evidence evidence;
    std::vector<move_kiss::LocalBinding> before;selection_move_native::Receipt native;
    bool confirmed=false,retired=false;std::string cause;};
std::optional<Pending> captured;std::map<std::string,Operation> operations;
uint64_t captures=0,submitted=0,rejections=0;std::string cause;
void reject(const std::string& e){++rejections;if(cause!=e){cause=e;LOG_ERROR("[SelectionMove] {}",e);}}
struct Box {uint32_t present=1;std::array<float,3> low{},high{};};
uint32_t average(uintptr_t image,uintptr_t shard,const Box& box,float& value){
    __try{value=reinterpret_cast<float(*)(void*,const Box*)>(image+0x508450)(reinterpret_cast<void*>(shard),&box);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool destination_height(Runtime& r,move_kiss::Intent& row,uintptr_t blueprint,std::string& e){
    uintptr_t ap=0,floating=0;if(!r.memory.read(row.target,&ap,sizeof ap,e)||!r.memory.read(ap+0x100,&floating,sizeof floating,e))return false;
    if(ap==r.image+0x16c1390){
        if(floating!=r.image+0xcf30c0){e="Selection Move Scenery floating vslot differs";return false;}
        return true;
    }
    if(ap!=r.image+0x16d93c0||floating!=r.image+0xe3f4a0){e="Selection Move normal Building floating vslot differs";return false;}
    host::WorldState current;uintptr_t manager=0,shard=0;std::array<float,6> bounds{};
    if(!world(r,current,e)||!r.memory.read(r.image+0x13df440,&manager,sizeof manager,e)||!manager||
       !r.memory.read(manager+0x60,&shard,sizeof shard,e)||shard!=current.root||
       !r.memory.read(blueprint+0x4c,bounds.data(),sizeof bounds,e)){
        if(e.empty())e="Selection Move original current Shard/Blueprint bounds unavailable";return false;}
    Box box;const std::array<float,3> position{row.after.x,row.after.y,row.after.z};
    for(size_t i=0;i<3;++i){box.low[i]=position[i]+bounds[i];box.high[i]=position[i]+bounds[i+3];
        if(!std::isfinite(box.low[i])||!std::isfinite(box.high[i])||box.low[i]>box.high[i]){e="Selection Move original Blueprint bounds invalid";return false;}}
    const auto fault=average(r.image,shard,box,row.after.y);
    if(fault||!std::isfinite(row.after.y)){e="Selection Move original terrain height failed SEH="+std::to_string(fault);return false;}
    return true;
}
bool read(Runtime& r,uintptr_t behaviour,const move_kiss::Origin& origin,selection_move_reader::Selection& out,std::string& e){
    auto height=[&](move_kiss::Intent& row,uintptr_t blueprint,std::string& error){return destination_height(r,row,blueprint,error);};
    return selection_move_reader::read(r.memory,r.services,r.image,behaviour,origin,height,out,e);
}
bool calls(Runtime& r,move_kiss::Calls& out,std::string& e){
    if(!move_kiss::resolve_calls(r.memory,r.image,"bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6",out,e))return false;
    struct Guard{uintptr_t rva;const char* bytes;size_t size;};
    const Guard guards[]{{0x508450,"\x53\x48\x83\xec\x30\x8b\x41\x48\x48\x89\xd3\x83\xf8\x01\x74\x78",16},
        {0xe3f4a0,"\x31\xc0\xc3",3},{0xcf30c0,"\xb8\x01\x00\x00\x00\xc3",6}};
    for(const auto& guard:guards){unsigned char bytes[16]{};
        if(!r.memory.read(r.image+guard.rva,bytes,guard.size,e))return false;
        if(std::memcmp(bytes,guard.bytes,guard.size)){e="Selection Move original pose helper differs RVA="+std::to_string(guard.rva);return false;}}
    return true;
}
bool refresh_targets(const action::Request& request,const std::vector<move_kiss::LocalBinding>& before,std::string& e){
    if(before.size()!=request.selection_move.rows.size()){e="Selection Move registry refresh target count differs";return false;}
    // Every resolver checks the complete registry. Refresh the whole confirmed
    // move set before resolving any single moved target.
    for(size_t i=0;i<before.size();++i)
        if(!remove_diagnostics::refresh_moved_building(request.snapshot_uuid,request.selection_move.rows[i].key,before[i].local_uid,e))return false;
    return true;
}
bool current(Runtime& r,const action::Binding& binding,const action::Request& request,const std::vector<move_kiss::LocalBinding>& before,
             uintptr_t graph,action::SelectionMoveState& out,action::Ledger& money,std::string& e){
    out={};if(before.size()!=request.selection_move.rows.size()){e="Selection Move retained local target count differs";return false;}
    for(size_t i=0;i<before.size();++i){ExecuteServices io(r);io.request=request;
        if(!remove_diagnostics::resolve_record_target(request.snapshot_uuid,request.selection_move.rows[i].key,io.target,e))return false;
        if(io.target.object!=before[i].target||io.target.uid!=before[i].local_uid||io.target.world_epoch!=binding.native_epoch||io.target.world_generation!=binding.native_generation){e="Selection Move current local full UID/owner differs index="+std::to_string(i);return false;}
        native::BuildingState actual;host::WorldState world_value;
        if(!current_prop(r,io.target,world_value,actual,e)||!ledger(money,e))return false;
        const move_kiss::Origin origin{request.snapshot_uuid,world_value.origin.world_epoch,world_value.origin.generation,uintptr_t(world_value.origin.world)};
        if(origin!=before[i].origin||money!=before[i].ledger){e="Selection Move current World/Ledger changed index="+std::to_string(i);return false;}
        const auto state_value=world_state::building_state(actual);
        if(state_value!=request.selection_move.rows[i].after){e="Selection Move current full pose differs index="+std::to_string(i);return false;}
        out.rows.push_back(state_value);
    }
    witness::UndoWitness undo;witness::GraphWitness graph_value;
    if(!witness::read_undo(r.memory.process,r.image,undo,e)||undo.active_graph||
       !witness::read_graph(r.memory.process,r.image,graph,graph_value,e)||graph_value.operations!=int32_t(before.size())){
        if(e.empty())e="Selection Move exact shared inverse graph size/idle differs";return false;}
    return true;
}
}
bool pending(){return bool(captured);}
bool capture_purchase(uintptr_t behaviour,uintptr_t caller,bool& handled,std::string& e){
    handled=false;auto* r=published.load(std::memory_order_acquire);
    if(!r||!r->ready||caller!=r->image+0x5c2a20||GetCurrentThreadId()!=r->thread||!net::session_wire::connected())return true;
    int32_t count=0;if(!r->memory.read(behaviour+0x468,&count,sizeof count,e))return false;
    if(count<=1)return true;handled=true;
    const auto controller=typed_action_runtime::controller_status();action::Binding binding;move_kiss::Origin origin;
    if(captured||r->pending||prop_text::pending()||boss_phase::pending()||service_membership::pending()||
       !controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||
       !r->services.active(origin,e)||!typed_action_runtime::network_binding(origin.snapshot_uuid,binding,e)){
        if(e.empty())e="Selection Move capture requires idle bound Controller";reject(e);return false;}
    move_kiss::Calls checked;if(!calls(*r,checked,e)){reject(e);return false;}
    selection_move_reader::Selection a,b;move_kiss::Origin after;
    if(!read(*r,behaviour,origin,a,e)||!r->services.active(after,e)||after!=origin||!read(*r,behaviour,after,b,e)||a!=b){
        if(e.empty())e="Selection Move complete original capture changed";reject(e);return false;}
    action::SelectionMoveIntent intent;for(const auto& row:a.rows)intent.rows.push_back({row.object_key,row.before,row.after});
    action::Ledger money;if(!action::valid_selection_move(intent)||!ledger(money,e)){if(e.empty())e="Selection Move unchanged/invalid complete target set";reject(e);return false;}
    const auto id=uuid(e);if(id.empty()){reject(e);return false;}action::Request request;request.action=action::Action::MoveSelection;request.action_uuid=id;
    request.snapshot_uuid=origin.snapshot_uuid;request.peer=1;request.sequence=1;request.canonical_record_key="selection:"+id;request.selection_move=intent;
    if(!action::selection_move_wire_fits(request,money,e)){reject(e);return false;}
    captured=Pending{binding,id,std::move(a),std::move(intent)};++captures;return true;
}
void tick(){
    if(!captured)return;auto value=std::move(*captured);captured.reset();std::string e;auto* r=published.load(std::memory_order_acquire);
    action::Binding binding;move_kiss::Origin origin;
    if(!r||!selection_move_reader::cleaned(r->memory,r->image,value.source,e)||!r->services.active(origin,e)||origin!=value.source.origin||
       !typed_action_runtime::network_binding(origin.snapshot_uuid,binding,e)||binding!=value.binding){reject(e.empty()?"Selection Move source cleanup/World changed":e);return;}
    for(const auto& row:value.source.rows){remove_diagnostics::LocalTarget target;native::BuildingState actual;host::WorldState current_world;
        if(!remove_diagnostics::resolve_record_target(origin.snapshot_uuid,row.object_key,target,e)||target.object!=row.target||target.uid!=row.local_uid||
           !current_prop(*r,target,current_world,actual,e)||world_state::building_state(actual)!=row.before){reject(e.empty()?"Selection Move source original target changed before submit":e);return;}}
    if(!typed_action_runtime::submit_selection_move(value.id,value.intent,e)){reject(e);return;}++submitted;
}
action::Outcome perform(const action::Binding& binding,const action::Request& request,const action::Evidence* authority,std::string& e){
    auto* r=published.load(std::memory_order_acquire);uint64_t revision=0;
    if(!r||!r->ready||GetCurrentThreadId()!=r->thread||request.action!=action::Action::MoveSelection||!action::valid_selection_move(request.selection_move)||
       !contract::pending(typed_action_runtime::controller_status(),request,revision,e)||bool(authority)==Config::instance().is_host){
        if(e.empty())e="Selection Move exact pending action/role unavailable";return action::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){e="Selection Move already attempted; no replay";return action::Outcome::Unknown;}
    if(authority&&!action::valid_selection_move_effect(request,*authority)){e="Selection Move Host complete evidence invalid";return action::Outcome::RejectedNoEffect;}
    move_kiss::Calls checked;if(!calls(*r,checked,e))return action::Outcome::RejectedNoEffect;
    std::vector<std::unique_ptr<ExecuteServices>> readers;std::vector<move_kiss::LocalBinding> before;
    std::vector<selection_move_native::Target> native_targets;std::set<uint64_t> uids;std::set<uintptr_t> objects;
    action::Ledger money;witness::UndoWitness initial_undo;
    for(const auto& row:request.selection_move.rows){auto io=std::make_unique<ExecuteServices>(*r);auto local_request=request;local_request.canonical_record_key=row.key;local_request.building=row.after;
        move_kiss::LocalBinding local;if(!io->prepare(local_request,local,e))return action::Outcome::RejectedNoEffect;
        if(local.actual!=row.before||local.origin.world_epoch!=binding.native_epoch||local.origin.generation!=binding.native_generation||
           !uids.insert(local.local_uid).second||!objects.insert(local.target).second){e="Selection Move current complete before/full UID differs key="+row.key;return action::Outcome::RejectedNoEffect;}
        if(readers.empty()){money=local.ledger;initial_undo=io->undo_before;}
        else if(local.ledger!=money||io->undo_before!=initial_undo||local.origin!=before.front().origin){e="Selection Move complete before World/Undo/Ledger changed";return action::Outcome::RejectedNoEffect;}
        selection_move_native::Target target;target.object=local.target;if(!move_kiss::transform(row.after,target.pose,e))return action::Outcome::RejectedNoEffect;
        before.push_back(local);native_targets.push_back(target);readers.push_back(std::move(io));
    }
    if(authority&&authority->before!=money){e="Selection Move Client Ledger differs from Host";return action::Outcome::RejectedNoEffect;}
    if(!action::selection_move_wire_fits(request,money,e))return action::Outcome::RejectedNoEffect;
    auto& op=operations.emplace(request.action_uuid,Operation{binding,request}).first->second;op.before=before;op.evidence.before=money;
    op.evidence.selection_move_before=action::SelectionMoveState{};for(const auto& row:before)op.evidence.selection_move_before->rows.push_back(row.actual);
    auto unknown=[&](const std::string& why){e=op.cause=why;return action::Outcome::Unknown;};
    if(!selection_move_native::execute(checked,native_targets,op.native,e))return unknown(e);
    for(size_t i=0;i<readers.size();++i){move_kiss::Readback back;
        if(!readers[i]->readback(before[i],op.native.scope.graph,back,e))return unknown(e);
        if(back.current.origin!=before[i].origin||back.current.target!=before[i].target||back.current.local_uid!=before[i].local_uid||
           back.current.actual!=request.selection_move.rows[i].after||back.current.ledger!=money||!back.manager_registered||!back.inverse_registered||!back.selection_present||!back.edit_closed)
            return unknown("Selection Move original full binding/pose/inverse/selection/ledger differs index="+std::to_string(i));}
    if(!refresh_targets(request,before,e))return unknown(e);
    action::SelectionMoveState after;if(!current(*r,binding,request,before,op.native.scope.graph,after,op.evidence.after,e))return unknown(e);
    op.evidence.selection_move_after=std::move(after);op.evidence.undo_confirmed=op.evidence.selection_confirmed=true;
    if(!action::valid_selection_move_effect(request,op.evidence)||(authority&&(op.evidence.selection_move_after!=authority->selection_move_after||op.evidence.after!=authority->after)))return unknown("Selection Move actual complete result differs from Host/request");
    op.confirmed=true;return action::Outcome::Confirmed;
}
bool observe(const action::Binding& binding,const action::Request& request,action::Evidence& out,std::string& e){
    auto* r=published.load(std::memory_order_acquire);const auto found=operations.find(request.action_uuid);
    if(!r||found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed){e="Selection Move observation lacks exact confirmed operation";return false;}
    const auto& op=found->second;action::SelectionMoveState value;action::Ledger money;
    if(!current(*r,binding,request,op.before,op.native.scope.graph,value,money,e))return false;
    if(value!=op.evidence.selection_move_after||money!=op.evidence.after){e="Selection Move current complete result changed before ACK";return false;}
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const action::Binding& binding,const action::Request& request,std::string& e){
    action::Evidence value;if(!observe(binding,request,value,e))return false;auto& op=operations.at(request.action_uuid);
    if(op.retired){e="Selection Move already retired";return false;}
    if(!building_history_slice::remember_selection(request.action_uuid,op.native.scope.graph,e))return false;
    op.retired=true;return true;
}
#include "selection_history_runtime.inc"
json status(){json rows=json::object();for(const auto& [id,op]:operations){json targets=json::array();for(size_t i=0;i<op.before.size();++i)
        targets.push_back({{"key",op.request.selection_move.rows[i].key},{"local_uid",op.before[i].local_uid}});
        rows[id]={{"group_key",op.request.canonical_record_key},{"targets",std::move(targets)},{"attempted",op.native.scope.started},{"confirmed",op.confirmed},
            {"retired",op.retired},{"original_moves",op.native.moved},{"graph",op.native.scope.graph},{"cause",op.cause}};}
    return {{"pending",pending()},{"captures",captures},{"submitted",submitted},{"rejected",rejections},{"cause",cause},{"operations",std::move(rows)}};
}
}
