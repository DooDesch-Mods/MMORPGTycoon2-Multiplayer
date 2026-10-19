namespace dungeon_sound {
namespace {
using Command=a::ui_diagnostic::UiCommand;
Command original=nullptr;uint64_t generation=0,captures=0,submitted=0,rejected=0;bool installed=false,ready=false,fault=false,replaying=false;
uintptr_t replay_receiver=0;uint32_t original_calls=0;int32_t replay_atmosphere=-1;bool preparing=false;std::string cause,last_action;
constexpr npc_quest_list_ui_codes::Code ui_code{0x6d4ff0,215,"94e4fd8e9934644efaf67f495886e82969ad509bb61c33fbb956f409a8edf4c7",
    {0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x48,0x48,0xb8,0x53,0x65,0x74,0x41,0x74,0x6d,0x6f,0x73,0x48,0x89,0xd7,0x48,0x8d,0x5c,0x24,0x20,0x48,0x89,0xce,0x48,0x89,0x44}};
struct Sample {dungeon_light::Sample dungeon;a::DungeonSoundState value;};
struct Pending {a::Binding binding;std::string snapshot;a::DungeonSoundIntent intent;};
struct Operation {a::Binding binding;a::Request request;Sample before,after;a::Evidence evidence;bool attempted=false,returned=false,confirmed=false,retired=false,prepared=false,seed_attempted=false;uint32_t calls=0,setup_callbacks=0,selection_callbacks=0;std::string cause;};
std::optional<Pending> pending;std::map<std::string,Operation> operations;
bool entry(void*,void*);
bool reject(const std::string& error){++rejected;if(cause!=error){cause=error;LOG_ERROR("[DungeonSound] {}",error);}return false;}
bool code(Memory& memory,std::string& error){
    std::string bytes(ui_code.bytes,'\0'),hash;if(!memory.copy(state.image+ui_code.rva,bytes.data(),bytes.size(),error))return false;
    if(original){MT2FW_MT2_RouteReceipt receipt{sizeof receipt};const npc_quest_list_ui_codes::Owned owner{uintptr_t(&entry),uintptr_t(original),generation};
        if(!quest_list::read_route(state.image+ui_code.rva,receipt,error)||!npc_quest_list_ui_codes::receipt(receipt,state.image+ui_code.rva,ui_code,owner,error)||std::memcmp(bytes.data(),receipt.current,32))return fail(error,error.empty()?"Dungeon sound exact owned route differs":error);
        std::array<unsigned char,22> trampoline{};uintptr_t resume=0;constexpr std::array<unsigned char,6> continuation{0xff,0x25,0,0,0,0};
        if(!memory.copy(uintptr_t(original),trampoline.data(),trampoline.size(),error))return false;std::memcpy(&resume,trampoline.data()+14,8);
        if(!std::equal(ui_code.prefix.begin(),ui_code.prefix.begin()+8,trampoline.begin())||!std::equal(continuation.begin(),continuation.end(),trampoline.begin()+8)||resume!=state.image+ui_code.rva+8)return fail(error,"Dungeon sound exact original eight-byte instruction boundary differs");
        std::memcpy(bytes.data(),receipt.pristine,32);if(!quest_list::verify_code(state.image+ui_code.rva,bytes.data(),std::min<size_t>(bytes.size(),4096),error))return false;}
    if(!quest_bridge::sha256(bytes,hash,error)||hash!=ui_code.sha)return fail(error,error.empty()?"Dungeon sound original outer UI transaction body differs":error);
    for(const auto& pin:dungeon_sound_codes::helpers){bytes.assign(pin.size,'\0');if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error)||!quest_bridge::sha256(bytes,hash,error))return false;
        if(hash!=pin.sha)return fail(error,"Dungeon sound original preparation/ownership helper body differs RVA="+std::to_string(pin.rva));}
    return true;
}
bool sample(Memory& memory,const a::Binding& binding,const std::string& snapshot,const a::DungeonSoundIntent& intent,Sample& out,std::string& error){
    out={};a::DungeonLightIntent target;target.key=intent.key;
    if(!dungeon_light::sample(memory,binding,snapshot,target,out.dungeon,error)||!field(memory,out.dungeon.dungeon.local.dungeon,0x830,out.value.atmosphere,error))return false;
    out.value.dungeon=out.dungeon.value.dungeon;out.value.light=out.dungeon.value.light;
    if(out.value.atmosphere<0||out.value.atmosphere>=6)return fail(error,"Dungeon sound actual atmosphere is outside native rows");return true;
}
uint32_t find_window(uintptr_t& outer)noexcept{__try{const quest_list::QuestTitleView name{"dungeon_sound",13};outer=reinterpret_cast<uintptr_t(*)(const quest_list::QuestTitleView&)>(state.image+0x81aa90)(name);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool window(Memory& memory,uintptr_t& outer,uintptr_t& owner,uintptr_t& grid,std::string& error){
    const auto seh=find_window(outer);uintptr_t ap=0;
    if(seh||!outer)return fail(error,"Dungeon sound named Window unavailable SEH="+std::to_string(seh));
    if(!field(memory,outer,0,ap,error)||!field(memory,outer,0x4e8,owner,error)||!field(memory,outer,0x4f0,grid,error))return false;
    if(ap!=state.image+0x172c3c0||!grid)return fail(error,"Dungeon sound actual Window/Grid differs");
    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& e){return field(memory,base,offset,value,e);};
    auto text=[&](uintptr_t base,size_t offset,std::string& value,std::string& e){return npc_quest_records::read_string(memory,base,offset,"Dungeon sound native Grid command",value,e);};
    return dungeon_sound_record::grid(at,text,state.image,outer,grid,error);
}
uint32_t prepare(uintptr_t outer,uintptr_t owner)noexcept{__try{*reinterpret_cast<uintptr_t*>(outer+0x4e8)=owner;reinterpret_cast<void(*)(void*)>(state.image+0x6d4cd0)(reinterpret_cast<void*>(outer));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t select(uintptr_t grid,int32_t atmosphere)noexcept{__try{reinterpret_cast<void(*)(void*,int32_t)>(state.image+0x70a9d0)(reinterpret_cast<void*>(grid),atmosphere);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool same_local(const Sample& before,const Sample& after){return dungeon_light::same_local(before.dungeon,after.dungeon);}
bool record_value(Memory& memory,void* record,int32_t& atmosphere,std::string& error){auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& e){return field(memory,base,offset,value,e);};return dungeon_sound_record::read(at,uintptr_t(record),atmosphere,error);}
bool replay_record_equals(const void* record,const char*){
    if(!reinterpret_cast<a::ui_diagnostic::RecordEquals>(state.image+0x1113d80)(record,"SetAtmos"))return false;
    Memory memory;std::string error;int32_t value=-1;
    if(!record_value(memory,const_cast<void*>(record),value,error)){reject(error);return false;}
    return value==replay_atmosphere;
}
bool entry(void* receiver,void* record)try{
    if(!original){fault=true;return reject("Dungeon sound original unavailable");}if(!net::session_wire::connected())return original(receiver,record);
    Memory memory;std::string error;a::ui_diagnostic::Api api;bool relevant=false;
    if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error)){fault=true;return reject(error);}
    const auto seh=quest_list::label(api.label_equals,record,"SetAtmos",relevant);
    if(seh){fault=true;return reject("Dungeon sound label SEH="+std::to_string(seh));}if(!relevant)return original(receiver,record);
    int32_t atmosphere=-1;if(!record_value(memory,record,atmosphere,error))return reject(error);
    // SelectRow sends a synchronous SetAtmos message. Preparing the local selection consumes that callback before its gameplay effect.
    if(replaying){bool execute=false;if(!dungeon_sound_record::replay(uintptr_t(receiver),atmosphere,replay_receiver,replay_atmosphere,preparing,original_calls,execute,error)){fault=true;return reject(error);}return execute?original(receiver,record):true;}
    const auto controller=typed_action_runtime::controller_status();a::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!ready||fault||pending||building_status::pending_ui()||dungeon_gizmo::pending_ui()||dungeon_palette::pending_ui()||quest_list::pending_ui()||character_rename::pending_ui()||ability_edit::pending_ui()||costume_edit::pending_ui()||dungeon_config::pending_ui()||dungeon_light::pending_ui()||GetCurrentThreadId()!=state.thread||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||!code(memory,error)||!typed_action_runtime::network_binding(snapshot,binding,error))return reject(error.empty()?"Dungeon sound capture requires idle bound Mainthread Controller":error);
    uintptr_t outer=0,owner=0,grid=0,exterior=0;uint64_t uid=0;a::DungeonSoundIntent intent;intent.atmosphere=atmosphere;
    if(!window(memory,outer,owner,grid,error)||outer!=uintptr_t(receiver)||!owner||!field(memory,owner,0x6f8,exterior,error)||!field(memory,exterior,0x18,uid,error)||!remove_diagnostics::resolve_current_building_key(snapshot,exterior,uid,intent.key,error))return reject(error.empty()?"Dungeon sound captured actual owner differs":error);
    Sample before;if(!sample(memory,binding,snapshot,intent,before,error)||before.dungeon.dungeon.local.dungeon!=owner)return reject(error.empty()?"Dungeon sound captured owner differs":error);
    intent.before=before.value.atmosphere;pending=Pending{binding,snapshot,intent};++captures;return true;
}catch(const std::exception& exception){fault=true;return reject(std::string("Dungeon sound capture boundary: ")+exception.what());}catch(...){fault=true;return reject("Dungeon sound capture non-standard exception");}
}
bool pending_ui(){return pending.has_value()||replaying;}
bool retire_lobby(std::string& e){
    if(!ready){e.clear();return true;}
    if(GetCurrentThreadId()!=state.thread)return fail(e,"Dungeon sound edit lobby retirement requires Mainthread");
    native::WorldStatus world;if(!native::read_world_status(world,e))return false;
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0)return fail(e,"Dungeon sound edit lobby retirement requires actual idle Title");
    if(replaying)return fail(e,"Dungeon sound edit lobby retirement cannot interrupt original execution");
    pending.reset();operations.clear();replay_receiver=0;replay_atmosphere=-1;preparing=false;original_calls=0;
    fault=false;cause.clear();last_action.clear();e.clear();return true;
}
bool enable(std::string& error){
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(error,"Dungeon sound edit requires bound Character Mainthread");
    if(ready){for(const auto& [id,op]:operations)if(!op.retired)return fail(error,"Dungeon sound edit retained unretired operation="+id);if(fault||pending_ui())return fail(error,"Dungeon sound edit retained unresolved capture");Memory memory;if(!code(memory,error))return false;operations.clear();return true;}
    if(installed)return fail(error,"Dungeon sound edit earlier installation failed: "+cause);installed=true;Memory memory;if(!code(memory,error))return false;
    auto status=MH_CreateHook(reinterpret_cast<void*>(state.image+ui_code.rva),reinterpret_cast<void*>(&entry),reinterpret_cast<void**>(&original));
    if(status!=MH_OK||!original)return fail(error,"Dungeon sound edit prepare failed status="+std::to_string(status)+" cause="+mt2mp::hooks::framework::process_backend().error());
    status=MH_EnableHook(reinterpret_cast<void*>(state.image+ui_code.rva));if(status!=MH_OK)return fail(error,"Dungeon sound edit enable failed status="+std::to_string(status));
    MT2FW_MT2_RouteReceipt receipt{sizeof receipt};if(!quest_list::read_route(state.image+ui_code.rva,receipt,error))return false;generation=receipt.generation;if(!code(memory,error))return false;ready=true;return true;
}
void tick(){if(!pending)return;const auto saved=*pending;pending.reset();std::string error;a::Binding binding;
    if(!ready||fault||saved.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(saved.snapshot,binding,error)||binding!=saved.binding){fault=true;reject(error.empty()?"Dungeon sound captured world changed":error);return;}
    const auto id=action_runtime::uuid(error);if(id.empty()||!typed_action_runtime::submit_dungeon_sound(id,saved.intent,error)){fault=true;reject(error);return;}++submitted;last_action=id;
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    if(!ready||fault||GetCurrentThreadId()!=state.thread||request.action!=a::Action::SetDungeonAtmosphere){error="Dungeon sound edit runtime unavailable";return a::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="Dungeon sound edit already attempted; no replay";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();if(!controller.value("pending",false)||controller.value("locked",true)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Dungeon sound edit requires exact pending Controller";return a::Outcome::RejectedNoEffect;}
    Memory memory;Sample before;const auto& intent=request.dungeon_sound;
    if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,intent,before,error))return a::Outcome::RejectedNoEffect;
    if(before.value.atmosphere!=intent.before){error=error.empty()?"Dungeon sound captured prior authored fields differ":error;return a::Outcome::RejectedNoEffect;}
    if(!a::dungeon_sound_wire_fits(request,before.value,before.dungeon.dungeon.money,error))return a::Outcome::RejectedNoEffect;
    if(authority){auto portable=before.value;portable.dungeon.global_random=authority->dungeon_sound_before.dungeon.global_random;if(portable!=authority->dungeon_sound_before||before.dungeon.dungeon.money!=authority->before){error="Dungeon sound Client actual prior owner/census/ledger differs from Host";return a::Outcome::RejectedNoEffect;}}
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;
    const auto unknown=[&](const std::string& why){replaying=false;preparing=false;replay_receiver=0;replay_atmosphere=-1;error=op.cause=why.empty()?"Dungeon sound edit failed without cause":why;return a::Outcome::Unknown;};
    uintptr_t outer=0,owner=0,grid=0;
    if(!window(memory,outer,owner,grid,error))return unknown(error);
    op.prepared=true;replaying=true;preparing=true;replay_receiver=outer;replay_atmosphere=intent.before;original_calls=0;
    const auto seh=prepare(outer,before.dungeon.dungeon.local.dungeon);op.setup_callbacks=original_calls;if(seh||fault)return unknown(seh?"Dungeon sound original Grid setup SEH="+std::to_string(seh):cause);
    replay_atmosphere=intent.atmosphere;original_calls=0;const auto selection=select(grid,intent.atmosphere);op.selection_callbacks=original_calls;
    replaying=false;preparing=false;replay_receiver=0;replay_atmosphere=-1;original_calls=0;if(selection||fault)return unknown(selection?"Dungeon sound original Grid selection SEH="+std::to_string(selection):cause);
    uintptr_t prepared_outer=0,prepared_owner=0,prepared_grid=0;
    if(!window(memory,prepared_outer,prepared_owner,prepared_grid,error)||prepared_outer!=outer||prepared_owner!=before.dungeon.dungeon.local.dungeon||prepared_grid!=grid)return unknown(error.empty()?"Dungeon sound prepared native Window/Grid differs":error);
    int32_t selected=-1,count=0;uint8_t active=0;
    if(!field(memory,grid,0x3e8,selected,error)||!field(memory,grid,0x3c8,count,error)||!field(memory,grid,0x3ec,active,error))return unknown(error);
    if(selected!=intent.atmosphere||count!=6||active!=1)return unknown("Dungeon sound original Grid preparation lacks the exact selected native row");
    Sample prepared;if(!sample(memory,binding,request.snapshot_uuid,intent,prepared,error))return unknown(error);auto portable=prepared.value;portable.dungeon.global_random=before.value.dungeon.global_random;
    if(!same_local(before,prepared)||portable!=before.value||prepared.dungeon.dungeon.money!=before.dungeon.dungeon.money)return unknown("Dungeon sound Window owner preparation changed gameplay");
    const auto desired_rng=authority?authority->dungeon_sound_before.dungeon.global_random:before.value.dungeon.global_random;op.seed_attempted=true;const auto seed=action_runtime::seed(state.image,desired_rng);if(seed)return unknown("Dungeon sound Host RNG adoption SEH="+std::to_string(seed));
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.before,error)||op.before.value.dungeon.global_random!=desired_rng)return unknown(error.empty()?"Dungeon sound Host RNG readback differs":error);
    op.evidence.before=op.before.dungeon.dungeon.money;op.evidence.dungeon_sound_before=op.before.value;
    a::ui_diagnostic::Api api;a::ui_diagnostic::Window command_window;command_window.window=outer;if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return unknown(error);
    const auto command_text=std::string("SetAtmos ")+std::to_string(intent.atmosphere);api.command=reinterpret_cast<Command>(state.image+ui_code.rva);api.destroy_label=command_text.c_str();api.label_equals=&replay_record_equals;
    a::ui_diagnostic::Receipt receipt;op.attempted=true;replaying=true;replay_receiver=outer;replay_atmosphere=intent.atmosphere;original_calls=0;const bool returned=a::ui_diagnostic::process_calls().command(api,command_window,receipt,error);op.calls=original_calls;op.returned=receipt.ui_command_returned;replaying=false;replay_receiver=0;replay_atmosphere=-1;
    if(!returned||!op.returned||op.calls!=1||!receipt.record_cleanup_confirmed||fault)return unknown(error.empty()?(cause.empty()?"Dungeon sound original transaction lacks exact once/cleanup receipt":cause):error);
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.after,error)||!same_local(op.before,op.after)||(!a::dungeon_sound_transition(intent,op.before.value,op.after.value)||op.before.dungeon.dungeon.money!=op.after.dungeon.dungeon.money))return unknown(error.empty()?"Dungeon sound original effect/owner/census/ledger/Undo differs":error);
    op.evidence.after=op.after.dungeon.dungeon.money;op.evidence.dungeon_sound_after=op.after.value;if(authority){auto expected=*authority;expected.retirement_confirmed=false;if(op.evidence!=expected)return unknown("Dungeon sound Client original result differs from Host");}
    op.confirmed=true;return a::Outcome::Confirmed;
}catch(const std::exception& exception){replaying=false;preparing=false;replay_receiver=0;replay_atmosphere=-1;error=std::string("Dungeon sound perform boundary: ")+exception.what();return a::Outcome::Unknown;}catch(...){replaying=false;preparing=false;replay_receiver=0;replay_atmosphere=-1;error="Dungeon sound perform non-standard exception";return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& result,std::string& error){const auto found=operations.find(request.action_uuid);
    if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.returned||found->second.calls!=1)return fail(error,"Dungeon sound observation lacks exact confirmed original-once receipt");
    const auto& op=found->second;Memory memory;Sample now;if(!sample(memory,binding,request.snapshot_uuid,request.dungeon_sound,now,error)||!same_local(op.after,now)||now.value!=op.after.value)return fail(error,error.empty()?"Dungeon sound current result differs before ACK":error);result=op.evidence;result.retirement_confirmed=op.retired;return true;}
bool retire(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence ignored;if(!observe(binding,request,ignored,error))return false;auto& op=operations.at(request.action_uuid);if(op.retired)return fail(error,"Dungeon sound receipt already retired");op.retired=true;return true;}
json status(){json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"prepared",op.prepared},{"setup_callbacks",op.setup_callbacks},{"selection_callbacks",op.selection_callbacks},{"attempted",op.attempted},{"returned",op.returned},{"original_calls",op.calls},{"confirmed",op.confirmed},{"retired",op.retired},{"seed_attempted",op.seed_attempted},{"cause",op.cause}};return {{"enabled",ready},{"fault",fault},{"pending_ui",pending_ui()},{"captures",captures},{"submitted",submitted},{"rejected",rejected},{"last_action",last_action},{"cause",cause},{"operations",rows}};}
}
