namespace dungeon_input {
namespace {
namespace pins=dungeon_input_action_codes;
using Purchase=bool(*)(const dungeon_input_calls::Text&,const void*);
using MonsterRelease=void(*)(void*,void*,const void*);
using PaintStamp=void(*)(void*,const void*);using PaintFinish=void(*)(void*,const void*);
Purchase original_purchase=nullptr;MonsterRelease original_monster_release=nullptr;
PaintStamp original_paint_stamp=nullptr;PaintFinish original_paint_finish=nullptr;
std::array<uint64_t,4> generations{};
bool ready=false,installed=false,fault=false,replaying=false;
uint64_t captures=0,submitted=0,rejected=0;std::string cause,last_action;
struct Pending {a::Binding binding;std::string snapshot;a::DungeonInputIntent intent;std::optional<dungeon_gizmo_move_read::Capture> control_cursor;};
struct Stroke {uintptr_t receiver=0,canvas=0,fill=0;Pending pending;};
std::optional<Pending> pending;std::optional<Stroke> stroke;
bool finish_capture(Memory&,Pending&,std::string&);
bool purchase_entry(const dungeon_input_calls::Text&,const void*);
void monster_release_entry(void*,void*,const void*);void paint_stamp_entry(void*,const void*);void paint_finish_entry(void*,const void*);
bool reject(const std::string& why){++rejected;if(cause!=why){cause=why;LOG_ERROR("[DungeonInput] {}",why);}return false;}
bool code(Memory& memory,std::string& error){
    if(!dungeon_cells::route(memory,pins::purchase,uintptr_t(&purchase_entry),uintptr_t(original_purchase),generations[0],pins::purchase_copied,error)||
       !dungeon_cells::route(memory,pins::monster_release,uintptr_t(&monster_release_entry),uintptr_t(original_monster_release),generations[1],pins::monster_release_copied,error)||
       !dungeon_cells::route(memory,pins::paint_stamp,uintptr_t(&paint_stamp_entry),uintptr_t(original_paint_stamp),generations[2],pins::paint_stamp_copied,error)||
       !dungeon_cells::route(memory,pins::paint_finish,uintptr_t(&paint_finish_entry),uintptr_t(original_paint_finish),generations[3],pins::paint_finish_copied,error))return false;
    for(const auto& pin:pins::helpers){std::vector<unsigned char> bytes(pin.bytes);
        if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error))return false;
        if(!hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error))
            return fail(error,"Dungeon input original helper differs RVA="+std::to_string(pin.rva)+" original="+error);
    }
    return true;
}
bool idle(a::Binding& binding,const std::string& snapshot,std::string& error){
    const auto controller=typed_action_runtime::controller_status();
    if(!ready||fault||pending||replaying||building_status::pending_ui()||quest_list::pending_ui()||
       character_rename::pending_ui()||ability_edit::pending_ui()||costume_edit::pending_ui()||
       dungeon_config::pending_ui()||dungeon_light::pending_ui()||dungeon_sound::pending_ui()||
       dungeon_palette::pending_ui()||dungeon_gizmo::pending_ui()||GetCurrentThreadId()!=state.thread||
       !controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true))
        return fail(error,"Dungeon input capture requires idle bound Mainthread Controller");
    return typed_action_runtime::network_binding(snapshot,binding,error);
}
bool capture_scene(a::DungeonInputIntent& intent,std::string& error){
    if(!dungeon_scene_runtime::cursor_scene(intent.scene,error))return false;
    const auto end=intent.scene.ends_with("/template")?intent.scene.size()-9:intent.scene.rfind("/instance/");
    if(end==std::string::npos)return fail(error,"Dungeon input active Cursor is not inside a canonical Dungeon scene");
    intent.key=intent.scene.substr(0,end);return true;
}
}
}
#include "dungeon_input_sample.inc"
#include "dungeon_input_port.inc"
#include "dungeon_input_birth.inc"
#include "dungeon_gizmo_control_port.inc"
#include "dungeon_gizmo_control_capture.inc"
namespace dungeon_input {
namespace {
struct Operation {
    a::Binding binding;a::Request request;Sample before,after;a::Evidence evidence;
    dungeon_input_native::Receipt native;dungeon_gizmo_control_native::Receipt control;dungeon_cells::BoundGraph current_graph;
    npc_quest_target_runtime::DungeonCellCascade zones;
    bool seed_attempted=false,unknown=false,confirmed=false,retired=false;std::string cause;
};
std::map<std::string,Operation> operations;
bool finish_capture(Memory& memory,Pending& value,std::string& error){
    Sample before;if(!sample(memory,value.binding,value.snapshot,value.intent,before,error)||!selected(before,value.intent)||
       !digest(before,value.intent.before_sha256,error))return false;
    return a::valid_dungeon_input(value.intent)||fail(error,"Dungeon input original UI produced an invalid fixed tool intent");
}
}
}
#include "dungeon_input_capture_runtime.inc"
#include "dungeon_input_history.inc"
namespace dungeon_input {
bool pending_ui(){return pending.has_value()||stroke.has_value()||replaying;}
bool owns(const std::string& action){return operations.contains(action);}
bool enable(std::string& error){
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(error,"Dungeon input requires bound Character Mainthread");
    if(ready){if(fault||pending_ui()||stroke)return fail(error,"Dungeon input retains unresolved capture or original outcome");
        for(const auto& [id,op]:operations)if(!op.retired)return fail(error,"Dungeon input retains unretired original action="+id);
        Memory memory;return code(memory,error);}
    if(installed)return fail(error,"Dungeon input previous installation failed: "+cause);installed=true;
    Memory memory;if(!code(memory,error))return false;
    struct Hook {uintptr_t rva,entry;void** original;};
    const std::array<Hook,4> hooks{{
        {pins::purchase.rva,uintptr_t(&purchase_entry),reinterpret_cast<void**>(&original_purchase)},
        {pins::monster_release.rva,uintptr_t(&monster_release_entry),reinterpret_cast<void**>(&original_monster_release)},
        {pins::paint_stamp.rva,uintptr_t(&paint_stamp_entry),reinterpret_cast<void**>(&original_paint_stamp)},
        {pins::paint_finish.rva,uintptr_t(&paint_finish_entry),reinterpret_cast<void**>(&original_paint_finish)}}};
    for(const auto& hook:hooks){const auto result=MH_CreateHook(reinterpret_cast<void*>(state.image+hook.rva),reinterpret_cast<void*>(hook.entry),hook.original);
        if(result!=MH_OK||!*hook.original)return fail(error,"Dungeon input original hook prepare failed RVA="+std::to_string(hook.rva)+" status="+std::to_string(result)+" original="+mt2mp::hooks::framework::process_backend().error());}
    if(!mt2mp::hooks::enable_prepared_routes(state.image,hooks,"Dungeon input hooks",error))return false;
    for(size_t i=0;i<hooks.size();++i){MT2FW_MT2_RouteReceipt receipt{sizeof receipt};
        if(!quest_list::read_route(state.image+hooks[i].rva,receipt,error))return false;generations[i]=receipt.generation;}
    if(!code(memory,error))return false;ready=true;return true;
}
void tick(){
    if(!pending)return;const auto saved=*pending;pending.reset();a::Binding binding;std::string error;
    if(!ready||fault||saved.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(saved.snapshot,binding,error)||binding!=saved.binding){
        fault=true;reject(error.empty()?"Dungeon input captured NativeOpen/session binding changed":error);return;}
    if(saved.control_cursor){Memory memory;dungeon_cells::MonsterMemory adapter{memory};
        if(!dungeon_gizmo_move_read::cleaned(adapter,state.image,*saved.control_cursor,error)){fault=true;reject(error);return;}}
    const auto id=action_runtime::uuid(error);if(!id.empty()&&saved.intent.controls)control_local_sources.insert(id);
    if(id.empty()||!typed_action_runtime::submit_dungeon_input(id,saved.intent,error)){
        fault=true;reject(error);return;}
    ++submitted;last_action=id;
}
bool retire_lobby(std::string& error){
    if(!ready){error.clear();return true;}native::WorldStatus world;
    if(GetCurrentThreadId()!=state.thread||!native::read_world_status(world,error))return false;
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0||replaying)
        return fail(error,"Dungeon input lobby retirement requires actual idle Title and no active original call");
    stroke.reset();pending.reset();operations.clear();clear_history();control_local_sources.clear();fault=false;cause.clear();last_action.clear();error.clear();return true;
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    uint64_t revision=0;
    if(!ready||fault||GetCurrentThreadId()!=state.thread||request.action!=a::Action::DungeonInput||request.snapshot_uuid!=net::current_snapshot_id()||
       bool(authority)==Config::instance().is_host||!ui_move_runtime::contract::pending(typed_action_runtime::controller_status(),request,revision,error)){
        if(error.empty())error="Dungeon input requires exact pending Mainthread world and authority";return a::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="Dungeon input original operation already entered; replay forbidden";return a::Outcome::Unknown;}
    Memory memory;Sample before;std::string prior;
    if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,request.dungeon_input,before,error)||
       !digest(before,prior,error)||prior!=request.dungeon_input.before_sha256){
        if(error.empty())error="Dungeon input captured complete native records changed before Host ordering";return a::Outcome::RejectedNoEffect;}
    if(authority){auto portable=before.value;portable.protected_state.dungeon.global_random=authority->dungeon_input_before.protected_state.dungeon.global_random;
        if(portable!=authority->dungeon_input_before||before.source.palette.dungeon.dungeon.dungeon.money!=authority->before){
            error="Dungeon input Client complete native scene/Quest/layout or full ledger differs from Host";return a::Outcome::RejectedNoEffect;}}
    std::vector<std::string> name_bounds;
    if(request.dungeon_input.controls&&request.dungeon_input.controls->command==ControlCommand::Duplicate){
        std::string separator;const auto read=[&](uintptr_t address,void* output,size_t copy_size,std::string& read_error){return memory.copy(address,output,copy_size,read_error);};
        if(!dungeon_gizmo_name_bound::separator(read,state.image,separator,error))return a::Outcome::RejectedNoEffect;
        for(const auto& row:request.dungeon_input.controls->rows)name_bounds.push_back(dungeon_gizmo_name_bound::widest(row.before.type,separator));
    }
    if(!a::dungeon_input_wire_fits(request,before.value,before.source.palette.dungeon.dungeon.dungeon.money,error,nullptr,nullptr,name_bounds.empty()?nullptr:&name_bounds))return a::Outcome::RejectedNoEffect;
    const bool controls=request.dungeon_input.tool==a::DungeonInputTool::Gizmos;
    control_ui_owners.clear();control_ui_failed=false;
    if(controls){ControlPort preview{memory,binding,request,before};if(!preview.preflight(error))return a::Outcome::RejectedNoEffect;}
    else{Port preview{memory,binding,request,before};if(!preview.preflight(dungeon_input_native::Tool(request.dungeon_input.tool),error))return a::Outcome::RejectedNoEffect;}
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;
    const auto unknown=[&](const std::string& why){replaying=false;op.unknown=true;op.cause=why.empty()?"Dungeon input original failure without cause":why;error=op.cause;return a::Outcome::Unknown;};
    const auto random=authority?authority->dungeon_input_before.protected_state.dungeon.global_random:before.value.protected_state.dungeon.global_random;
    op.seed_attempted=true;if(!original_result(action_runtime::seed(state.image,random),"Host RNG adoption",error)||
       !sample(memory,binding,request.snapshot_uuid,request.dungeon_input,op.before,error)||op.before.value.protected_state.dungeon.global_random!=random)
        return unknown(error.empty()?"Dungeon input Host RNG readback differs":error);
    op.evidence.before=op.before.source.palette.dungeon.dungeon.dungeon.money;op.evidence.dungeon_input_before=op.before.value;
    if(!npc_quest_target_runtime::prepare_dungeon_cells(binding,request,op.zones,error))return unknown(error);
    Port port{memory,binding,request,op.before};ControlPort control{memory,binding,request,op.before};replaying=true;
    control_ui_active=controls;
    const bool applied=controls?dungeon_gizmo_control_native::execute(control,*request.dungeon_input.controls,op.control,error):
        dungeon_input_native::apply(port,dungeon_input_native::Tool(request.dungeon_input.tool),op.native,error);
    control_ui_active=false;control_ui_owners.clear();replaying=false;
    if(!applied){const auto original=error;std::string cleanup;if(!port.cleanup(cleanup))error=original+"; cleanup: "+cleanup;return unknown(error);}
    if(controls){op.native.entered=op.control.attempted;op.native.graph=op.control.graph;op.native.committed=op.control.confirmed;}
    if(!(controls?publish_controls(memory,binding,request,op.before,control,error):publish_birth(memory,binding,request,op.before,port,error))||
       !npc_quest_target_runtime::confirm_dungeon_cells(binding,request,op.zones,error)||
       !sample(memory,binding,request.snapshot_uuid,request.dungeon_input,op.after,error,&op.before.source.quests,request.action_uuid)||
       (controls&&!control_monsters(op.before,op.after,error))||
       !dungeon_palette::same_local(op.before.source.palette,op.after.source.palette)||
       !a::dungeon_input_transition(request.action_uuid,request.dungeon_input,op.before.value,op.after.value)||
       !a::dungeon_input_purchase(request.dungeon_input,op.evidence.before,op.after.source.palette.dungeon.dungeon.dungeon.money))
        return unknown(error.empty()?"Dungeon input original complete effect, protected owner or exact full ledger differs":error);
    const auto tool=request.dungeon_input.tool;
    const dungeon_cell_graph::Policy policy=controls?control_policy(*request.dungeon_input.controls):dungeon_cell_graph::Policy{tool==a::DungeonInputTool::Paint?dungeon_cell_graph::Shape::Paint:
        tool==a::DungeonInputTool::Door?dungeon_cell_graph::Shape::Door:(tool==a::DungeonInputTool::Key||tool==a::DungeonInputTool::Treasure)?dungeon_cell_graph::Shape::Key:dungeon_cell_graph::Shape::Monster,
        uint32_t(tool==a::DungeonInputTool::Door||tool==a::DungeonInputTool::Treasure||(tool==a::DungeonInputTool::Key&&request.dungeon_input.cost!=0))};
    undo_probe::GraphWitness graph;
    if(!undo_probe::read_graph(memory.process,state.image,op.native.graph,graph,error)||
       !undo_probe::validate_undo_commit(op.before.source.palette.dungeon.dungeon.dungeon.undo,op.after.source.palette.dungeon.dungeon.dungeon.undo,graph,error)||
       !graph.locked||graph.pending_delete||!dungeon_cells::graph_capture(memory,op.before.source,op.after.source,op.native.graph,op.current_graph,error,policy)||
       (controls&&!control_graph(memory,request,op.current_graph,error))||
       !building_history_slice::remember_dungeon_input(request.action_uuid,op.native.graph,error))return unknown(error.empty()?"Dungeon input original graph commit/retained payload differs":error);
    op.evidence.after=op.after.source.palette.dungeon.dungeon.dungeon.money;op.evidence.dungeon_input_after=op.after.value;op.evidence.undo_confirmed=true;
    if(authority){auto expected=*authority;expected.retirement_confirmed=false;if(op.evidence!=expected)return unknown("Dungeon input Client complete original result differs from exact Host evidence");}
    op.confirmed=true;return a::Outcome::Confirmed;
}catch(const std::exception& exception){control_ui_active=false;control_ui_owners.clear();replaying=false;error=std::string("Dungeon input original boundary: ")+exception.what();
    const auto retained=operations.find(request.action_uuid);if(retained!=operations.end()){retained->second.unknown=true;retained->second.cause=error;}return a::Outcome::Unknown;}
catch(...){control_ui_active=false;control_ui_owners.clear();replaying=false;error="Dungeon input original non-standard boundary exception";
    const auto retained=operations.find(request.action_uuid);if(retained!=operations.end()){retained->second.unknown=true;retained->second.cause=error;}return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& error){
    const auto found=operations.find(request.action_uuid);
    if(GetCurrentThreadId()!=state.thread||found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed)
        return fail(error,"Dungeon input observation lacks exact confirmed current native receipt");
    const auto& op=found->second;Memory memory;Sample current;
    if(!sample(memory,binding,request.snapshot_uuid,request.dungeon_input,current,error,&op.after.source.quests,request.action_uuid)||
       !dungeon_palette::same_local(op.after.source.palette,current.source.palette)||current.value!=op.after.value||
       current.source.palette.dungeon.dungeon.dungeon.money!=op.evidence.after||!dungeon_cells::graph_unchanged(memory,op.current_graph,error))
        return fail(error,error.empty()?"Dungeon input complete current native result changed before ACK":error);
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence value;if(!observe(binding,request,value,error))return false;
    auto& op=operations.at(request.action_uuid);if(op.retired)return fail(error,"Dungeon input receipt already retired");op.retired=true;return true;}
json status(){json values=json::object();for(const auto& [id,op]:operations)values[id]={{"entered",op.native.entered},{"steps",op.native.steps},
    {"purchased",op.native.purchased},{"created",op.native.created},{"committed",op.native.committed},{"unknown",op.unknown||op.native.unknown},
    {"confirmed",op.confirmed},{"retired",op.retired},{"cause",op.cause},{"cleanup_cause",op.native.cleanup_cause}};
    return {{"enabled",ready},{"fault",fault},{"collecting",bool(stroke)},{"pending_ui",pending_ui()},{"captures",captures},{"submitted",submitted},
        {"rejected",rejected},{"last_action",last_action},{"cause",cause},{"operations",values},{"history",history_status()}};
}
}
