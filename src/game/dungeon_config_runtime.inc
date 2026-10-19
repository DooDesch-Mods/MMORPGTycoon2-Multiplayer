namespace dungeon_config {
namespace {
using Command=a::ui_diagnostic::UiCommand;
Command original=nullptr;uint64_t generation=0,captures=0,submitted=0,rejected=0;bool installed=false,ready=false,fault=false,replaying=false;
uintptr_t replay_receiver=0;uint32_t original_calls=0;std::string cause,last_action;
constexpr npc_quest_list_ui_codes::Code ui_code{0x6aea60,348,"d83bc1cc9ba2ec0cb1a974ad81ba102ce240e141ab298b4593493ae648a53ce5",
    {0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x48,0x48,0x8d,0x5c,0x24,0x20,0x48,0x89,0xd6,0x48,0x89,0xcf,0x48,0x8d,0x15,0xed,0x80,0xe6,0,0x48,0x89,0xd9,0xe8,0x6e,0xed}};
struct Sample {host::WorldState world;dungeon_config_reader::Local local;uintptr_t business=0,config=0; a::DungeonConfigState value;a::Ledger money;undo_probe::UndoWitness undo;};
struct Pending {a::Binding binding;std::string snapshot;a::DungeonConfigIntent intent;};
struct Operation {a::Binding binding;a::Request request;Sample before,after;a::Evidence evidence;bool attempted=false,returned=false,confirmed=false,retired=false,prepared=false,seed_attempted=false;uint32_t calls=0;std::string cause;};
std::optional<Pending> pending;std::map<std::string,Operation> operations;
bool entry(void*,void*);
bool reject(const std::string& error){++rejected;if(cause!=error){cause=error;LOG_ERROR("[DungeonConfig] {}",error);}return false;}
bool code(Memory& memory,std::string& error){
    std::string bytes(ui_code.bytes,'\0'),hash;if(!memory.copy(state.image+ui_code.rva,bytes.data(),bytes.size(),error))return false;
    if(original){MT2FW_MT2_RouteReceipt receipt{sizeof receipt};const npc_quest_list_ui_codes::Owned owner{uintptr_t(&entry),uintptr_t(original),generation};
        if(!quest_list::read_route(state.image+ui_code.rva,receipt,error)||!npc_quest_list_ui_codes::receipt(receipt,state.image+ui_code.rva,ui_code,owner,error)||std::memcmp(bytes.data(),receipt.current,32))return fail(error,error.empty()?"Dungeon config exact owned route differs":error);
        std::array<unsigned char,22> trampoline{};uintptr_t resume=0;constexpr std::array<unsigned char,6> continuation{0xff,0x25,0,0,0,0};
        if(!memory.copy(uintptr_t(original),trampoline.data(),trampoline.size(),error))return false;std::memcpy(&resume,trampoline.data()+14,8);
        if(!std::equal(ui_code.prefix.begin(),ui_code.prefix.begin()+8,trampoline.begin())||!std::equal(continuation.begin(),continuation.end(),trampoline.begin()+8)||resume!=state.image+ui_code.rva+8)return fail(error,"Dungeon config exact original eight-byte instruction boundary differs");
        std::memcpy(bytes.data(),receipt.pristine,32);if(!quest_list::verify_code(state.image+ui_code.rva,bytes.data(),std::min<size_t>(bytes.size(),4096),error))return false;}
    if(!quest_bridge::sha256(bytes,hash,error)||hash!=ui_code.sha)return fail(error,error.empty()?"Dungeon config original outer UI transaction body differs":error);
    for(const auto& pin:dungeon_config_codes::helpers){
        bytes.assign(pin.size,'\0');if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error))return false;
        const bool economic_helper=pin.rva==0x54d80;
        if(economic_helper){
            MT2FW_MT2_RouteReceipt receipt{sizeof receipt};
            if(!quest_list::read_route(state.image+pin.rva,receipt,error))return false;
            if(bytes.size()<sizeof receipt.current||std::memcmp(bytes.data(),receipt.current,sizeof receipt.current))
                return fail(error,"Dungeon config economic helper current route bytes differ");
            // AddExpense is already owned by the economic fence. Reconstruct only this local code copy.
            std::memcpy(bytes.data(),receipt.pristine,sizeof receipt.pristine);
        }
        if(!quest_bridge::sha256(bytes,hash,error))return false;
        if(hash!=pin.sha)return fail(error,"Dungeon config original preparation/ownership helper body differs RVA="+std::to_string(pin.rva));
        if(economic_helper&&!quest_list::verify_code(state.image+pin.rva,bytes.data(),bytes.size(),error))return false;
    }
    return true;
}
bool sample(Memory& memory,const a::Binding& binding,const std::string& snapshot,const a::DungeonConfigIntent& intent,Sample& out,std::string& error){
    out={};host::LookupApi api;host::NativeOrigin origin;if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;host::Reader reader(memory.process,origin,state.image,state.thread,api);
    uintptr_t exterior=0;uint64_t uid=0;if(!reader.current(out.world,true,error)||!action_runtime::bound(out.world,binding,snapshot,error)||!remove_diagnostics::resolve_current_building_identity(snapshot,intent.key,exterior,uid,error))return false;
    auto at=[&](uintptr_t object,size_t offset,auto& value,std::string& e){return field(memory,object,offset,value,e);};
    if(!dungeon_config_reader::read(at,state.image,out.world.root,exterior,uid,out.value,out.local,error))return false;
    uintptr_t ap=0;uint8_t sandbox=2;if(!field(memory,state.image,0x13df220,out.business,error)||!field(memory,out.business,0,ap,error)||ap!=state.image+0x16d97e0||!field(memory,out.business,0x108,out.config,error)||!field(memory,out.config,0,ap,error)||ap!=state.image+0x1712b40||!field(memory,out.config,0x30,out.value.price,error)||out.value.price<0||!field(memory,out.business,0xf0,sandbox,error)||sandbox>1)return fail(error,error.empty()?"Dungeon config current Business/config/price/sandbox differs":error);out.value.sandbox=sandbox!=0;
    native::SimulationState money;uintptr_t random=0;if(!native::read_simulation_state(money,error)||!undo_probe::read_undo(memory.process,state.image,out.undo,error)||out.undo.active_graph||!field(memory,state.image,0x164b250,random,error)||!field(memory,random,0,out.value.global_random,error))return fail(error,error.empty()?"Dungeon config ledger/Undo/global RNG differs":error);
    out.money=ledger(money);out.value.undo_count=uint32_t(out.undo.undo.size());return true;
}
uint32_t find_window(uintptr_t& outer)noexcept{__try{const quest_list::QuestTitleView name{"instance_config_window",22};outer=reinterpret_cast<uintptr_t(*)(const quest_list::QuestTitleView&)>(state.image+0x81aa90)(name);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool window(Memory& memory,uintptr_t& outer,uintptr_t& owner,int32_t& desired,int32_t& cost,std::string& error){
    const auto seh=find_window(outer);uintptr_t ap=0;if(seh||!outer||!field(memory,outer,0,ap,error)||ap!=state.image+0x173bee0||!field(memory,outer,0x510,owner,error)||!field(memory,outer,0x508,desired,error)||!field(memory,outer,0x50c,cost,error))return fail(error,error.empty()?"Dungeon config actual named native Window differs SEH="+std::to_string(seh):error);return true;
}
uint32_t find_spinner(uintptr_t outer,uintptr_t& spinner)noexcept{__try{
    const quest_list::QuestTitleView name{"instances_count",15};spinner=reinterpret_cast<uintptr_t(*)(void*,const quest_list::QuestTitleView&)>(state.image+0x81d860)(reinterpret_cast<void*>(outer),name);return 0;
}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool input_guard(Memory& memory,uintptr_t outer,uintptr_t& spinner,std::string& error){
    uintptr_t method=0,cached=0;if(!field(memory,state.image+0x173bee0,0xf0,method,error)||method!=state.image+0x81d860)return fail(error,error.empty()?"Dungeon config original Window FindPane entry differs":error);
    const auto seh=find_spinner(outer,spinner);auto read=[&](uintptr_t base,size_t offset,auto& out,std::string& e){return field(memory,base,offset,out,e);};
    if(seh)return fail(error,"Dungeon config original named Spinner lookup SEH="+std::to_string(seh));
    if(!dungeon_config_input::spinner(read,state.image,spinner,error)||!field(memory,outer,0x4f8,cached,error))return false;
    return !cached||cached==spinner||fail(error,"Dungeon config cached Spinner differs from original named lookup");
}
uint32_t prepare(uintptr_t outer,uintptr_t owner,int32_t desired)noexcept{__try{
    reinterpret_cast<void(*)(void*,void*)>(state.image+0x6aebc0)(reinterpret_cast<void*>(outer),reinterpret_cast<void*>(owner));
    *reinterpret_cast<int32_t*>(outer+0x508)=desired;
    reinterpret_cast<void(*)(void*)>(state.image+0x6adc90)(reinterpret_cast<void*>(outer));return 0;
}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool same_local(const Sample& before,const Sample& after){return same_world(before.world,after.world)&&before.local==after.local&&before.business==after.business&&before.config==after.config&&before.undo==after.undo;}
bool entry(void* receiver,void* record)try{
    if(!original){fault=true;return reject("Dungeon config original unavailable");}if(!net::session_wire::connected())return original(receiver,record);
    Memory memory;std::string error;a::ui_diagnostic::Api api;bool relevant=false;if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error)){fault=true;return reject(error);}
    auto seh=quest_list::label(api.label_equals,record,"Accept",relevant);if(seh){fault=true;return reject("Dungeon config Accept label SEH="+std::to_string(seh));}if(!relevant)return original(receiver,record);
    if(replaying){if(uintptr_t(receiver)!=replay_receiver||original_calls){fault=true;return reject("Dungeon config original-once receiver/count differs");}++original_calls;return original(receiver,record);}
    const auto controller=typed_action_runtime::controller_status();a::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!ready||fault||pending||building_status::pending_ui()||dungeon_light::pending_ui()||dungeon_sound::pending_ui()||dungeon_gizmo::pending_ui()||dungeon_palette::pending_ui()||quest_list::pending_ui()||character_rename::pending_ui()||ability_edit::pending_ui()||costume_edit::pending_ui()||GetCurrentThreadId()!=state.thread||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||!code(memory,error)||!typed_action_runtime::network_binding(snapshot,binding,error))return reject(error.empty()?"Dungeon config capture requires idle bound Mainthread Controller":error);
    uintptr_t outer=0,owner=0,exterior=0;int32_t desired=0,cost=0;uint64_t uid=0;
    if(!window(memory,outer,owner,desired,cost,error)||outer!=uintptr_t(receiver)||!owner||!field(memory,owner,0x6f8,exterior,error)||!field(memory,exterior,0x18,uid,error))return reject(error.empty()?"Dungeon config captured native owner absent":error);
    uintptr_t spinner=0;if(!input_guard(memory,outer,spinner,error))return reject(error);
    a::DungeonConfigIntent intent;intent.desired=desired;if(!remove_diagnostics::resolve_current_building_key(snapshot,exterior,uid,intent.key,error))return reject(error);Sample before;
    if(!sample(memory,binding,snapshot,intent,before,error)||before.local.dungeon!=owner)return reject(error.empty()?"Dungeon config captured owner differs":error);intent.before=before.value.desired;
    const int64_t expected=std::max<int64_t>(int64_t(desired)-intent.before,0)*before.value.price;
    if(desired<1||desired>10||expected>INT32_MAX||cost!=expected)return reject("Dungeon config captured UI count/cost differs from original formula");
    if(!before.value.sandbox&&before.money.cash<cost)return original(receiver,record);
    pending=Pending{binding,snapshot,intent};++captures;return true;
}catch(const std::exception& exception){fault=true;return reject(std::string("Dungeon config capture boundary: ")+exception.what());}catch(...){fault=true;return reject("Dungeon config capture non-standard exception");}
}
bool pending_ui(){return pending.has_value()||replaying;}
bool retire_lobby(std::string& e){
    if(!ready){e.clear();return true;}
    if(GetCurrentThreadId()!=state.thread)return fail(e,"Dungeon config edit lobby retirement requires Mainthread");
    native::WorldStatus world;if(!native::read_world_status(world,e))return false;
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0)return fail(e,"Dungeon config edit lobby retirement requires actual idle Title");
    if(replaying)return fail(e,"Dungeon config edit lobby retirement cannot interrupt original execution");
    pending.reset();operations.clear();replay_receiver=0;original_calls=0;
    fault=false;cause.clear();last_action.clear();e.clear();return true;
}
bool enable(std::string& error){
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(error,"Dungeon config edit requires bound Character Mainthread");
    if(ready){for(const auto& [id,op]:operations)if(!op.retired)return fail(error,"Dungeon config edit retained unretired operation="+id);if(fault||pending_ui())return fail(error,"Dungeon config edit retained unresolved capture");Memory memory;if(!code(memory,error))return false;operations.clear();return true;}
    if(installed)return fail(error,"Dungeon config edit earlier installation failed: "+cause);installed=true;Memory memory;if(!code(memory,error))return false;
    auto status=MH_CreateHook(reinterpret_cast<void*>(state.image+ui_code.rva),reinterpret_cast<void*>(&entry),reinterpret_cast<void**>(&original));
    if(status!=MH_OK||!original)return fail(error,"Dungeon config edit prepare failed status="+std::to_string(status)+" cause="+mt2mp::hooks::framework::process_backend().error());
    status=MH_EnableHook(reinterpret_cast<void*>(state.image+ui_code.rva));if(status!=MH_OK)return fail(error,"Dungeon config edit enable failed status="+std::to_string(status));
    MT2FW_MT2_RouteReceipt receipt{sizeof receipt};if(!quest_list::read_route(state.image+ui_code.rva,receipt,error))return false;generation=receipt.generation;if(!code(memory,error))return false;ready=true;return true;
}
void tick(){if(!pending)return;const auto saved=*pending;pending.reset();std::string error;a::Binding binding;
    if(!ready||fault||saved.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(saved.snapshot,binding,error)||binding!=saved.binding){fault=true;reject(error.empty()?"Dungeon config captured world changed":error);return;}
    const auto id=action_runtime::uuid(error);if(id.empty()||!typed_action_runtime::submit_dungeon_config(id,saved.intent,error)){fault=true;reject(error);return;}++submitted;last_action=id;
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    if(!ready||fault||GetCurrentThreadId()!=state.thread||request.action!=a::Action::ConfigureDungeonInstances){error="Dungeon config edit runtime unavailable";return a::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="Dungeon config edit already attempted; no replay";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();if(!controller.value("pending",false)||controller.value("locked",true)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Dungeon config edit requires exact pending Controller";return a::Outcome::RejectedNoEffect;}
    Memory memory;Sample before;const auto& intent=request.dungeon_config;
    if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,intent,before,error))return a::Outcome::RejectedNoEffect;
    if(before.value.desired!=intent.before){error="Dungeon config actual prior desired count differs";return a::Outcome::RejectedNoEffect;}
    if(!a::dungeon_config_wire_fits(request,before.value,before.money,error))return a::Outcome::RejectedNoEffect;
    if(authority){auto portable=before.value;portable.global_random=authority->dungeon_config_before.global_random;if(portable!=authority->dungeon_config_before||before.money!=authority->before){error="Dungeon config Client actual prior owner/census/ledger differs from Host";return a::Outcome::RejectedNoEffect;}}
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;
    const auto unknown=[&](const std::string& why){replaying=false;replay_receiver=0;error=op.cause=why.empty()?"Dungeon config edit failed without cause":why;return a::Outcome::Unknown;};
    uintptr_t outer=0,owner=0;int32_t desired=0,cost=0;if(!window(memory,outer,owner,desired,cost,error))return unknown(error);
    uintptr_t spinner=0;if(!input_guard(memory,outer,spinner,error))return unknown(error);
    op.prepared=true;const auto seh=prepare(outer,before.local.dungeon,intent.desired);if(seh)return unknown("Dungeon config original owned Window preparation SEH="+std::to_string(seh));
    const int64_t expected=std::max<int64_t>(int64_t(intent.desired)-before.value.desired,0)*before.value.price;
    if(!window(memory,outer,owner,desired,cost,error)||owner!=before.local.dungeon||desired!=intent.desired||expected>INT32_MAX||cost!=expected)return unknown(error.empty()?"Dungeon config actual prepared UI owner/count/cost differs":error);
    uintptr_t prepared_spinner=0;if(!input_guard(memory,outer,prepared_spinner,error)||prepared_spinner!=spinner)return unknown(error.empty()?"Dungeon config actual Spinner changed during original preparation":error);
    Sample prepared;if(!sample(memory,binding,request.snapshot_uuid,intent,prepared,error))return unknown(error);auto portable=prepared.value;portable.global_random=before.value.global_random;
    if(!same_local(before,prepared)||portable!=before.value||prepared.money!=before.money)return unknown("Dungeon config local UI preparation changed gameplay");
    const auto desired_rng=authority?authority->dungeon_config_before.global_random:before.value.global_random;op.seed_attempted=true;const auto seed=action_runtime::seed(state.image,desired_rng);if(seed)return unknown("Dungeon config Host RNG adoption SEH="+std::to_string(seed));
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.before,error)||op.before.value.global_random!=desired_rng)return unknown(error.empty()?"Dungeon config Host RNG readback differs":error);
    op.evidence.before=op.before.money;op.evidence.dungeon_config_before=op.before.value;
    a::ui_diagnostic::Api api;a::ui_diagnostic::Window command_window;command_window.window=outer;if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return unknown(error);api.command=reinterpret_cast<Command>(state.image+ui_code.rva);api.destroy_label="Accept";
    a::ui_diagnostic::Receipt receipt;op.attempted=true;replaying=true;replay_receiver=outer;original_calls=0;const bool returned=a::ui_diagnostic::process_calls().command(api,command_window,receipt,error);op.calls=original_calls;op.returned=receipt.ui_command_returned;replaying=false;replay_receiver=0;
    if(!returned||!op.returned||op.calls!=1||!receipt.record_cleanup_confirmed)return unknown(error.empty()?"Dungeon config original transaction lacks exact once/cleanup receipt":error);
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.after,error)||!same_local(op.before,op.after)||(!a::dungeon_config_transition(intent,op.before.value,op.after.value)||!a::dungeon_config_effect(request,op.evidence.before,op.after.money,op.before.value)))return unknown(error.empty()?"Dungeon config original effect/owner/census/ledger/Undo differs":error);
    op.evidence.after=op.after.money;op.evidence.dungeon_config_after=op.after.value;if(authority){auto expected=*authority;expected.retirement_confirmed=false;if(op.evidence!=expected)return unknown("Dungeon config Client original result differs from Host");}
    op.confirmed=true;return a::Outcome::Confirmed;
}catch(const std::exception& exception){replaying=false;replay_receiver=0;error=std::string("Dungeon config perform boundary: ")+exception.what();return a::Outcome::Unknown;}catch(...){replaying=false;replay_receiver=0;error="Dungeon config perform non-standard exception";return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& result,std::string& error){const auto found=operations.find(request.action_uuid);
    if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.returned||found->second.calls!=1)return fail(error,"Dungeon config observation lacks exact confirmed original-once receipt");
    const auto& op=found->second;Memory memory;Sample now;if(!sample(memory,binding,request.snapshot_uuid,request.dungeon_config,now,error)||!same_local(op.after,now)||now.value!=op.after.value)return fail(error,error.empty()?"Dungeon config current result differs before ACK":error);result=op.evidence;result.retirement_confirmed=op.retired;return true;}
bool retire(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence ignored;if(!observe(binding,request,ignored,error))return false;auto& op=operations.at(request.action_uuid);if(op.retired)return fail(error,"Dungeon config receipt already retired");op.retired=true;return true;}
json status(){json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"prepared",op.prepared},{"attempted",op.attempted},{"returned",op.returned},{"original_calls",op.calls},{"confirmed",op.confirmed},{"retired",op.retired},{"seed_attempted",op.seed_attempted},{"cause",op.cause}};return {{"enabled",ready},{"fault",fault},{"pending_ui",pending_ui()},{"captures",captures},{"submitted",submitted},{"rejected",rejected},{"last_action",last_action},{"cause",cause},{"operations",rows}};}
}
