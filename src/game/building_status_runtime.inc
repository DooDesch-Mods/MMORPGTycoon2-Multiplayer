namespace building_status {
namespace {
using Command=bool(*)(void*,int32_t);
Command original=nullptr;uint64_t generation=0,captures=0,submitted=0,rejected=0;bool installed=false,ready=false,fault=false,replaying=false;
uintptr_t replay_receiver=0;int32_t replay_state=-1;uint32_t original_calls=0;std::string cause,last_action;
struct Sample {host::WorldState world;uintptr_t building=0,dungeon=0,collection=0;uint64_t uid=0;a::BuildingStatusState value;a::Ledger money;undo_probe::UndoWitness undo;
    building_status_work::Readback work;};
struct Pending {a::Binding binding;std::string snapshot;a::BuildingStatusIntent intent;};
struct Operation {a::Binding binding;a::Request request;Sample before,after;a::Evidence evidence;bool attempted=false,returned=false,confirmed=false,retired=false,seed_attempted=false;uint32_t calls=0;std::string cause;};
std::optional<Pending> pending;std::map<std::string,Operation> operations;
bool entry(void*,int32_t);
bool reject(const std::string& error){++rejected;if(cause!=error){cause=error;LOG_ERROR("[BuildingStatus] {}",error);}return false;}
bool normal_caller(uintptr_t caller){return caller==0x6d5b62||caller==0x6d5baa||caller==0x6d5bba||caller==0x66e41e||caller==0x66e435||caller==0x66e46b;}
bool code(Memory& memory,std::string& error){
    const auto& pin=building_status_codes::entry;std::string bytes(pin.bytes,'\0'),hash;if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error))return false;
    if(original){MT2FW_MT2_RouteReceipt receipt{sizeof receipt};const npc_quest_list_ui_codes::Owned owner{uintptr_t(&entry),uintptr_t(original),generation};
        if(!quest_list::read_route(state.image+pin.rva,receipt,error)||!npc_quest_list_ui_codes::receipt(receipt,state.image+pin.rva,pin,owner,error)||std::memcmp(bytes.data(),receipt.current,32))return fail(error,error.empty()?"Building status exact owned route differs":error);
        std::array<unsigned char,20> trampoline{};uintptr_t resume=0;constexpr std::array<unsigned char,6> continuation{0xff,0x25,0,0,0,0};
        if(!memory.copy(uintptr_t(original),trampoline.data(),trampoline.size(),error))return false;std::memcpy(&resume,trampoline.data()+12,8);
        if(!std::equal(pin.prefix.begin(),pin.prefix.begin()+6,trampoline.begin())||!std::equal(continuation.begin(),continuation.end(),trampoline.begin()+6)||resume!=state.image+pin.rva+6)return fail(error,"Building status exact original six-byte instruction boundary differs");
        std::memcpy(bytes.data(),receipt.pristine,32);if(!quest_list::verify_code(state.image+pin.rva,bytes.data(),bytes.size(),error))return false;}
    if(!quest_bridge::sha256(bytes,hash,error)||hash!=pin.sha)return fail(error,error.empty()?"Building status original complete transaction body differs":error);
    for(const auto& helper:building_status_codes::helpers){bytes.assign(helper.size,'\0');if(!memory.copy(state.image+helper.rva,bytes.data(),bytes.size(),error)||!quest_bridge::sha256(bytes,hash,error))return false;
        if(hash!=helper.sha)return fail(error,"Building status normal UI/native dependency body differs RVA="+std::to_string(helper.rva));}
    float bandwidth=0;if(!field(memory,state.image,0x149d1d0,bandwidth,error)||bandwidth!=.5f)return fail(error,error.empty()?"Building original online bandwidth constant differs":error);return true;
}
bool sample(Memory& memory,const a::Binding& binding,const std::string& snapshot,const a::BuildingStatusIntent& intent,Sample& out,std::string& error){
    out={};host::LookupApi api;host::NativeOrigin origin;if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;host::Reader reader(memory.process,origin,state.image,state.thread,api);
    if(!reader.current(out.world,true,error)||!action_runtime::bound(out.world,binding,snapshot,error)||!remove_diagnostics::resolve_current_building_identity(snapshot,intent.key,out.building,out.uid,error))return false;
    uintptr_t ap=0,shard=0;uint8_t flag=2;
    if(!field(memory,out.building,0,ap,error)||ap!=state.image+0x16d93c0||!field(memory,out.building,0x170,shard,error)||shard!=out.world.root||!field(memory,out.building,0x7a8,out.value.state,error)||out.value.state<0||out.value.state>2||
       !field(memory,out.building,0x388,flag,error)||flag>1)return fail(error,error.empty()?"Building status root/full identity/state/construction differs":error);out.value.construction=flag;
    if(!field(memory,out.building,0xbd0,flag,error)||flag>1||!field(memory,out.building,0x988,out.value.bandwidth,error)||!std::isfinite(out.value.bandwidth)||out.value.bandwidth<0)return fail(error,error.empty()?"Building status original user flag/bandwidth differs":error);out.value.user_set=flag!=0;
    if(!field(memory,out.building,0xa38,out.dungeon,error)||!field(memory,out.building,0xbd8,out.collection,error))return false;out.value.dungeon=out.dungeon!=0;
    if(out.dungeon){uintptr_t exterior=0;if(!field(memory,out.dungeon,0,ap,error)||ap!=state.image+0x16c0560||!field(memory,out.dungeon,0x6f8,exterior,error)||exterior!=out.building||!field(memory,out.dungeon,0x408,flag,error)||flag>1)return fail(error,error.empty()?"Building status Dungeon reciprocal exterior/readiness differs":error);out.value.dungeon_ready=flag!=0;}
    a::Context context;if(!a::selected_session().current(context,error)||context.snapshot_uuid!=snapshot||
       !building_status_work::read(context,binding,state.image,state.thread,uintptr_t(out.world.origin.world),uintptr_t(out.world.origin.map),out.building,out.collection,intent.key,intent.state,out.work,error))return false;
    out.value.bugs=out.work.bugs;out.value.jobs=out.work.jobs;out.value.linked=out.work.linked;out.value.developers=out.work.developers;
    out.value.bugs_sha256=out.work.bugs_sha256;out.value.expected_bugs_sha256=out.work.expected_bugs_sha256;out.value.pool_sha256=out.work.pool_sha256;
    native::SimulationState money;uintptr_t random=0;if(!native::read_simulation_state(money,error)||!undo_probe::read_undo(memory.process,state.image,out.undo,error)||out.undo.active_graph||!field(memory,state.image,0x164b250,random,error)||!field(memory,random,0,out.value.global_random,error))return fail(error,error.empty()?"Building status ledger/Undo/global RNG binding differs":error);
    out.money=ledger(money);out.value.undo_count=out.undo.undo.size();
    return true;
}
bool same_local(const Sample& before,const Sample& after){return same_world(before.world,after.world)&&before.building==after.building&&before.uid==after.uid&&before.dungeon==after.dungeon&&before.collection==after.collection&&before.work.children==after.work.children&&before.money==after.money&&before.undo==after.undo;}
bool eligible(const a::BuildingStatusIntent& intent,const a::BuildingStatusState& before,std::string& error){
    if(intent.before!=before.state)return fail(error,"Building status captured prior state differs");if(intent.state==2&&!intent.dungeon_ui&&before.construction)return fail(error,"Building status rejected by original BuildingInfoWindow state2 guard");
    if(intent.dungeon_ui&&!before.dungeon)return fail(error,"Building status Dungeon UI intent has no reciprocal actual Dungeon");
    if(before.dungeon&&intent.state&&!before.dungeon_ready)return fail(error,"Building status nonzero state rejected by original Dungeon readiness guard");return true;
}
bool entry(void* receiver,int32_t value)try{
    if(!original){fault=true;return reject("Building status original unavailable");}if(!net::session_wire::connected())return original(receiver,value);
    if(replaying){if(uintptr_t(receiver)!=replay_receiver||value!=replay_state||original_calls){fault=true;return reject("Building status original-once receiver/state/count differs");}++original_calls;return original(receiver,value);}
    const auto caller=uintptr_t(_ReturnAddress())-state.image;if(!normal_caller(caller))return original(receiver,value);
    Memory memory;std::string error;const auto controller=typed_action_runtime::controller_status();a::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!ready||fault||pending||costume_edit::pending_ui()||dungeon_config::pending_ui()||dungeon_sound::pending_ui()||dungeon_gizmo::pending_ui()||dungeon_palette::pending_ui()||dungeon_light::pending_ui()||quest_list::pending_ui()||character_rename::pending_ui()||ability_edit::pending_ui()||GetCurrentThreadId()!=state.thread||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||value<0||value>2||!code(memory,error)||!typed_action_runtime::network_binding(snapshot,binding,error))return reject(error.empty()?"Building status capture requires idle bound Mainthread Controller":error);
    uint64_t uid=0;a::BuildingStatusIntent intent;intent.state=value;intent.dungeon_ui=caller==0x6d5b62||caller==0x6d5baa||caller==0x6d5bba;
    if(!field(memory,uintptr_t(receiver),0x18,uid,error)||!remove_diagnostics::resolve_current_building_key(snapshot,uintptr_t(receiver),uid,intent.key,error)||!field(memory,uintptr_t(receiver),0x7a8,intent.before,error))return reject(error);
    Sample before;if(!sample(memory,binding,snapshot,intent,before,error)||before.building!=uintptr_t(receiver)||!eligible(intent,before.value,error))return reject(error.empty()?"Building status current captured receiver differs":error);
    pending=Pending{binding,snapshot,intent};++captures;return true;
}catch(const std::exception& exception){fault=true;return reject(std::string("Building status capture boundary: ")+exception.what());}catch(...){fault=true;return reject("Building status capture non-standard exception");}
uint32_t invoke(uintptr_t building,int32_t value,bool& accepted)noexcept{__try{accepted=reinterpret_cast<Command>(state.image+building_status_codes::entry.rva)(reinterpret_cast<void*>(building),value);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
}
bool pending_ui(){return pending.has_value()||replaying;}
bool retire_lobby(std::string& e){
    if(!ready){e.clear();return true;}
    if(GetCurrentThreadId()!=state.thread)return fail(e,"Building status lobby retirement requires Mainthread");
    native::WorldStatus world;if(!native::read_world_status(world,e))return false;
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0)return fail(e,"Building status lobby retirement requires actual idle Title");
    if(replaying)return fail(e,"Building status lobby retirement cannot interrupt original execution");
    pending.reset();operations.clear();replay_state=-1;replay_receiver=0;original_calls=0;
    fault=false;cause.clear();last_action.clear();e.clear();return true;
}
bool enable(std::string& error){
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(error,"Building status requires bound Mainthread");if(ready){for(const auto& [id,op]:operations)if(!op.retired)return fail(error,"Building status retained unretired operation="+id);if(fault||pending_ui())return fail(error,"Building status retained unresolved capture");Memory memory;if(!code(memory,error))return false;operations.clear();return true;}
    if(installed)return fail(error,"Building status earlier installation failed: "+cause);installed=true;Memory memory;if(!code(memory,error))return false;
    const auto& pin=building_status_codes::entry;auto status=MH_CreateHook(reinterpret_cast<void*>(state.image+pin.rva),reinterpret_cast<void*>(&entry),reinterpret_cast<void**>(&original));
    if(status!=MH_OK||!original)return fail(error,"Building status prepare failed status="+std::to_string(status)+" cause="+mt2mp::hooks::framework::process_backend().error());status=MH_EnableHook(reinterpret_cast<void*>(state.image+pin.rva));if(status!=MH_OK)return fail(error,"Building status enable failed status="+std::to_string(status));
    MT2FW_MT2_RouteReceipt receipt{sizeof receipt};if(!quest_list::read_route(state.image+pin.rva,receipt,error))return false;generation=receipt.generation;if(!code(memory,error))return false;ready=true;return true;
}
void tick(){if(!pending)return;const auto saved=*pending;pending.reset();std::string error;a::Binding binding;if(!ready||fault||saved.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(saved.snapshot,binding,error)||binding!=saved.binding){fault=true;reject(error.empty()?"Building status captured world changed":error);return;}
    const auto id=action_runtime::uuid(error);if(id.empty()||!typed_action_runtime::submit_building_status(id,saved.intent,error)){fault=true;reject(error);return;}++submitted;last_action=id;}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    if(!ready||fault||GetCurrentThreadId()!=state.thread||request.action!=a::Action::SetBuildingStatus){error="Building status runtime unavailable";return a::Outcome::RejectedNoEffect;}if(operations.contains(request.action_uuid)){error="Building status already attempted; no replay";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();if(!controller.value("pending",false)||controller.value("locked",true)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Building status requires exact pending Controller";return a::Outcome::RejectedNoEffect;}
    Memory memory;Sample before;const auto& intent=request.building_status;if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,intent,before,error)||!eligible(intent,before.value,error))return a::Outcome::RejectedNoEffect;
    if(authority){auto portable=before.value;portable.global_random=authority->building_status_before.global_random;if(portable!=authority->building_status_before||before.money!=authority->before){error="Building status Client prior native Bug/Job/ledger/target census differs from Host";return a::Outcome::RejectedNoEffect;}}
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;
    const auto unknown=[&](const std::string& why){replaying=false;replay_receiver=0;error=op.cause=why.empty()?"Building status failed without cause":why;return a::Outcome::Unknown;};
    const auto desired_rng=authority?authority->building_status_before.global_random:before.value.global_random;op.seed_attempted=true;const auto seed=action_runtime::seed(state.image,desired_rng);if(seed)return unknown("Building status Host RNG adoption SEH="+std::to_string(seed));
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.before,error)||!same_local(before,op.before)||op.before.value.global_random!=desired_rng)return unknown(error.empty()?"Building status Host RNG/current identity readback differs":error);
    op.evidence.before=op.before.money;op.evidence.building_status_before=op.before.value;op.attempted=true;replaying=true;replay_receiver=op.before.building;replay_state=intent.state;original_calls=0;
    bool accepted=false;const auto seh=invoke(op.before.building,intent.state,accepted);op.calls=original_calls;op.returned=seh==0;replaying=false;replay_receiver=0;
    if(seh||!accepted||op.calls!=1)return unknown("Building status original native transaction unconfirmed SEH="+std::to_string(seh)+" accepted="+std::to_string(accepted)+" calls="+std::to_string(op.calls));
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.after,error)||!same_local(op.before,op.after)||!a::building_status_transition(intent,op.before.value,op.after.value))return unknown(error.empty()?"Building status original state/bandwidth/Bug/Job/Undo/ledger effect differs":error);
    op.evidence.after=op.after.money;op.evidence.building_status_after=op.after.value;if(authority){auto expected=*authority;expected.retirement_confirmed=false;if(op.evidence!=expected)return unknown("Building status Client original result differs from Host");}op.confirmed=true;return a::Outcome::Confirmed;
}catch(const std::exception& exception){replaying=false;replay_receiver=0;error=std::string("Building status perform boundary: ")+exception.what();return a::Outcome::Unknown;}catch(...){replaying=false;replay_receiver=0;error="Building status perform non-standard exception";return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& result,std::string& error){const auto found=operations.find(request.action_uuid);if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.returned||found->second.calls!=1)return fail(error,"Building status observation lacks exact confirmed original-once receipt");
    const auto& op=found->second;Memory memory;Sample now;if(!sample(memory,binding,request.snapshot_uuid,request.building_status,now,error)||!same_local(op.after,now)||now.value!=op.after.value)return fail(error,error.empty()?"Building status current native result differs before ACK":error);result=op.evidence;result.retirement_confirmed=op.retired;return true;}
bool retire(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence ignored;if(!observe(binding,request,ignored,error))return false;auto& op=operations.at(request.action_uuid);if(op.retired)return fail(error,"Building status receipt already retired");op.retired=true;return true;}
json status(){json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"attempted",op.attempted},{"returned",op.returned},{"original_calls",op.calls},{"confirmed",op.confirmed},{"retired",op.retired},{"seed_attempted",op.seed_attempted},{"cause",op.cause},
    {"bugs_before",op.before.work.bugs_record},{"bugs_after",op.after.work.bugs_record},{"pool_before",op.before.work.pool_record},{"pool_after",op.after.work.pool_record}};
    return {{"enabled",ready},{"fault",fault},{"pending_ui",pending_ui()},{"captures",captures},{"submitted",submitted},{"rejected",rejected},{"last_action",last_action},{"cause",cause},{"operations",rows}};}
}
