namespace dungeon_gizmo {
namespace {
using Command=a::ui_diagnostic::UiCommand;
Command original=nullptr;uint64_t generation=0,captures=0,submitted=0,rejected=0;
bool installed=false,ready=false,fault=false,replaying=false,callback_failed=false;std::string cause,last_action;
uintptr_t replay_receiver=0;uint32_t original_calls=0;a::DungeonGizmoEdit replay_edit=a::DungeonGizmoEdit::Gold;int32_t replay_value=0;
const a::DungeonGizmoRecord* preparation=nullptr;bool preparation_door=false;
dungeon_gizmo_identity::Member state_callback_target;
constexpr npc_quest_list_ui_codes::Code ui_code{0x6f4930,2071,"06943e49c74a354ab951ca86dd1939140ee6677cbb436b660a524954cd557e08",
    {0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0x10,0x02,0x00,0x00,0x48,0x89,0xcb,0x48,0x8b,0x89,0xf8,0x04,0x00,0x00,0x48,0x89,0xd6,0x48,0x85,0xc9,0x0f,0x84,0x9d}};
struct Sample {dungeon_palette::Sample dungeon;dungeon_scene_identity_runtime::Census census;a::DungeonGizmoState value;dungeon_gizmo_identity::Member target;};
struct Pending {a::Binding binding;std::string snapshot;a::DungeonGizmoIntent intent;};
struct Operation {a::Binding binding;a::Request request;Sample before,after;a::Evidence evidence;bool attempted=false,returned=false,confirmed=false,retired=false,seed_attempted=false,restored=false,refreshed=false;uint32_t calls=0;std::string cause;};
std::optional<Pending> pending;std::map<std::string,Operation> operations;
bool entry(void*,void*);
bool reject(const std::string& error){if(replaying)callback_failed=true;++rejected;if(cause!=error){cause=error;LOG_ERROR("[DungeonGizmo] {}",error);}return false;}
bool code(Memory& memory,std::string& error){
    live_host_state::apply_timing::PageReadScope pages(!live_host_state::apply_timing::PageReadScope::reader_active());
    static thread_local std::array<hooks::framework::CodeBodyDigest,1+std::size(dungeon_gizmo_codes::helpers)+std::size(dungeon_gizmo_live_codes::values)> digests;
    size_t slot=0;
    std::string bytes(ui_code.bytes,'\0'),hash;if(!memory.copy(state.image+ui_code.rva,bytes.data(),bytes.size(),error))return false;
    if(original){MT2FW_MT2_RouteReceipt receipt{sizeof receipt};const npc_quest_list_ui_codes::Owned owner{uintptr_t(&entry),uintptr_t(original),generation};
        if(!quest_list::read_route(state.image+ui_code.rva,receipt,error)||!npc_quest_list_ui_codes::receipt(receipt,state.image+ui_code.rva,ui_code,owner,error)||std::memcmp(bytes.data(),receipt.current,32))return fail(error,error.empty()?"Gizmo exact owned route differs":error);
        std::array<unsigned char,19> trampoline{};uintptr_t resume=0;constexpr std::array<unsigned char,6> continuation{0xff,0x25,0,0,0,0};
        if(!memory.copy(uintptr_t(original),trampoline.data(),trampoline.size(),error))return false;std::memcpy(&resume,trampoline.data()+11,8);
        if(!std::equal(ui_code.prefix.begin(),ui_code.prefix.begin()+5,trampoline.begin())||!std::equal(continuation.begin(),continuation.end(),trampoline.begin()+5)||resume!=state.image+ui_code.rva+5)return fail(error,"Gizmo exact original five-byte instruction boundary differs");
        std::memcpy(bytes.data(),receipt.pristine,32);if(!quest_list::verify_code(state.image+ui_code.rva,bytes.data(),bytes.size(),error))return false;}
    const auto digest=[&](uintptr_t entry,const char* expected){return digests[slot++].read(entry,bytes.data(),bytes.size(),expected,hash,error,
        [&](std::string& value,std::string& cause){return quest_bridge::sha256(bytes,value,cause);});};
    if(!digest(state.image+ui_code.rva,ui_code.sha)||hash!=ui_code.sha)return fail(error,error.empty()?"Gizmo original outer UI body differs":error);
    for(const auto& pin:dungeon_gizmo_codes::helpers){bytes.assign(pin.size,'\0');if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error)||!digest(state.image+pin.rva,pin.sha))return false;
        if(hash!=pin.sha)return fail(error,"Gizmo original UI/weak/feature/token helper differs RVA="+std::to_string(pin.rva));}
    for(const auto& pin:dungeon_gizmo_live_codes::values){bytes.assign(pin.bytes,'\0');if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error)||
        !hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error,&digests[slot++]))return fail(error,"Gizmo original State/animation helper differs RVA="+std::to_string(pin.rva)+" original="+error);}
    return true;
}
bool protected_digest(const a::DungeonPaletteState& value,std::string& hash,std::string& error){
    json cells=json::array();for(const auto& cell:value.cells)cells.push_back({{"theme",cell.theme},{"colors",cell.colors},{"open",cell.open}});
    const auto& d=value.protected_state.dungeon;json instances=json::array();for(const auto& instance:d.instances)instances.push_back({{"index",instance.index},{"started",instance.started}});
    return quest_bridge::sha256(json{{"desired",d.desired},{"price",d.price},{"sandbox",d.sandbox},{"instances",instances},{"undo_count",d.undo_count},
        {"light",value.protected_state.light},{"atmosphere",value.protected_state.atmosphere},{"cells",cells},{"palette",value.palette},{"defaults",value.defaults}}.dump(),hash,error);
}
bool digest(const a::DungeonGizmoState& value,std::string& hash,std::string& error){
    auto authored=value;if(authored.live){authored.live.reset();authored.network.reset();for(auto& scene:authored.scenes)for(auto& row:scene.rows){row.value.triggered=false;row.value.state=0;}}
    return quest_bridge::sha256(dungeon_gizmo_portable::state(authored,false).dump(),hash,error);
}
bool sample(Memory& memory,const a::Binding& binding,const std::string& snapshot,const a::DungeonGizmoIntent& intent,Sample& out,std::string& error){
    out={};if(!dungeon_scene_identity_runtime::read_current(out.census,error)||!dungeon_scene_identity_runtime::lookup(intent.key,out.target,error))return false;
    if(out.target.scene!=intent.scene)return fail(error,"Gizmo exact canonical scene owner differs");
    std::string exterior;size_t slot=0;bool found=false;
    for(const auto& pool:out.census.pools){a::DungeonGizmoScene scene;scene.key=pool.scene.owner.key;
        for(const auto& record:pool.gizmos){if(slot>=out.census.keys.size())return fail(error,"Gizmo complete current key roster truncated");scene.rows.push_back({out.census.keys[slot++],record});}
        if(scene.key==intent.scene){if(found)return fail(error,"Gizmo scene owner is duplicated");found=true;exterior=pool.scene.owner.exterior_key;}
        out.value.scenes.push_back(std::move(scene));
    }
    if(!found||slot!=out.census.keys.size())return fail(error,"Gizmo target scene/current key roster differs");
    a::DungeonPaletteIntent owner;owner.key=exterior;
    if(!dungeon_palette::sample(memory,binding,snapshot,owner,out.dungeon,error)||!protected_digest(out.dungeon.value,out.value.protected_sha256,error))return false;
    out.value.global_random=out.dungeon.value.protected_state.dungeon.global_random;
    if(intent.edit==a::DungeonGizmoEdit::State){a::Context context;dungeon_gizmo_live::State live;
        if(!a::selected_session().current(context,error)||!capture_live(context,binding,live,error))return false;out.value.live=std::move(live);
        live_host_state::NativePort port(context,binding,nullptr);live_host_state::Pin pin;live_host_state::network::State network;
        if(!port.read(pin,error)||!live_host_state::network::read_action105_effect(pin,network,error)||!port.current_owners(pin,error))return false;out.value.network=std::move(network);}
    return true;
}
bool same_local(const Sample& before,const Sample& after){
    const auto& xworld=before.census.world;const auto& yworld=after.census.world;
    if(!dungeon_palette::same_local(before.dungeon,after.dungeon)||before.target!=after.target||
       xworld.game!=yworld.game||xworld.mode!=yworld.mode||xworld.state!=yworld.state||xworld.world!=yworld.world||xworld.map!=yworld.map||
       xworld.generation!=yworld.generation||xworld.world_epoch!=yworld.world_epoch||xworld.uuid!=yworld.uuid||xworld.sha256!=yworld.sha256||before.census.pools.size()!=after.census.pools.size())return false;
    for(size_t i=0;i<before.census.pools.size();++i){const auto& x=before.census.pools[i];const auto& y=after.census.pools[i];
        if(x.scene.owner.key!=y.scene.owner.key||x.managers!=y.managers||x.local!=y.local)return false;}return true;
}
const a::DungeonGizmoRecord* selected(const a::DungeonGizmoState& value,const std::string& key){for(const auto& scene:value.scenes)for(const auto& row:scene.rows)if(row.key==key)return &row.value;return nullptr;}
uint32_t find_window(uintptr_t& out)noexcept{__try{const quest_list::QuestTitleView name{"info_gizmo",10};out=reinterpret_cast<uintptr_t(*)(const quest_list::QuestTitleView&)>(state.image+0x81aa90)(name);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t set_window(uintptr_t window,uintptr_t target)noexcept{__try{if(target)reinterpret_cast<void(*)(void*,void*)>(state.image+0x6f53f0)(reinterpret_cast<void*>(window),reinterpret_cast<void*>(target));else reinterpret_cast<void(*)(void*)>(state.image+0x6f35f0)(reinterpret_cast<void*>(window));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t refresh_window(uintptr_t window)noexcept{__try{reinterpret_cast<void(*)(void*)>(state.image+0x6f3800)(reinterpret_cast<void*>(window));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t is_door(uintptr_t target,bool& door)noexcept{__try{door=reinterpret_cast<bool(*)(uintptr_t)>(state.image+0x145990)(target);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t feature(uintptr_t target,bool& container)noexcept{__try{container=reinterpret_cast<bool(*)(const void*,int32_t)>(state.image+0x145560)(reinterpret_cast<const void*>(target),0);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool window(Memory& memory,dungeon_gizmo_record::Window& out,std::string& error){
    uintptr_t object=0;const auto seh=find_window(object);if(seh||!object)return fail(error,"Gizmo actual named Window unavailable SEH="+std::to_string(seh));host::LookupApi api;
    if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;
    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& e){return field(memory,base,offset,value,e);};
    auto find=[&](uint64_t uid,uintptr_t& actual,std::string& e){const auto lookup_seh=lookup(api.find,uid,actual);return !lookup_seh||fail(e,"Gizmo Window original full UID lookup SEH="+std::to_string(lookup_seh));};
    return dungeon_gizmo_record::window(at,find,state.image,object,out,error);
}
bool container(Memory& memory,const dungeon_gizmo_record::Window& owner,std::string& error){if(!owner.target)return fail(error,"Gizmo container Window has no target");bool content=false;const auto seh=feature(owner.target,content);
    if(seh||!content)return fail(error,"Gizmo original container feature unavailable SEH="+std::to_string(seh));
    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& e){return field(memory,base,offset,value,e);};return dungeon_gizmo_record::grid(at,state.image,owner,error);
}
bool record_value(Memory& memory,void* record,int32_t& value,std::string& error){auto at=[&](uintptr_t base,size_t offset,auto& field_value,std::string& e){return field(memory,base,offset,field_value,e);};return dungeon_gizmo_record::value(at,uintptr_t(record),value,error);}
bool replay_record_equals(const void* record,const char*){
    if(!reinterpret_cast<a::ui_diagnostic::RecordEquals>(state.image+0x1113d80)(record,dungeon_gizmo_record::labels[size_t(replay_edit)]))return false;
    Memory memory;std::string error;int32_t value=0;if(!record_value(memory,const_cast<void*>(record),value,error)){reject(error);return false;}return value==replay_value;
}
bool entry(void* receiver,void* record)try{
    if(!original){fault=true;return reject("Gizmo original unavailable");}if(!net::session_wire::connected()&&!replaying)return original(receiver,record);
    Memory memory;std::string error;a::ui_diagnostic::Api api;if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error)){fault=true;return reject(error);}
    int edit=-1;for(size_t i=0;i<dungeon_gizmo_record::labels.size();++i){bool match=false;const auto seh=quest_list::label(api.label_equals,record,dungeon_gizmo_record::labels[i],match);
        if(seh){fault=true;return reject("Gizmo native command label SEH="+std::to_string(seh));}if(match){edit=int(i);break;}}
    if(edit<0){bool destroy=false;
        const auto seh=quest_list::label(api.label_equals,record,"DestroyGizmo",destroy);
        if(seh)return reject("Gizmo Destroy label read SEH="+std::to_string(seh));
        if(!destroy)return original(receiver,record);
        dungeon_gizmo_record::Window owner;
        if(!window(memory,owner,error)||owner.object!=uintptr_t(receiver)||!owner.target)return reject(error.empty()?"Gizmo Destroy current Window target differs":error);
        return dungeon_input::capture_gizmo_destroy(owner.target,owner.uid,error)||reject(error);
    }
    int32_t value=0;if(!record_value(memory,record,value,error)){if(replaying){callback_failed=true;fault=true;}return reject(error);}
    if(dungeon_input::preparing_gizmo_ui()){
        dungeon_gizmo_record::Window owner;
        if(!window(memory,owner,error)||owner.object!=uintptr_t(receiver)){
            dungeon_input::reject_gizmo_ui();return reject(error.empty()?"Gizmo control UI Window changed":error);}
        if(!owner.target)return original(receiver,record);
        if(!dungeon_input::gizmo_ui_owner(owner.target,owner.uid)){
            dungeon_input::reject_gizmo_ui();return reject(error.empty()?"Gizmo control UI preparation owner differs":error);}
        int32_t current=0;
        if(edit==5){uint8_t trigger=2;bool door=false;
            if(!field(memory,owner.target,0x250,trigger,error)||!field(memory,owner.target,0x254,current,error)){
                dungeon_input::reject_gizmo_ui();return reject(error);}
            const auto seh=is_door(owner.target,door);
            if(trigger>1||seh){dungeon_input::reject_gizmo_ui();return reject("Gizmo control UI trigger/IsDoor invalid SEH="+std::to_string(seh));}
            current=door?current+1:trigger?1:3;
        }else{constexpr std::array<size_t,5> offsets{0x260,0x264,0x268,0x26c,0x270};
            if(!field(memory,owner.target,offsets[size_t(edit)],current,error)){dungeon_input::reject_gizmo_ui();return reject(error);}}
        if(current!=value){dungeon_input::reject_gizmo_ui();return reject("Gizmo control UI preparation changed a native value");}
        return true;
    }
    if(replaying&&!preparation&&replay_edit==a::DungeonGizmoEdit::State&&original_calls==1){
        dungeon_gizmo_record::Window current;
        if(!window(memory,current,error)||current.object!=replay_receiver){callback_failed=true;return reject(error.empty()?"Gizmo State callback Window differs":error);}
        if(!current.target)return original(receiver,record);
        if(current.target!=state_callback_target.object||current.uid!=state_callback_target.uid){callback_failed=true;return reject("Gizmo State callback changed its complete target lifetime");}
        a::DungeonGizmoRecord observed;bool door=false;
        const auto seh=is_door(current.target,door);
        if(seh||!field(memory,current.target,0x250,observed.triggered,error)||!field(memory,current.target,0x254,observed.state,error)||
           !field(memory,current.target,0x260,observed.gold,error)||!field(memory,current.target,0x264,observed.loot,error)||
           !field(memory,current.target,0x268,observed.vendor_trash,error)||!field(memory,current.target,0x26c,observed.keys,error)||!field(memory,current.target,0x270,observed.boss_keys,error)){
            callback_failed=true;return reject(seh?"Gizmo State callback IsDoor SEH="+std::to_string(seh):error);}
        uint32_t preparation_calls=0;bool execute=false;
        if(!dungeon_gizmo_record::replay(uintptr_t(receiver),a::DungeonGizmoEdit(edit),value,replay_receiver,replay_edit,replay_value,&observed,preparation_calls,execute,error,door)){
            callback_failed=true;return reject(error);}
        return true;
    }
    if(replaying){bool execute=false;if(!dungeon_gizmo_record::replay(uintptr_t(receiver),a::DungeonGizmoEdit(edit),value,replay_receiver,replay_edit,replay_value,preparation,original_calls,execute,error,preparation_door)){callback_failed=true;fault=true;return reject(error);}return execute?original(receiver,record):true;}
    const auto controller=typed_action_runtime::controller_status();a::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!ready||fault||pending||building_status::pending_ui()||quest_list::pending_ui()||character_rename::pending_ui()||ability_edit::pending_ui()||costume_edit::pending_ui()||dungeon_config::pending_ui()||dungeon_light::pending_ui()||dungeon_sound::pending_ui()||dungeon_palette::pending_ui()||GetCurrentThreadId()!=state.thread||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||!code(memory,error)||!typed_action_runtime::network_binding(snapshot,binding,error))return reject(error.empty()?"Gizmo capture requires idle bound Mainthread Controller":error);
    dungeon_gizmo_record::Window owner;a::DungeonGizmoIntent intent;intent.edit=a::DungeonGizmoEdit(edit);intent.value=value;
    if(!window(memory,owner,error)||owner.object!=uintptr_t(receiver)||(intent.edit!=a::DungeonGizmoEdit::State&&!container(memory,owner,error))||!dungeon_scene_identity_runtime::resolve(owner.target,owner.uid,intent.key,error))return reject(error.empty()?"Gizmo captured actual native Window/owner differs":error);
    dungeon_gizmo_identity::Member member;if(!dungeon_scene_identity_runtime::lookup(intent.key,member,error))return reject(error);intent.scene=member.scene;
    if(intent.edit==a::DungeonGizmoEdit::State){const auto seh=is_door(owner.target,intent.door);if(seh||intent.value<1||intent.value>3||(!intent.door&&intent.value==2))return reject("Gizmo State requires its actual Door/trigger command domain SEH="+std::to_string(seh));}
    Sample before;if(!sample(memory,binding,snapshot,intent,before,error)||before.target.object!=owner.target||before.target.uid!=owner.uid||!digest(before.value,intent.before_sha256,error))return reject(error.empty()?"Gizmo captured full native identity differs":error);
    pending=Pending{binding,snapshot,intent};++captures;return true;
}catch(const std::exception& exception){fault=true;return reject(std::string("Gizmo capture boundary: ")+exception.what());}catch(...){fault=true;return reject("Gizmo capture non-standard exception");}
void reset_replay(){replaying=false;preparation=nullptr;replay_receiver=0;original_calls=0;callback_failed=false;state_callback_target={};}
}
bool pending_ui(){return pending.has_value()||replaying;}
bool retire_lobby(std::string& e){
    if(!ready){e.clear();return true;}if(GetCurrentThreadId()!=state.thread)return fail(e,"Gizmo lobby retirement requires Mainthread");native::WorldStatus world;if(!native::read_world_status(world,e))return false;
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0)return fail(e,"Gizmo lobby retirement requires actual idle Title");if(replaying)return fail(e,"Gizmo lobby retirement cannot interrupt original execution");
    pending.reset();operations.clear();replaying=false;preparation=nullptr;replay_receiver=0;original_calls=0;callback_failed=false;fault=false;cause.clear();last_action.clear();e.clear();return true;
}
bool enable(std::string& error){
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(error,"Gizmo requires bound Character Mainthread");
    if(ready){for(const auto& [id,op]:operations)if(!op.retired)return fail(error,"Gizmo retained unretired operation="+id);if(fault||pending_ui())return fail(error,"Gizmo retained unresolved capture");Memory memory;if(!code(memory,error))return false;operations.clear();return true;}
    if(installed)return fail(error,"Gizmo earlier installation failed: "+cause);installed=true;Memory memory;if(!code(memory,error))return false;
    auto result=MH_CreateHook(reinterpret_cast<void*>(state.image+ui_code.rva),reinterpret_cast<void*>(&entry),reinterpret_cast<void**>(&original));if(result!=MH_OK||!original)return fail(error,"Gizmo hook prepare failed status="+std::to_string(result)+" cause="+mt2mp::hooks::framework::process_backend().error());
    result=MH_EnableHook(reinterpret_cast<void*>(state.image+ui_code.rva));if(result!=MH_OK)return fail(error,"Gizmo hook enable failed status="+std::to_string(result));MT2FW_MT2_RouteReceipt receipt{sizeof receipt};if(!quest_list::read_route(state.image+ui_code.rva,receipt,error))return false;generation=receipt.generation;if(!code(memory,error))return false;ready=true;return true;
}
void tick(){if(!pending)return;const auto saved=*pending;pending.reset();std::string error;a::Binding binding;if(!ready||fault||saved.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(saved.snapshot,binding,error)||binding!=saved.binding){fault=true;reject(error.empty()?"Gizmo captured world changed":error);return;}
    const auto id=action_runtime::uuid(error);if(id.empty()||!typed_action_runtime::submit_dungeon_gizmo(id,saved.intent,error)){fault=true;reject(error);return;}++submitted;last_action=id;
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    if(!ready||fault||GetCurrentThreadId()!=state.thread||request.action!=a::Action::EditDungeonGizmo){error="Gizmo runtime unavailable";return a::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="Gizmo already attempted; no replay";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();if(!controller.value("pending",false)||controller.value("locked",true)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Gizmo requires exact pending Controller";return a::Outcome::RejectedNoEffect;}
    Memory memory;Sample before;std::string prior;const auto& intent=request.dungeon_gizmo;
    if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,intent,before,error)||!digest(before.value,prior,error))return a::Outcome::RejectedNoEffect;
    bool actual_door=false;if(is_door(before.target.object,actual_door)||(intent.edit==a::DungeonGizmoEdit::State&&actual_door!=intent.door)){error="Gizmo actual native Door feature changed";return a::Outcome::RejectedNoEffect;}
    if(prior!=intent.before_sha256){error="Gizmo captured prior scene/roster/protected state differs";return a::Outcome::RejectedNoEffect;}
    if(!a::dungeon_gizmo_wire_fits(request,before.value,before.dungeon.dungeon.dungeon.dungeon.money,error))return a::Outcome::RejectedNoEffect;
    if(authority){auto portable=before.value;portable.global_random=authority->dungeon_gizmo_before.global_random;if(portable!=authority->dungeon_gizmo_before||before.dungeon.dungeon.dungeon.dungeon.money!=authority->before){error="Gizmo Client actual prior roster/owner/config/ledger differs from Host";return a::Outcome::RejectedNoEffect;}}
    dungeon_gizmo_record::Window saved;if(!window(memory,saved,error))return a::Outcome::RejectedNoEffect;
    a::DungeonGizmoRecord restore_values;bool restore_door=false;
    if(saved.target&&(is_door(saved.target,restore_door)||!field(memory,saved.target,0x250,restore_values.triggered,error)||!field(memory,saved.target,0x254,restore_values.state,error)||!field(memory,saved.target,0x260,restore_values.gold,error)||!field(memory,saved.target,0x264,restore_values.loot,error)||!field(memory,saved.target,0x268,restore_values.vendor_trash,error)||!field(memory,saved.target,0x26c,restore_values.keys,error)||!field(memory,saved.target,0x270,restore_values.boss_keys,error)))return a::Outcome::RejectedNoEffect;
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;bool rebound=false,restore_attempted=false;
    const auto restore=[&](std::string& e){if(!rebound)return true;return dungeon_gizmo_record::restore_once(restore_attempted,[&](std::string& restore_cause){replaying=true;replay_receiver=saved.object;preparation=&restore_values;preparation_door=restore_door;original_calls=0;
        const auto seh=set_window(saved.object,saved.target);const auto failed=callback_failed;reset_replay();if(seh||failed)return fail(restore_cause,seh?"Gizmo original local Window restore SEH="+std::to_string(seh):cause);dungeon_gizmo_record::Window current;
        if(!window(memory,current,restore_cause))return false;if(current.object!=saved.object||current.target!=saved.target||current.uid!=saved.uid)return fail(restore_cause,"Gizmo original local Window restore full UID differs");rebound=false;op.restored=true;return true;},e);};
    const auto unknown=[&](const std::string& why){reset_replay();std::string restore_error;if(!restore(restore_error)){error=op.cause=why+"; "+restore_error;return a::Outcome::Unknown;}
        error=op.cause=why.empty()?"Gizmo original operation failed without cause":why;return a::Outcome::Unknown;};
    const auto* prior_record=selected(before.value,intent.key);if(!prior_record)return unknown("Gizmo target absent from actual prior authored roster");
    rebound=true;replaying=true;replay_receiver=saved.object;preparation=prior_record;preparation_door=actual_door;original_calls=0;const auto setup=set_window(saved.object,before.target.object);reset_replay();
    if(setup||fault)return unknown(setup?"Gizmo original Window preparation SEH="+std::to_string(setup):cause);
    dungeon_gizmo_record::Window prepared_window;if(!window(memory,prepared_window,error)||(intent.edit!=a::DungeonGizmoEdit::State&&!container(memory,prepared_window,error))||prepared_window.object!=saved.object||prepared_window.target!=before.target.object||prepared_window.uid!=before.target.uid)return unknown(error.empty()?"Gizmo original prepared owner differs":error);
    Sample prepared;if(!sample(memory,binding,request.snapshot_uuid,intent,prepared,error))return unknown(error);auto portable=prepared.value;portable.global_random=before.value.global_random;
    if(!same_local(before,prepared)||portable!=before.value||prepared.dungeon.dungeon.dungeon.dungeon.money!=before.dungeon.dungeon.dungeon.dungeon.money)return unknown("Gizmo original Window preparation changed gameplay");
    const auto desired_rng=authority?authority->dungeon_gizmo_before.global_random:before.value.global_random;op.seed_attempted=true;const auto seed=action_runtime::seed(state.image,desired_rng);if(seed)return unknown("Gizmo Host RNG adoption SEH="+std::to_string(seed));
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.before,error)||op.before.value.global_random!=desired_rng)return unknown(error.empty()?"Gizmo Host RNG readback differs":error);
    op.evidence.before=op.before.dungeon.dungeon.dungeon.dungeon.money;op.evidence.dungeon_gizmo_before=op.before.value;
    a::ui_diagnostic::Api api;a::ui_diagnostic::Window command_window;command_window.window=saved.object;if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return unknown(error);
    const auto command=std::string(dungeon_gizmo_record::labels[size_t(intent.edit)])+" "+std::to_string(intent.value);api.command=reinterpret_cast<Command>(state.image+ui_code.rva);api.destroy_label=command.c_str();api.label_equals=&replay_record_equals;
    a::ui_diagnostic::Receipt receipt;op.attempted=true;replaying=true;replay_receiver=saved.object;replay_edit=intent.edit;replay_value=intent.value;original_calls=0;state_callback_target=op.before.target;
    const bool returned=a::ui_diagnostic::process_calls().command(api,command_window,receipt,error);op.calls=original_calls;op.returned=receipt.ui_command_returned;const bool ui_callback_failed=callback_failed;reset_replay();
    if(!returned||!op.returned||op.calls!=1||!receipt.record_cleanup_confirmed||ui_callback_failed||fault)return unknown(error.empty()?(cause.empty()?"Gizmo original transaction lacks exact once/record cleanup":cause):error);
    a::DungeonGizmoState projected;if(!a::dungeon_gizmo_project(intent,op.before.value,projected))return unknown("Gizmo original result projection missing");
    const auto* refreshed=selected(projected,intent.key);if(!refreshed)return unknown("Gizmo original result target missing");auto refreshed_values=*refreshed;
    // Set returns early for an already selected actor. Refresh the original Grid
    // explicitly so a remote command has the same visible values as local input.
    replaying=true;replay_receiver=saved.object;preparation=&refreshed_values;preparation_door=actual_door;original_calls=0;const auto refresh=refresh_window(saved.object);const auto refresh_failed=callback_failed;reset_replay();
    if(refresh||refresh_failed)return unknown(refresh?"Gizmo original content refresh SEH="+std::to_string(refresh):cause);op.refreshed=true;
    if(saved.target==before.target.object){restore_values=refreshed_values;restore_door=actual_door;}
    if(!restore(error))return unknown(error);
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.after,error)||!same_local(op.before,op.after)||!a::dungeon_gizmo_transition(intent,op.before.value,op.after.value)||op.before.dungeon.dungeon.dungeon.dungeon.money!=op.after.dungeon.dungeon.dungeon.dungeon.money)return unknown(error.empty()?"Gizmo original result/scene/owner/config/ledger/Undo differs":error);
    op.evidence.after=op.after.dungeon.dungeon.dungeon.dungeon.money;op.evidence.dungeon_gizmo_after=op.after.value;
    if(authority){auto expected=*authority;expected.retirement_confirmed=false;if(op.evidence!=expected)return unknown("Gizmo Client original result differs from Host");}op.confirmed=true;return a::Outcome::Confirmed;
}catch(const std::exception& exception){reset_replay();error=std::string("Gizmo perform boundary: ")+exception.what();return a::Outcome::Unknown;}catch(...){reset_replay();error="Gizmo perform non-standard exception";return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& result,std::string& error){const auto found=operations.find(request.action_uuid);if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.returned||found->second.calls!=1||!found->second.restored||!found->second.refreshed)return fail(error,"Gizmo observation lacks exact original-once/refreshed/restored receipt");const auto& op=found->second;Memory memory;Sample now;
    if(!sample(memory,binding,request.snapshot_uuid,request.dungeon_gizmo,now,error)||!same_local(op.after,now)||now.value!=op.after.value)return fail(error,error.empty()?"Gizmo current result differs before ACK":error);result=op.evidence;result.retirement_confirmed=op.retired;return true;}
bool retire(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence ignored;if(!observe(binding,request,ignored,error))return false;auto& op=operations.at(request.action_uuid);if(op.retired)return fail(error,"Gizmo receipt already retired");op.retired=true;return true;}
json status(){json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"attempted",op.attempted},{"returned",op.returned},{"calls",op.calls},{"refreshed",op.refreshed},{"restored",op.restored},{"confirmed",op.confirmed},{"retired",op.retired},{"seed_attempted",op.seed_attempted},{"cause",op.cause}};return {{"enabled",ready},{"fault",fault},{"pending_ui",pending_ui()},{"captures",captures},{"submitted",submitted},{"rejected",rejected},{"last_action",last_action},{"cause",cause},{"operations",rows}};}
}

#include "dungeon_gizmo_live_native.inc"
#include "dungeon_gizmo_rename.inc"
