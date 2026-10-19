namespace dungeon_light {
namespace {
using Command=void(*)(void*);
Command original=nullptr;uint64_t generation=0,captures=0,submitted=0,rejected=0;bool installed=false,ready=false,fault=false,replaying=false;
uintptr_t replay_receiver=0;uint32_t original_calls=0;std::string cause,last_action;
constexpr npc_quest_list_ui_codes::Code ui_code{0x6d40b0,713,"1ad8ecbb064ecddeeee5936bb2e67430098874fb29180ac49eb105945a82ed19",
    {0x56,0x53,0x48,0x81,0xec,0x88,0,0,0,0x0f,0x11,0x74,0x24,0x40,0x0f,0x11,0x7c,0x24,0x50,0x44,0x0f,0x11,0x44,0x24,0x60,0x44,0x0f,0x11,0x4c,0x24,0x70,0x48}};
struct Sample {dungeon_config::Sample dungeon;uintptr_t environment=0;uint8_t dirty=0;a::DungeonLightState value;};
struct Pending {a::Binding binding;std::string snapshot;a::DungeonLightIntent intent;};
struct Operation {a::Binding binding;a::Request request;Sample before,after;a::Evidence evidence;bool attempted=false,returned=false,confirmed=false,retired=false,prepared=false,seed_attempted=false;uint32_t calls=0;std::string cause;};
std::optional<Pending> pending;std::map<std::string,Operation> operations;
void entry(void*);
bool reject(const std::string& error){++rejected;if(cause!=error){cause=error;LOG_ERROR("[DungeonLight] {}",error);}return false;}
bool code(Memory& memory,std::string& error){
    std::string bytes(ui_code.bytes,'\0'),hash;if(!memory.copy(state.image+ui_code.rva,bytes.data(),bytes.size(),error))return false;
    if(original){MT2FW_MT2_RouteReceipt receipt{sizeof receipt};const npc_quest_list_ui_codes::Owned owner{uintptr_t(&entry),uintptr_t(original),generation};
        if(!quest_list::read_route(state.image+ui_code.rva,receipt,error)||!npc_quest_list_ui_codes::receipt(receipt,state.image+ui_code.rva,ui_code,owner,error)||std::memcmp(bytes.data(),receipt.current,32))return fail(error,error.empty()?"Dungeon light exact owned route differs":error);
        std::array<unsigned char,23> trampoline{};uintptr_t resume=0;constexpr std::array<unsigned char,6> continuation{0xff,0x25,0,0,0,0};
        if(!memory.copy(uintptr_t(original),trampoline.data(),trampoline.size(),error))return false;std::memcpy(&resume,trampoline.data()+15,8);
        if(!std::equal(ui_code.prefix.begin(),ui_code.prefix.begin()+9,trampoline.begin())||!std::equal(continuation.begin(),continuation.end(),trampoline.begin()+9)||resume!=state.image+ui_code.rva+9)return fail(error,"Dungeon light exact original nine-byte instruction boundary differs");
        std::memcpy(bytes.data(),receipt.pristine,32);if(!quest_list::verify_code(state.image+ui_code.rva,bytes.data(),std::min<size_t>(bytes.size(),4096),error))return false;}
    if(!quest_bridge::sha256(bytes,hash,error)||hash!=ui_code.sha)return fail(error,error.empty()?"Dungeon light original outer UI transaction body differs":error);
    for(const auto& pin:dungeon_light_codes::helpers){bytes.assign(pin.size,'\0');if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error)||!quest_bridge::sha256(bytes,hash,error))return false;
        if(hash!=pin.sha)return fail(error,"Dungeon light original preparation/ownership helper body differs RVA="+std::to_string(pin.rva));}
    constexpr std::array<std::pair<uintptr_t,uint32_t>,5> constants{{{0x151f6c4,0xbc8efa36},{0x151f6c8,0},{0x151f6cc,0xbf800000},{0x151f6d0,0x3a83126f},{0x151f69c,0x3c23d70a}}};
    for(const auto& [rva,expected]:constants){uint32_t value=0;if(!field(memory,state.image,rva,value,error)||value!=expected)return fail(error,error.empty()?"Dungeon light pinned numeric constant differs":error);}return true;
}
bool sample(Memory& memory,const a::Binding& binding,const std::string& snapshot,const a::DungeonLightIntent& intent,Sample& out,std::string& error){
    out={};a::DungeonConfigIntent target;target.key=intent.key;if(!dungeon_config::sample(memory,binding,snapshot,target,out.dungeon,error)||!field(memory,out.dungeon.local.dungeon,0x1f0,out.value.light,error))return false;out.value.dungeon=out.dungeon.value;
    for(float x:out.value.light)if(!std::isfinite(x))return fail(error,"Dungeon light actual authored field is non-finite");
    uintptr_t ap=0;if(!field(memory,state.image,0x13df6c0,out.environment,error))return false;
    if(out.environment&&(!field(memory,out.environment,0,ap,error)||ap!=state.image+0x172c6a0||!field(memory,out.environment,0x4bc,out.dirty,error)||out.dirty>1))return fail(error,error.empty()?"Dungeon light actual EnvironmentManager/dirty flag differs":error);return true;
}
uint32_t find_window(uintptr_t& outer)noexcept{__try{const quest_list::QuestTitleView name{"dungeon_light",13};outer=reinterpret_cast<uintptr_t(*)(const quest_list::QuestTitleView&)>(state.image+0x81aa90)(name);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool window(Memory& memory,uintptr_t& outer,uintptr_t& owner,a::DungeonLightInput& input,dungeon_light_input::Widgets& widgets,std::string& error){
    const auto seh=find_window(outer);uintptr_t ap=0;if(seh||!outer||!field(memory,outer,0,ap,error)||ap!=state.image+0x172c100||!field(memory,outer,0x4e8,owner,error))return fail(error,error.empty()?"Dungeon light actual named native Window differs SEH="+std::to_string(seh):error);
    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& e){return field(memory,base,offset,value,e);};return dungeon_light_input::read(at,state.image,outer,input,widgets,error);
}
uint32_t convert(bool slider,uintptr_t object,int32_t raw,float& result)noexcept{__try{result=reinterpret_cast<float(*)(const void*,int32_t)>(state.image+(slider?0x7b95a0:0x7bf9e0))(reinterpret_cast<const void*>(object),raw);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t direction(float degrees,std::array<float,3>& result)noexcept{__try{
    alignas(16) std::array<unsigned char,16> angle{};const float radians=degrees*-0.01745329424738884f;
    reinterpret_cast<void(*)(void*,float)>(state.image+0x9eec90)(angle.data(),radians);
    constexpr std::array<float,2> forward{0,-1};const auto rotated=reinterpret_cast<uint64_t(*)(const void*,const void*)>(state.image+0x9eefa0)(angle.data(),forward.data());
    std::memcpy(&result[0],&rotated,4);result[1]=1;std::memcpy(&result[2],reinterpret_cast<const unsigned char*>(&rotated)+4,4);
    reinterpret_cast<void(*)(void*)>(state.image+0x9ffd80)(result.data());return 0;
}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool calculate(const a::DungeonLightInput& input,const dungeon_light_input::Widgets& widgets,std::array<float,23>& result,std::string& error){
    auto number=[&](bool slider,uintptr_t object,int32_t raw,float& out,std::string& e){const auto seh=convert(slider,object,raw,out);return !seh||fail(e,"Dungeon light original raw input conversion SEH="+std::to_string(seh));};
    auto vector=[&](float degrees,std::array<float,3>& out,std::string& e){const auto seh=direction(degrees,out);return !seh||fail(e,"Dungeon light original direction calculation SEH="+std::to_string(seh));};
    return dungeon_light_input::calculate(input,widgets,number,vector,result,error);
}
uint32_t prepare(uintptr_t outer,uintptr_t owner,const a::DungeonLightInput& input,const dungeon_light_input::Widgets& widgets)noexcept{__try{
    *reinterpret_cast<uintptr_t*>(outer+0x4e8)=owner;
    for(size_t i=0;i<4;++i){reinterpret_cast<void(*)(void*,const void*,bool)>(state.image+0x839d30)(reinterpret_cast<void*>(widgets.colors[i]),input.colors[i].data(),false);
        reinterpret_cast<void(*)(void*,int32_t)>(state.image+0x7c2c60)(reinterpret_cast<void*>(widgets.spinners[i]),input.spinners[i]);}
    for(size_t i=0;i<3;++i)reinterpret_cast<void(*)(void*,int32_t,bool)>(state.image+0x7bbdd0)(reinterpret_cast<void*>(widgets.sliders[i]),input.sliders[i],false);return 0;
}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool same_local(const Sample& before,const Sample& after){return dungeon_config::same_local(before.dungeon,after.dungeon)&&before.environment==after.environment;}
void entry(void* receiver)try{
    if(!original){fault=true;reject("Dungeon light original unavailable");return;}if(!net::session_wire::connected()){original(receiver);return;}
    const auto caller=uintptr_t(_ReturnAddress())-state.image;
    if(replaying){if(uintptr_t(receiver)!=replay_receiver||caller!=0x6d4438||original_calls){fault=true;reject("Dungeon light original-once receiver/caller/count differs");return;}++original_calls;original(receiver);return;}
    if(caller!=0x6d4438){original(receiver);return;}
    Memory memory;std::string error;const auto controller=typed_action_runtime::controller_status();a::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!ready||fault||pending||building_status::pending_ui()||quest_list::pending_ui()||character_rename::pending_ui()||ability_edit::pending_ui()||costume_edit::pending_ui()||dungeon_config::pending_ui()||dungeon_sound::pending_ui()||dungeon_gizmo::pending_ui()||dungeon_palette::pending_ui()||GetCurrentThreadId()!=state.thread||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||!code(memory,error)||!typed_action_runtime::network_binding(snapshot,binding,error)){reject(error.empty()?"Dungeon light capture requires idle bound Mainthread Controller":error);return;}
    uintptr_t outer=0,owner=0,exterior=0;uint64_t uid=0;a::DungeonLightIntent intent;dungeon_light_input::Widgets widgets;
    if(!window(memory,outer,owner,intent.input,widgets,error)||outer!=uintptr_t(receiver)||!owner||!field(memory,owner,0x6f8,exterior,error)||!field(memory,exterior,0x18,uid,error)||!remove_diagnostics::resolve_current_building_key(snapshot,exterior,uid,intent.key,error)){reject(error.empty()?"Dungeon light captured actual owner differs":error);return;}
    Sample before;if(!sample(memory,binding,snapshot,intent,before,error)||before.dungeon.local.dungeon!=owner||!calculate(intent.input,widgets,intent.light,error)||!quest_bridge::sha256(json(before.value.light).dump(),intent.before_sha256,error)){reject(error.empty()?"Dungeon light captured current authored record differs":error);return;}
    pending=Pending{binding,snapshot,intent};++captures;
}catch(const std::exception& exception){fault=true;reject(std::string("Dungeon light capture boundary: ")+exception.what());}catch(...){fault=true;reject("Dungeon light capture non-standard exception");}
}
bool pending_ui(){return pending.has_value()||replaying;}
bool retire_lobby(std::string& e){
    if(!ready){e.clear();return true;}
    if(GetCurrentThreadId()!=state.thread)return fail(e,"Dungeon light edit lobby retirement requires Mainthread");
    native::WorldStatus world;if(!native::read_world_status(world,e))return false;
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0)return fail(e,"Dungeon light edit lobby retirement requires actual idle Title");
    if(replaying)return fail(e,"Dungeon light edit lobby retirement cannot interrupt original execution");
    pending.reset();operations.clear();replay_receiver=0;original_calls=0;
    fault=false;cause.clear();last_action.clear();e.clear();return true;
}
bool enable(std::string& error){
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(error,"Dungeon light edit requires bound Character Mainthread");
    if(ready){for(const auto& [id,op]:operations)if(!op.retired)return fail(error,"Dungeon light edit retained unretired operation="+id);if(fault||pending_ui())return fail(error,"Dungeon light edit retained unresolved capture");Memory memory;if(!code(memory,error))return false;operations.clear();return true;}
    if(installed)return fail(error,"Dungeon light edit earlier installation failed: "+cause);installed=true;Memory memory;if(!code(memory,error))return false;
    auto status=MH_CreateHook(reinterpret_cast<void*>(state.image+ui_code.rva),reinterpret_cast<void*>(&entry),reinterpret_cast<void**>(&original));
    if(status!=MH_OK||!original)return fail(error,"Dungeon light edit prepare failed status="+std::to_string(status)+" cause="+mt2mp::hooks::framework::process_backend().error());
    status=MH_EnableHook(reinterpret_cast<void*>(state.image+ui_code.rva));if(status!=MH_OK)return fail(error,"Dungeon light edit enable failed status="+std::to_string(status));
    MT2FW_MT2_RouteReceipt receipt{sizeof receipt};if(!quest_list::read_route(state.image+ui_code.rva,receipt,error))return false;generation=receipt.generation;if(!code(memory,error))return false;ready=true;return true;
}
void tick(){if(!pending)return;const auto saved=*pending;pending.reset();std::string error;a::Binding binding;
    if(!ready||fault||saved.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(saved.snapshot,binding,error)||binding!=saved.binding){fault=true;reject(error.empty()?"Dungeon light captured world changed":error);return;}
    const auto id=action_runtime::uuid(error);if(id.empty()||!typed_action_runtime::submit_dungeon_light(id,saved.intent,error)){fault=true;reject(error);return;}++submitted;last_action=id;
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    if(!ready||fault||GetCurrentThreadId()!=state.thread||request.action!=a::Action::SetDungeonLighting){error="Dungeon light edit runtime unavailable";return a::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="Dungeon light edit already attempted; no replay";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();if(!controller.value("pending",false)||controller.value("locked",true)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Dungeon light edit requires exact pending Controller";return a::Outcome::RejectedNoEffect;}
    Memory memory;Sample before;const auto& intent=request.dungeon_light;std::string prior_sha;
    if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,intent,before,error))return a::Outcome::RejectedNoEffect;
    if(!quest_bridge::sha256(json(before.value.light).dump(),prior_sha,error)||prior_sha!=intent.before_sha256){error=error.empty()?"Dungeon light captured prior authored fields differ":error;return a::Outcome::RejectedNoEffect;}
    if(!a::dungeon_light_wire_fits(request,before.value,before.dungeon.money,error))return a::Outcome::RejectedNoEffect;
    if(authority){auto portable=before.value;portable.dungeon.global_random=authority->dungeon_light_before.dungeon.global_random;if(portable!=authority->dungeon_light_before||before.dungeon.money!=authority->before){error="Dungeon light Client actual prior owner/census/ledger differs from Host";return a::Outcome::RejectedNoEffect;}}
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;
    const auto unknown=[&](const std::string& why){replaying=false;replay_receiver=0;error=op.cause=why.empty()?"Dungeon light edit failed without cause":why;return a::Outcome::Unknown;};
    uintptr_t outer=0,owner=0;a::DungeonLightInput local_input;dungeon_light_input::Widgets widgets;
    if(!window(memory,outer,owner,local_input,widgets,error))return unknown(error);
    op.prepared=true;const auto seh=prepare(outer,before.dungeon.local.dungeon,intent.input,widgets);if(seh)return unknown("Dungeon light original raw UI input preparation SEH="+std::to_string(seh));
    dungeon_light_input::Widgets prepared_widgets;std::array<float,23> expected;
    if(!window(memory,outer,owner,local_input,prepared_widgets,error)||owner!=before.dungeon.local.dungeon||prepared_widgets!=widgets||local_input!=intent.input||!calculate(local_input,widgets,expected,error)||expected!=intent.light)return unknown(error.empty()?"Dungeon light prepared actual inputs/original numeric result differ":error);
    Sample prepared;if(!sample(memory,binding,request.snapshot_uuid,intent,prepared,error))return unknown(error);auto portable=prepared.value;portable.dungeon.global_random=before.value.dungeon.global_random;
    if(!same_local(before,prepared)||portable!=before.value||prepared.dungeon.money!=before.dungeon.money)return unknown("Dungeon light local UI preparation changed gameplay");
    const auto desired_rng=authority?authority->dungeon_light_before.dungeon.global_random:before.value.dungeon.global_random;op.seed_attempted=true;const auto seed=action_runtime::seed(state.image,desired_rng);if(seed)return unknown("Dungeon light Host RNG adoption SEH="+std::to_string(seed));
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.before,error)||op.before.value.dungeon.global_random!=desired_rng)return unknown(error.empty()?"Dungeon light Host RNG readback differs":error);
    op.evidence.before=op.before.dungeon.money;op.evidence.dungeon_light_before=op.before.value;
    a::ui_diagnostic::Api api;a::ui_diagnostic::Window command_window;command_window.window=outer;if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return unknown(error);api.command=reinterpret_cast<a::ui_diagnostic::UiCommand>(state.image+0x6d4380);api.destroy_label="MainLight";
    a::ui_diagnostic::Receipt receipt;op.attempted=true;replaying=true;replay_receiver=outer;original_calls=0;const bool returned=a::ui_diagnostic::process_calls().command(api,command_window,receipt,error);op.calls=original_calls;op.returned=receipt.ui_command_returned;replaying=false;replay_receiver=0;
    if(!returned||!op.returned||op.calls!=1||!receipt.record_cleanup_confirmed)return unknown(error.empty()?"Dungeon light original transaction lacks exact once/cleanup receipt":error);
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.after,error)||!same_local(op.before,op.after)||(!a::dungeon_light_transition(intent,op.before.value,op.after.value)||op.before.dungeon.money!=op.after.dungeon.money||(op.after.environment&&op.after.dirty!=1)))return unknown(error.empty()?"Dungeon light original effect/owner/census/ledger/Undo differs":error);
    op.evidence.after=op.after.dungeon.money;op.evidence.dungeon_light_after=op.after.value;if(authority){auto expected=*authority;expected.retirement_confirmed=false;if(op.evidence!=expected)return unknown("Dungeon light Client original result differs from Host");}
    op.confirmed=true;return a::Outcome::Confirmed;
}catch(const std::exception& exception){replaying=false;replay_receiver=0;error=std::string("Dungeon light perform boundary: ")+exception.what();return a::Outcome::Unknown;}catch(...){replaying=false;replay_receiver=0;error="Dungeon light perform non-standard exception";return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& result,std::string& error){const auto found=operations.find(request.action_uuid);
    if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.returned||found->second.calls!=1)return fail(error,"Dungeon light observation lacks exact confirmed original-once receipt");
    const auto& op=found->second;Memory memory;Sample now;if(!sample(memory,binding,request.snapshot_uuid,request.dungeon_light,now,error)||!same_local(op.after,now)||now.value!=op.after.value)return fail(error,error.empty()?"Dungeon light current result differs before ACK":error);result=op.evidence;result.retirement_confirmed=op.retired;return true;}
bool retire(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence ignored;if(!observe(binding,request,ignored,error))return false;auto& op=operations.at(request.action_uuid);if(op.retired)return fail(error,"Dungeon light receipt already retired");op.retired=true;return true;}
json status(){json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"prepared",op.prepared},{"attempted",op.attempted},{"returned",op.returned},{"original_calls",op.calls},{"confirmed",op.confirmed},{"retired",op.retired},{"seed_attempted",op.seed_attempted},{"environment_present",op.after.environment!=0},{"environment_dirty_before",op.before.dirty},{"environment_dirty_after",op.after.dirty},{"cause",op.cause}};return {{"enabled",ready},{"fault",fault},{"pending_ui",pending_ui()},{"captures",captures},{"submitted",submitted},{"rejected",rejected},{"last_action",last_action},{"cause",cause},{"operations",rows}};}
}
