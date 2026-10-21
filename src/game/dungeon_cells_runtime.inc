namespace dungeon_cells {
namespace {
using Drag=void(*)(void*,void*,const void*,float);using Release=void(*)(void*,void*,const void*);
using Create=uintptr_t(*)(void*,const calls::Text&,const calls::Text&,const void*);
Create original_create=nullptr;uint64_t create_generation=0;
struct Creating {dungeon_gizmo_restore::Receipts receipts;std::string cause;};
thread_local Creating* creating=nullptr;
Drag original_drag=nullptr;Release original_release=nullptr;uint64_t drag_generation=0,release_generation=0;
bool ready=false,installed=false,fault=false,replaying=false;uint64_t captures=0,submitted=0,rejected=0;
std::string cause,last_action;dungeon_cells_capture::Collector collector;
struct Pending {a::Binding binding;std::string snapshot;a::DungeonPaletteIntent intent;};
struct Operation {a::Binding binding;a::Request request;Sample before,after;a::Evidence evidence;
    BoundGraph current_graph;dungeon_cells_native::Receipt native;calls::Start start;calls::Commit commit;npc_quest_target_runtime::DungeonCellCascade zone_cascade;
    bool seed_attempted=false,confirmed=false,retired=false;std::string cause;};
std::optional<Pending> pending;std::map<std::string,Operation> operations;
void clear_history();
json history_status();
uintptr_t create_entry(void*,const calls::Text&,const calls::Text&,const void*);
void drag_entry(void*,void*,const void*,float);void release_entry(void*,void*,const void*);
bool reject(const std::string& why){++rejected;if(cause!=why){cause=why;LOG_ERROR("[DungeonCells] {}",why);}return false;}
bool route(Memory& memory,const npc_quest_list_ui_codes::Code& pin,uintptr_t entry,uintptr_t original,uint64_t generation,size_t copied,std::string& error){
    std::string bytes(pin.bytes,'\0'),hash;if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error))return false;
    if(original){MT2FW_MT2_RouteReceipt receipt{sizeof receipt};const npc_quest_list_ui_codes::Owned owner{entry,original,generation};
        if(!quest_list::read_route(state.image+pin.rva,receipt,error)||!npc_quest_list_ui_codes::receipt(receipt,state.image+pin.rva,pin,owner,error)||
           std::memcmp(bytes.data(),receipt.current,32))return fail(error,error.empty()?"Dungeon cell exact owned route bytes differ":error);
        std::array<unsigned char,25> trampoline{};uintptr_t resume=0;constexpr std::array<unsigned char,6> continuation{0xff,0x25,0,0,0,0};
        if(copied>11||!memory.copy(original,trampoline.data(),copied+14,error))return false;
        std::memcpy(&resume,trampoline.data()+copied+6,8);
        if(!std::equal(pin.prefix.begin(),pin.prefix.begin()+copied,trampoline.begin())||
           !std::equal(continuation.begin(),continuation.end(),trampoline.begin()+copied)||resume!=state.image+pin.rva+copied)
            return fail(error,"Dungeon cell exact original instruction boundary or trampoline continuation differs");
        std::memcpy(bytes.data(),receipt.pristine,32);if(!quest_list::verify_code(state.image+pin.rva,bytes.data(),bytes.size(),error))return false;
    }
    if(!quest_bridge::sha256(bytes,hash,error)||hash!=pin.sha)return fail(error,error.empty()?"Dungeon cell original UI body differs":error);return true;
}
bool code(Memory& memory,std::string& error){
    if(!route(memory,dungeon_cells_codes::drag,uintptr_t(&drag_entry),uintptr_t(original_drag),drag_generation,6,error)||
       !route(memory,dungeon_cells_codes::release,uintptr_t(&release_entry),uintptr_t(original_release),release_generation,7,error)||
       !route(memory,dungeon_cells_codes::create,uintptr_t(&create_entry),uintptr_t(original_create),create_generation,11,error))return false;
    for(const auto& pin:dungeon_cells_codes::helpers){std::string bytes(pin.bytes,'\0');
        if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error))return false;
        if(!hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error))return fail(error,"Dungeon cell original explicit-owner helper differs RVA="+std::to_string(pin.rva)+" original="+error);}
    for(const auto& pin:dungeon_cells_codes::graph_helpers){std::string bytes(pin.bytes,'\0'),hash;
        if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error)||!quest_bridge::sha256(bytes,hash,error))return false;
        if(hash!=pin.sha)return fail(error,"Dungeon original history helper differs RVA="+std::to_string(pin.rva));}
    return true;
}
uintptr_t create_entry(void* receiver,const calls::Text& type,const calls::Text& variant,const void* transform){
    // Observe the actual RAX result. The original call always runs once, including
    // calls outside our exact pending history scope.
    const auto object=original_create(receiver,type,variant,transform);
    if(!creating)return object;
    try{uintptr_t node=0;std::string error;
        if(!creating->receipts.associate(uintptr_t(receiver),uintptr_t(&type),uintptr_t(&variant),uintptr_t(transform),node,error)||
           !creating->receipts.record(node,object,error)){
            if(creating->cause.empty())creating->cause=error;
        }
    }catch(const std::exception& exception){if(creating->cause.empty())creating->cause=std::string("Dungeon original Gizmo return observation: ")+exception.what();}
    catch(...){if(creating->cause.empty())creating->cause="Dungeon original Gizmo return observation non-standard exception";}
    return object;
}
bool idle(a::Binding& binding,const std::string& snapshot,std::string& error){
    const auto controller=typed_action_runtime::controller_status();
    if(!ready||fault||pending||replaying||GetCurrentThreadId()!=state.thread||dungeon_palette::pending_ui()||
       !controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))
        return fail(error,"Dungeon cell capture requires idle bound Mainthread Controller");
    return typed_action_runtime::network_binding(snapshot,binding,error);
}
bool capture_hit(Memory& memory,void* receiver,void* cursor,dungeon_cells_capture::Stroke& out,int32_t& cell,bool& hit,std::string& error){
    out={};cell=-1;hit=false;uintptr_t registered=0,ap=0,collision=0,marker=0;uint32_t flags=0;int32_t mode=0;
    const auto seh=dungeon_palette::find_tile(registered);
    if(seh||registered!=uintptr_t(receiver)||!field(memory,registered,0,ap,error)||ap!=state.image+0x173ea20||
       !field(memory,registered,0x148,mode,error)||!field(memory,registered,0x348,marker,error)||marker)
        return fail(error,error.empty()?"Dungeon cell capture registered Tile, original mode or unedited receiver differs SEH="+std::to_string(seh):error);
    if(mode==0)return true;if(mode!=1&&mode!=2)return fail(error,"Dungeon cell normal Tile mode is neither Paint nor Close");
    if(!field(memory,uintptr_t(cursor),0x230,collision,error)||!collision||!field(memory,collision,0x1ec,flags,error))return false;
    if(flags!=0x02000000)return true;uintptr_t dungeon=0;
    if(!original_result(calls::hit(state.image,collision,dungeon,cell),"Dungeon collision",error))return false;
    if(!dungeon||cell<0)return fail(error,"Dungeon cell original collision has no Dungeon or cell");
    std::string scene;if(!dungeon_scene_runtime::cursor_scene(scene,error)||!scene.ends_with("/template"))return fail(error,error.empty()?"Dungeon cell normal UI must own the current template scene":error);
    out.receiver=registered;out.dungeon=dungeon;out.key=scene.substr(0,scene.size()-9);out.snapshot=net::current_snapshot_id();
    out.intent.mode=mode==1?a::DungeonCellMode::Paint:a::DungeonCellMode::Close;
    if(mode==1){uintptr_t window=0,row=0;if(!field(memory,registered,0x2a8,window,error)||!window||
           !original_result(calls::selected(state.image,window,registered+0x2d0,row),"selected Theme row",error)||!row||
           !npc_quest_records::read_string(memory,row,0,"Dungeon cell selected Theme",out.intent.theme,error))return false;}
    a::Binding binding;if(!idle(binding,out.snapshot,error))return false;
    a::DungeonPaletteIntent intent;intent.key=out.key;dungeon_palette::Sample sample;
    if(!dungeon_palette::sample(memory,binding,out.snapshot,intent,sample,error)||sample.dungeon.dungeon.dungeon.local.dungeon!=dungeon||
       size_t(cell)>=sample.value.cells.size())return fail(error,error.empty()?"Dungeon cell collision owner/index differs from bound canonical Dungeon":error);
    hit=true;return true;
}
void drag_entry(void* receiver,void* cursor,const void* position,float distance)try{
    if(!original_drag){fault=true;reject("Dungeon cell original Drag is unavailable");return;}
    if(!net::session_wire::connected()){original_drag(receiver,cursor,position,distance);return;}
    if(replaying){fault=true;reject("Dungeon cell explicit replay unexpectedly reentered native UI Drag");return;}
    Memory memory;std::string error;dungeon_cells_capture::Stroke stroke;int32_t cell=-1;bool hit=false;
    if(!capture_hit(memory,receiver,cursor,stroke,cell,hit,error)){reject(error);return;}
    if(!original_result(calls::brush(state.image,receiver,cursor,position,distance),"UI-only brush",error)){fault=true;reject(error);return;}
    if(hit&&!collector.append(stroke,cell,error)){fault=true;reject(error);}
}catch(const std::exception& exception){fault=true;reject(std::string("Dungeon cell Drag capture boundary: ")+exception.what());}
catch(...){fault=true;reject("Dungeon cell Drag capture non-standard exception");}
void release_entry(void* receiver,void* cursor,const void* position)try{
    if(!original_release){fault=true;reject("Dungeon cell original Release is unavailable");return;}
    if(!net::session_wire::connected()){original_release(receiver,cursor,position);return;}
    std::string error;std::optional<dungeon_cells_capture::Stroke> stroke;
    if(!collector.take(uintptr_t(receiver),stroke,error)){fault=true;reject(error);return;}
    // No original edit marker was created by capture. Release only resets the
    // normal mode and updates the local brush through the original UI method.
    Memory memory;uintptr_t marker=0;if(!field(memory,uintptr_t(receiver),0x348,marker,error)||marker){fault=true;reject(error.empty()?"Dungeon cell original Release has an unexpected live edit marker":error);return;}
    original_release(receiver,cursor,position);if(!stroke)return;
    a::Binding binding;if(!idle(binding,stroke->snapshot,error)||!code(memory,error)){fault=true;reject(error);return;}
    a::DungeonPaletteIntent intent;intent.key=stroke->key;intent.edit=a::DungeonPaletteEdit::Cells;intent.stroke=stroke->intent;
    dungeon_palette::Sample before;if(!dungeon_palette::sample(memory,binding,stroke->snapshot,intent,before,error)||
       before.dungeon.dungeon.dungeon.local.dungeon!=stroke->dungeon||!theme_colors(memory,before,intent.stroke,intent.stroke.theme_packed,error)||
       !static_digest(before,intent.stroke,intent.before_sha256,error)){fault=true;reject(error.empty()?"Dungeon cell released stroke owner differs":error);return;}
    pending=Pending{binding,stroke->snapshot,std::move(intent)};++captures;
}catch(const std::exception& exception){fault=true;reject(std::string("Dungeon cell Release capture boundary: ")+exception.what());}
catch(...){fault=true;reject("Dungeon cell Release capture non-standard exception");}
}
bool pending_ui(){return pending.has_value()||replaying;}
bool enable(std::string& error){
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(error,"Dungeon cells require bound Mainthread");
    if(ready){if(fault||pending_ui()||collector.active())return fail(error,"Dungeon cells retain an unresolved capture");
        for(const auto& [id,op]:operations)if(!op.retired)return fail(error,"Dungeon cells retain an unretired operation="+id);
        Memory memory;return code(memory,error);}
    if(installed)return fail(error,"Dungeon cell earlier installation failed: "+cause);installed=true;Memory memory;if(!code(memory,error))return false;
    auto status=MH_CreateHook(reinterpret_cast<void*>(state.image+dungeon_cells_codes::drag.rva),reinterpret_cast<void*>(&drag_entry),reinterpret_cast<void**>(&original_drag));
    if(status!=MH_OK||!original_drag)return fail(error,"Dungeon cell Drag prepare failed status="+std::to_string(status)+" cause="+mt2mp::hooks::framework::process_backend().error());
    status=MH_CreateHook(reinterpret_cast<void*>(state.image+dungeon_cells_codes::release.rva),reinterpret_cast<void*>(&release_entry),reinterpret_cast<void**>(&original_release));
    if(status!=MH_OK||!original_release)return fail(error,"Dungeon cell Release prepare failed status="+std::to_string(status)+" cause="+mt2mp::hooks::framework::process_backend().error());
    status=MH_CreateHook(reinterpret_cast<void*>(state.image+dungeon_cells_codes::create.rva),reinterpret_cast<void*>(&create_entry),reinterpret_cast<void**>(&original_create));
    if(status!=MH_OK||!original_create)return fail(error,"Dungeon original Gizmo return observer prepare failed status="+std::to_string(status)+" cause="+mt2mp::hooks::framework::process_backend().error());
    for(const auto rva:{dungeon_cells_codes::drag.rva,dungeon_cells_codes::release.rva,dungeon_cells_codes::create.rva}){status=MH_EnableHook(reinterpret_cast<void*>(state.image+rva));if(status!=MH_OK)return fail(error,"Dungeon cell UI enable failed RVA="+std::to_string(rva)+" status="+std::to_string(status));}
    MT2FW_MT2_RouteReceipt drag{sizeof drag},release{sizeof release},create{sizeof create};
    if(!quest_list::read_route(state.image+dungeon_cells_codes::drag.rva,drag,error)||!quest_list::read_route(state.image+dungeon_cells_codes::release.rva,release,error)||!quest_list::read_route(state.image+dungeon_cells_codes::create.rva,create,error))return false;
    drag_generation=drag.generation;release_generation=release.generation;create_generation=create.generation;if(!code(memory,error))return false;ready=true;return true;
}
void tick(){if(!pending)return;const auto saved=*pending;pending.reset();std::string error;a::Binding binding;
    if(!ready||fault||saved.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(saved.snapshot,binding,error)||binding!=saved.binding){fault=true;reject(error.empty()?"Dungeon cell captured world changed":error);return;}
    const auto id=action_runtime::uuid(error);if(id.empty()||!typed_action_runtime::submit_dungeon_palette(id,saved.intent,error)){fault=true;reject(error);return;}++submitted;last_action=id;
}
bool retire_lobby(std::string& error){
    if(!ready){error.clear();return true;}native::WorldStatus world;
    if(GetCurrentThreadId()!=state.thread||!native::read_world_status(world,error)||world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0||replaying)
        return fail(error,error.empty()?"Dungeon cell lobby retirement requires actual idle Title":error);
    collector.clear();pending.reset();operations.clear();clear_history();fault=false;cause.clear();last_action.clear();error.clear();return true;
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    if(!ready||fault||GetCurrentThreadId()!=state.thread||request.action!=a::Action::SetDungeonPalette||request.dungeon_palette.edit!=a::DungeonPaletteEdit::Cells){error="Dungeon cell runtime unavailable";return a::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="Dungeon cell original operation already entered; replay forbidden";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();if(!controller.value("pending",false)||controller.value("locked",true)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Dungeon cell execution requires exact pending Controller";return a::Outcome::RejectedNoEffect;}
    Memory memory;Sample before;std::string prior;std::array<uint32_t,4> packed{};const auto& intent=request.dungeon_palette;
    if(!a::valid_cell_stroke(intent.stroke)||!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,intent,before,error)||
       !static_digest(before.palette,intent.stroke,prior,error)||!theme_colors(memory,before.palette,intent.stroke,packed,error))return a::Outcome::RejectedNoEffect;
    if(prior!=intent.before_sha256||packed!=intent.stroke.theme_packed){error="Dungeon cell captured prior layout/palette or selected Theme resource differs";return a::Outcome::RejectedNoEffect;}
    if(!a::dungeon_palette_wire_fits(request,before.value,before.palette.dungeon.dungeon.dungeon.money,error))return a::Outcome::RejectedNoEffect;
    if(authority){auto portable=before.value;portable.protected_state.dungeon.global_random=authority->dungeon_palette_before.protected_state.dungeon.global_random;
        if(portable!=authority->dungeon_palette_before||before.palette.dungeon.dungeon.dungeon.money!=authority->before){error="Dungeon cell Client complete original scenes/Quests/visitors/layout differs from Host";return a::Outcome::RejectedNoEffect;}}
    std::vector<dungeon_cells_native::Cell> cells;for(const auto index:intent.stroke.cells)cells.push_back({index,intent.stroke.mode==a::DungeonCellMode::Close?dungeon_cells_native::Edit::Close:dungeon_cells_native::Edit::Open,intent.stroke.theme});
    Port preview{memory,before,intent.stroke};if(!preview.ready(cells,error))return a::Outcome::RejectedNoEffect;
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;
    const auto unknown=[&](const std::string& why){replaying=false;error=op.cause=why.empty()?"Dungeon cell original execution failed without cause":why;return a::Outcome::Unknown;};
    const auto desired_rng=authority?authority->dungeon_palette_before.protected_state.dungeon.global_random:before.value.protected_state.dungeon.global_random;
    op.seed_attempted=true;if(!original_result(action_runtime::seed(state.image,desired_rng),"Host RNG adoption",error))return unknown(error);
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.before,error)||op.before.value.protected_state.dungeon.global_random!=desired_rng)return unknown(error.empty()?"Dungeon cell Host RNG readback differs":error);
    op.evidence.before=op.before.palette.dungeon.dungeon.dungeon.money;op.evidence.dungeon_palette_before=op.before.value;
    if(!npc_quest_target_runtime::prepare_dungeon_cells(binding,request,op.zone_cascade,error))return unknown(error);
    Port port{memory,op.before,intent.stroke};replaying=true;const bool applied=dungeon_cells_native::apply(port,cells,op.native,error);replaying=false;
    op.start=port.started;op.commit=port.committed;
    if(!applied||!op.native.committed||!op.native.finished||!op.native.changes||op.native.active)return unknown(error.empty()?"Dungeon cell native graph lacks committed once-only receipt":error);
    if(intent.stroke.mode==a::DungeonCellMode::Close){dungeon_scene_identity_runtime::Census cascade;
        if(!dungeon_scene_identity_runtime::confirm_cell_cascade(binding,request,op.before.scenes,cascade,error)||!retire_scenery(op.before,cascade,error)||!retire_monsters(memory,op.before,error))return unknown(error);}
    if(!npc_quest_target_runtime::confirm_dungeon_cells(binding,request,op.zone_cascade,error))return unknown(error);
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.after,error,&op.before.quests)||
       !dungeon_palette::same_local(op.before.palette,op.after.palette)||!a::dungeon_palette_transition(intent,op.before.value,op.after.value)||
       op.before.palette.dungeon.dungeon.dungeon.money!=op.after.palette.dungeon.dungeon.dungeon.money)return unknown(error.empty()?"Dungeon cell original complete after state, cascade, layout or ledger differs":error);
    undo_probe::GraphWitness graph;if(!undo_probe::read_graph(memory.process,state.image,op.native.graph,graph,error)||
       !undo_probe::validate_undo_commit(op.before.palette.dungeon.dungeon.dungeon.undo,op.after.palette.dungeon.dungeon.dungeon.undo,graph,error)||
       !graph.locked||graph.pending_delete||graph.operations<2)return unknown(error.empty()?"Dungeon cell original paint/layout/cascade graph is not locked and complete":error);
    if(!graph_capture(memory,op.before,op.after,op.native.graph,op.current_graph,error)||
       !building_history_slice::remember_dungeon_tiles(request.action_uuid,op.native.graph,error))return unknown(error);
    op.evidence.after=op.after.palette.dungeon.dungeon.dungeon.money;op.evidence.dungeon_palette_after=op.after.value;op.evidence.undo_confirmed=true;
    if(authority){auto expected=*authority;expected.retirement_confirmed=false;if(op.evidence!=expected)return unknown("Dungeon cell Client actual complete original result differs from Host");}
    op.confirmed=true;return a::Outcome::Confirmed;
}catch(const std::exception& exception){replaying=false;error=std::string("Dungeon cell perform boundary: ")+exception.what();return a::Outcome::Unknown;}
catch(...){replaying=false;error="Dungeon cell perform non-standard exception";return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& error){
    const auto found=operations.find(request.action_uuid);if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.native.committed)return fail(error,"Dungeon cell observation lacks exact confirmed native graph receipt");
    const auto& op=found->second;Memory memory;Sample now;if(!sample(memory,binding,request.snapshot_uuid,request.dungeon_palette,now,error,&op.after.quests)||
       !dungeon_palette::same_local(op.after.palette,now.palette)||now.value!=op.after.value)return fail(error,error.empty()?"Dungeon cell actual complete result changed before ACK":error);
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence ignored;if(!observe(binding,request,ignored,error))return false;
    auto& op=operations.at(request.action_uuid);if(op.retired)return fail(error,"Dungeon cell receipt is already retired");op.retired=true;return true;}
json status(){json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"native_entered",op.native.entered},{"native_calls",op.native.calls},{"changes",op.native.changes},
    {"queued_drains",op.native.drains},{"graph_committed",op.native.committed},{"confirmed",op.confirmed},{"retired",op.retired},{"cause",op.cause},{"cleanup_cause",op.native.cleanup_cause}};
    return {{"enabled",ready},{"fault",fault},{"collecting",collector.active()},{"pending_ui",pending_ui()},{"captures",captures},{"submitted",submitted},{"rejected",rejected},{"last_action",last_action},{"cause",cause},{"operations",rows},{"history",history_status()}};
}
}
