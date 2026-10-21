namespace dungeon_palette {
namespace {
struct Text {const char* data;uint64_t size,capacity=0,padding=0;};
using Command=bool(*)(void*,const Text&);
Command original=nullptr;uint64_t generation=0,captures=0,submitted=0,rejected=0;bool installed=false,ready=false,fault=false,replaying=false;std::string cause,last_action;
constexpr npc_quest_list_ui_codes::Code ui_code{0x59e990,308,"317278efbc554ccc2a7faa8ea978a467e38e22ea96e20b6cee43034e9838eafe",
    {0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xc0,0x00,0x00,0x00,0x48,0x8d,0x74,0x24,0x40,0x48,0x89,0xd7,0x48,0x8d,0x5c,0x24,0x20,0x48,0x89,0xcd,0x48,0x89,0xf1}};
struct Sample {dungeon_sound::Sample dungeon;a::DungeonPaletteState value;dungeon_layout_reader::Local local;uint64_t dungeon_uid=0;};
struct Pending {a::Binding binding;std::string snapshot;a::DungeonPaletteIntent intent;};
struct Operation {a::Binding binding;a::Request request;Sample before,after;a::Evidence evidence;dungeon_palette_record::Receipt native;bool seed_attempted=false,confirmed=false,retired=false;std::string cause;};
std::optional<Pending> pending;std::map<std::string,Operation> operations;
bool entry(void*,const Text&);
bool reject(const std::string& error){++rejected;if(cause!=error){cause=error;LOG_ERROR("[DungeonPalette] {}",error);}return false;}
bool code(Memory& memory,std::string& error){
    std::string bytes(ui_code.bytes,'\0'),hash;if(!memory.copy(state.image+ui_code.rva,bytes.data(),bytes.size(),error))return false;
    if(original){MT2FW_MT2_RouteReceipt receipt{sizeof receipt};const npc_quest_list_ui_codes::Owned owner{uintptr_t(&entry),uintptr_t(original),generation};
        if(!quest_list::read_route(state.image+ui_code.rva,receipt,error)||!npc_quest_list_ui_codes::receipt(receipt,state.image+ui_code.rva,ui_code,owner,error)||std::memcmp(bytes.data(),receipt.current,32))return fail(error,error.empty()?"Dungeon palette exact owned route differs":error);
        std::array<unsigned char,19> trampoline{};uintptr_t resume=0;constexpr std::array<unsigned char,6> continuation{0xff,0x25,0,0,0,0};
        if(!memory.copy(uintptr_t(original),trampoline.data(),trampoline.size(),error))return false;std::memcpy(&resume,trampoline.data()+11,8);
        if(!std::equal(ui_code.prefix.begin(),ui_code.prefix.begin()+5,trampoline.begin())||!std::equal(continuation.begin(),continuation.end(),trampoline.begin()+5)||resume!=state.image+ui_code.rva+5)return fail(error,"Dungeon palette exact original five-byte instruction boundary differs");
        std::memcpy(bytes.data(),receipt.pristine,32);if(!quest_list::verify_code(state.image+ui_code.rva,bytes.data(),bytes.size(),error))return false;}
    if(!quest_bridge::sha256(bytes,hash,error)||hash!=ui_code.sha)return fail(error,error.empty()?"Dungeon palette original outer command body differs":error);
    for(const auto& pin:dungeon_palette_codes::helpers){bytes.assign(pin.size,'\0');if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error))return false;if(pin.rva==0x6a1780&&!quest_list::checkbox_original(state.image+pin.rva,bytes,error))return false;if(!hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error))return fail(error,"Dungeon palette original explicit-owner helper body differs RVA="+std::to_string(pin.rva)+": "+error);}
    uint32_t factor=0;if(!field(memory,state.image,0x159509c,factor,error))return false;if(factor!=0x437f0000)return fail(error,"Dungeon palette original packed-color factor differs");return true;
}
bool sample(Memory& memory,const a::Binding& binding,const std::string& snapshot,const a::DungeonPaletteIntent& intent,Sample& out,std::string& error){
    out={};a::DungeonSoundIntent target;target.key=intent.key;if(!dungeon_sound::sample(memory,binding,snapshot,target,out.dungeon,error))return false;out.value.protected_state=out.dungeon.value;
    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& e){return field(memory,base,offset,value,e);};
    auto text=[&](uintptr_t base,size_t offset,std::string& value,std::string& e){return npc_quest_records::read_string(memory,base,offset,"Dungeon native cell Theme",value,e);};
    dungeon_layout_reader::Record layout;uint8_t defaults=2;uintptr_t ap=0;const auto owner=out.dungeon.dungeon.dungeon.local.dungeon;
    if(!field(memory,owner,0x18,out.dungeon_uid,error)||!field(memory,owner,0x1a0,ap,error)||!field(memory,owner,0x1d0,defaults,error)||!dungeon_layout_reader::read(at,text,state.image,owner,layout,out.local,error))return false;
    if(!out.dungeon_uid||ap!=state.image+0x172bf80||defaults>1)return fail(error,"Dungeon palette actual Dungeon full UID/ColorConfig class/default flag differs");
    out.value.defaults=defaults!=0;out.value.cells=std::move(layout.cells);out.value.palette=std::move(layout.palette);return true;
}
bool same_local(const Sample& before,const Sample& after){return dungeon_sound::same_local(before.dungeon,after.dungeon)&&before.local==after.local&&before.dungeon_uid==after.dungeon_uid;}
bool digest(const a::DungeonPaletteState& value,std::string& hash,std::string& error){json cells=json::array();for(const auto& c:value.cells)cells.push_back({{"theme",c.theme},{"colors",c.colors},{"open",c.open}});return quest_bridge::sha256(json{{"cells",cells},{"palette",value.palette},{"defaults",value.defaults}}.dump(),hash,error);}
uint32_t pack_native(uintptr_t slot,const std::array<float,4>* color)noexcept{__try{reinterpret_cast<void(*)(void*,const void*)>(state.image+0x972960)(reinterpret_cast<void*>(slot),color);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool pack(const std::array<std::array<float,4>,4>& colors,std::array<uint32_t,4>& packed,std::string& error){for(size_t i=0;i<4;++i){const auto seh=pack_native(uintptr_t(&packed[i]),&colors[i]);if(seh)return fail(error,"Dungeon palette original local packed-color conversion SEH="+std::to_string(seh));}return true;}
uint32_t find_tile(uintptr_t& cursor)noexcept{__try{const Text name{"@DungeonTile",12};cursor=reinterpret_cast<uintptr_t(*)(const Text&)>(state.image+0x589dd0)(name);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t shard_dungeon(uintptr_t shard,uintptr_t& owner)noexcept{__try{owner=reinterpret_cast<uintptr_t(*)(void*)>(state.image+0x507ae0)(reinterpret_cast<void*>(shard));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t sync_input(const dungeon_palette_record::Input* input)noexcept{__try{
    if(input->reset_checkbox)reinterpret_cast<void(*)(void*,bool)>(state.image+0x6a1780)(reinterpret_cast<void*>(input->checkbox),false);
    if(input->refresh_swatches)for(size_t i=0;i<4;++i)reinterpret_cast<void(*)(void*,const void*,bool)>(state.image+0x839d30)(reinterpret_cast<void*>(input->swatches[i]),&input->colors[i],false);return 0;
}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
struct FlagReceipt {bool constructed=false,label=false,token=false,loaded=false,destroyed=false;uint32_t cleanup=0;};
uint32_t load_flag(uintptr_t slot,bool desired,FlagReceipt& receipt)noexcept{
    a::ui_diagnostic::NativeRecordStorage record{};uint32_t exception=0;
    __try{reinterpret_cast<void(*)(void*,const char*)>(state.image+0x967370)(&record,desired?"useDefault true":"useDefault false");receipt.constructed=true;
        receipt.label=reinterpret_cast<bool(*)(const void*,const char*)>(state.image+0x1113d80)(&record,"useDefault");
        if(receipt.label&&*reinterpret_cast<const int32_t*>(record.bytes.data()+0x40)==1){
            const auto token=reinterpret_cast<void*(*)(const void*,int32_t)>(state.image+0x967740)(&record,0);
            const auto text=reinterpret_cast<const Text*(*)(const void*)>(state.image+0x96d880)(token);
            receipt.token=text&&text->data&&text->size==(desired?4u:5u)&&std::memcmp(text->data,desired?"true":"false",size_t(text->size))==0;
            if(receipt.token)receipt.loaded=reinterpret_cast<bool(*)(void*,void*,void*)>(state.image+0x8a1c70)(&record,reinterpret_cast<void*>(slot),nullptr);
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){exception=GetExceptionCode();}
    if(receipt.constructed){__try{reinterpret_cast<void(*)(void*)>(state.image+0x9676a0)(&record);receipt.destroyed=true;}__except(EXCEPTION_EXECUTE_HANDLER){receipt.cleanup=GetExceptionCode();}}
    return exception;
}
uint32_t get_item(uintptr_t array,int32_t index,uintptr_t& slot)noexcept{__try{slot=reinterpret_cast<uintptr_t(*)(const void*,int32_t)>(state.image+0xe3ff80)(reinterpret_cast<void*>(array),index);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool entry(void* receiver,const Text& command)try{
    if(!original){fault=true;return reject("Dungeon palette original unavailable");}if(!net::session_wire::connected())return original(receiver,command);
    Memory memory;std::string error,text;if(!npc_quest_records::read_string(memory,uintptr_t(&command),0,"Dungeon Tile command",text,error))return reject(error);
    const auto label=text.substr(0,text.find_first_of(" \t\r\n"));if(label!="UpdateColors"&&label!="UpdateOptions")return original(receiver,command);
    if(text!=label)return reject("Dungeon palette normal command has unexpected arguments");
    const auto controller=typed_action_runtime::controller_status();a::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!ready||fault||pending||replaying||building_status::pending_ui()||quest_list::pending_ui()||character_rename::pending_ui()||ability_edit::pending_ui()||costume_edit::pending_ui()||dungeon_config::pending_ui()||dungeon_light::pending_ui()||dungeon_gizmo::pending_ui()||dungeon_sound::pending_ui()||GetCurrentThreadId()!=state.thread||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||!code(memory,error)||!typed_action_runtime::network_binding(snapshot,binding,error))return reject(error.empty()?"Dungeon palette capture requires idle bound Mainthread Controller":error);
    uintptr_t cursor=0,manager=0,current=0,owner=0,ap=0;int32_t type=-1;std::string scene;const auto found=find_tile(cursor);
    if(found||cursor!=uintptr_t(receiver)||!dungeon_scene_runtime::cursor_scene(scene,error)||!scene.ends_with("/template")||!field(memory,state.image,0x13df440,manager,error)||!field(memory,manager,0,ap,error)||ap!=state.image+0x1702b00||!field(memory,manager,0x60,current,error)||!field(memory,current,0,ap,error)||ap!=state.image+0x174ad20||!field(memory,current,0x48,type,error)||type!=1)return reject(error.empty()?"Dungeon palette normal registered Tile/template scene differs SEH="+std::to_string(found):error);
    const auto owned=shard_dungeon(current,owner);if(owned||!owner)return reject("Dungeon palette actual current template owner unavailable SEH="+std::to_string(owned));
    a::DungeonPaletteIntent intent;intent.key=scene.substr(0,scene.size()-9);intent.edit=label=="UpdateColors"?a::DungeonPaletteEdit::Colors:a::DungeonPaletteEdit::Options;
    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& e){return field(memory,base,offset,value,e);};dungeon_palette_record::Input input;
    Sample before;if(!dungeon_palette_record::input(at,state.image,cursor,intent.edit,input,error)||!sample(memory,binding,snapshot,intent,before,error)||before.dungeon.dungeon.dungeon.local.dungeon!=owner||!digest(before.value,intent.before_sha256,error))return reject(error.empty()?"Dungeon palette captured actual owner differs":error);
    intent.defaults=input.defaults;intent.colors=input.colors;if(!pack(intent.colors,intent.packed,error))return reject(error);
    pending=Pending{binding,snapshot,intent};++captures;
    // These already bound UI-only setters preserve the immediate normal Checkbox/Swatch feedback. Gameplay waits for the ordered adapter.
    const auto synced=sync_input(&input);if(synced){pending.reset();fault=true;return reject("Dungeon palette normal UI-only feedback SEH="+std::to_string(synced));}return true;
}catch(const std::exception& exception){fault=true;return reject(std::string("Dungeon palette capture boundary: ")+exception.what());}catch(...){fault=true;return reject("Dungeon palette capture non-standard exception");}
}
bool pending_ui(){return pending.has_value()||replaying||dungeon_cells::pending_ui();}
bool retire_lobby(std::string& e){
    if(!dungeon_cells::retire_lobby(e))return false;
    if(!ready){e.clear();return true;}if(GetCurrentThreadId()!=state.thread)return fail(e,"Dungeon palette lobby retirement requires Mainthread");native::WorldStatus world;if(!native::read_world_status(world,e))return false;
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0)return fail(e,"Dungeon palette lobby retirement requires actual idle Title");if(replaying)return fail(e,"Dungeon palette lobby retirement cannot interrupt original execution");
    pending.reset();operations.clear();fault=false;cause.clear();last_action.clear();e.clear();return true;
}
bool enable(std::string& error){
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(error,"Dungeon palette requires bound Character Mainthread");
    if(ready){for(const auto& [id,op]:operations)if(!op.retired)return fail(error,"Dungeon palette retained unretired operation="+id);if(fault||pending_ui())return fail(error,"Dungeon palette retained unresolved capture");Memory memory;if(!code(memory,error)||!dungeon_cells::enable(error))return false;operations.clear();return true;}
    if(installed)return fail(error,"Dungeon palette earlier installation failed: "+cause);installed=true;Memory memory;if(!code(memory,error))return false;
    auto status=MH_CreateHook(reinterpret_cast<void*>(state.image+ui_code.rva),reinterpret_cast<void*>(&entry),reinterpret_cast<void**>(&original));if(status!=MH_OK||!original)return fail(error,"Dungeon palette hook prepare failed status="+std::to_string(status)+" cause="+mt2mp::hooks::framework::process_backend().error());
    status=MH_EnableHook(reinterpret_cast<void*>(state.image+ui_code.rva));if(status!=MH_OK)return fail(error,"Dungeon palette enable failed status="+std::to_string(status));MT2FW_MT2_RouteReceipt receipt{sizeof receipt};if(!quest_list::read_route(state.image+ui_code.rva,receipt,error))return false;generation=receipt.generation;if(!code(memory,error))return false;ready=true;return dungeon_cells::enable(error);
}
void tick(){dungeon_cells::tick();if(!pending)return;const auto saved=*pending;pending.reset();std::string error;a::Binding binding;if(!ready||fault||saved.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(saved.snapshot,binding,error)||binding!=saved.binding){fault=true;reject(error.empty()?"Dungeon palette captured world changed":error);return;}
    const auto id=action_runtime::uuid(error);if(id.empty()||!typed_action_runtime::submit_dungeon_palette(id,saved.intent,error)){fault=true;reject(error);return;}++submitted;last_action=id;
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    if(request.dungeon_palette.edit==a::DungeonPaletteEdit::Cells)return dungeon_cells::perform(binding,request,authority,error);
    if(!ready||fault||GetCurrentThreadId()!=state.thread||request.action!=a::Action::SetDungeonPalette){error="Dungeon palette runtime unavailable";return a::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="Dungeon palette already attempted; no replay";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();if(!controller.value("pending",false)||controller.value("locked",true)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Dungeon palette requires exact pending Controller";return a::Outcome::RejectedNoEffect;}
    Memory memory;Sample before;std::string prior;std::array<uint32_t,4> packed{};const auto& intent=request.dungeon_palette;
    if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,intent,before,error)||!digest(before.value,prior,error)||!pack(intent.colors,packed,error))return a::Outcome::RejectedNoEffect;
    if(prior!=intent.before_sha256||packed!=intent.packed){error="Dungeon palette captured prior cells/palette or original packed conversion differs";return a::Outcome::RejectedNoEffect;}
    if(!a::dungeon_palette_wire_fits(request,before.value,before.dungeon.dungeon.dungeon.money,error))return a::Outcome::RejectedNoEffect;
    if(authority){auto portable=before.value;portable.protected_state.dungeon.global_random=authority->dungeon_palette_before.protected_state.dungeon.global_random;if(portable!=authority->dungeon_palette_before||before.dungeon.dungeon.dungeon.money!=authority->before){error="Dungeon palette Client actual prior owner/cells/config/ledger differs from Host";return a::Outcome::RejectedNoEffect;}}
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;
    const auto unknown=[&](const std::string& why){replaying=false;error=op.cause=why.empty()?"Dungeon palette failed without cause":why;return a::Outcome::Unknown;};
    const auto desired_rng=authority?authority->dungeon_palette_before.protected_state.dungeon.global_random:before.value.protected_state.dungeon.global_random;op.seed_attempted=true;const auto seed=action_runtime::seed(state.image,desired_rng);if(seed)return unknown("Dungeon palette Host RNG adoption SEH="+std::to_string(seed));
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.before,error)||op.before.value.protected_state.dungeon.global_random!=desired_rng)return unknown(error.empty()?"Dungeon palette Host RNG readback differs":error);
    op.evidence.before=op.before.dungeon.dungeon.dungeon.money;op.evidence.dungeon_palette_before=op.before.value;
    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& e){return field(memory,base,offset,value,e);};
    auto flag=[&](uintptr_t slot,bool desired,std::string& e){FlagReceipt receipt;const auto seh=load_flag(slot,desired,receipt);if(seh||receipt.cleanup||!receipt.constructed||!receipt.label||!receipt.token||!receipt.loaded||!receipt.destroyed)return fail(e,"Dungeon palette original bool loader/record lifetime differs SEH="+std::to_string(seh)+" cleanup="+std::to_string(receipt.cleanup));return true;};
    auto item=[&](uintptr_t array,int32_t index,uintptr_t& slot,std::string& e){const auto seh=get_item(array,index,slot);return !seh||fail(e,"Dungeon palette original GetItem SEH="+std::to_string(seh));};
    auto color=[&](uintptr_t slot,const std::array<float,4>& rgba,std::string& e){const auto seh=pack_native(slot,&rgba);return !seh||fail(e,"Dungeon palette original ColorPacked constructor SEH="+std::to_string(seh));};
    replaying=true;const bool returned=dungeon_palette_record::apply(at,flag,item,color,state.image,op.before.dungeon.dungeon.dungeon.local.dungeon,op.before.local.palette_data,op.before.local.palette_count,intent,op.native,error);replaying=false;
    if(!returned||!op.native.returned||op.native.flag_calls!=1||op.native.color_calls!=4)return unknown(error.empty()?"Dungeon palette native adapter lacks exact once/readback receipt":error);
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.after,error)||!same_local(op.before,op.after)||!a::dungeon_palette_transition(intent,op.before.value,op.after.value)||op.before.dungeon.dungeon.dungeon.money!=op.after.dungeon.dungeon.dungeon.money)return unknown(error.empty()?"Dungeon palette native result/cells/owner/config/ledger/Undo differs":error);
    op.evidence.after=op.after.dungeon.dungeon.dungeon.money;op.evidence.dungeon_palette_after=op.after.value;if(authority){auto expected=*authority;expected.retirement_confirmed=false;if(op.evidence!=expected)return unknown("Dungeon palette Client original-leaf result differs from Host");}op.confirmed=true;return a::Outcome::Confirmed;
}catch(const std::exception& exception){replaying=false;error=std::string("Dungeon palette perform boundary: ")+exception.what();return a::Outcome::Unknown;}catch(...){replaying=false;error="Dungeon palette perform non-standard exception";return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& result,std::string& error){if(request.dungeon_palette.edit==a::DungeonPaletteEdit::Cells)return dungeon_cells::observe(binding,request,result,error);const auto found=operations.find(request.action_uuid);if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.native.returned||found->second.native.flag_calls!=1||found->second.native.color_calls!=4)return fail(error,"Dungeon palette observation lacks exact confirmed adapter-once receipt");const auto& op=found->second;Memory memory;Sample now;if(!sample(memory,binding,request.snapshot_uuid,request.dungeon_palette,now,error)||!same_local(op.after,now)||now.value!=op.after.value)return fail(error,error.empty()?"Dungeon palette current result differs before ACK":error);result=op.evidence;result.retirement_confirmed=op.retired;return true;}
bool retire(const a::Binding& binding,const a::Request& request,std::string& error){if(request.dungeon_palette.edit==a::DungeonPaletteEdit::Cells)return dungeon_cells::retire(binding,request,error);a::Evidence ignored;if(!observe(binding,request,ignored,error))return false;auto& op=operations.at(request.action_uuid);if(op.retired)return fail(error,"Dungeon palette receipt already retired");op.retired=true;return true;}
json status(){json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"adapter_attempted",op.native.attempted},{"adapter_returned",op.native.returned},{"original_flag_calls",op.native.flag_calls},{"original_color_calls",op.native.color_calls},{"confirmed",op.confirmed},{"retired",op.retired},{"seed_attempted",op.seed_attempted},{"cause",op.cause}};return {{"enabled",ready},{"cells",dungeon_cells::status()},{"fault",fault},{"pending_ui",pending_ui()},{"captures",captures},{"submitted",submitted},{"rejected",rejected},{"last_action",last_action},{"cause",cause},{"operations",rows}};}
}

#include "dungeon_cells_sample.inc"
#include "dungeon_cells_graph.inc"
#include "dungeon_authored_sample.inc"
#include "dungeon_building_sample.inc"
#include "dungeon_cells_port.inc"
#include "dungeon_cells_runtime.inc"
#include "dungeon_cells_history.inc"
