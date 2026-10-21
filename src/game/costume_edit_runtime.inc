namespace costume_edit {
namespace {
using Command=a::ui_diagnostic::UiCommand;
Command original=nullptr;uint64_t generation=0,captures=0,submitted=0,rejected=0;bool installed=false,ready=false,fault=false,replaying=false;
uintptr_t replay_receiver=0;uint32_t original_calls=0;std::string cause,last_action;
constexpr npc_quest_list_ui_codes::Code ui_code{0x6b51c0,4250,"aa46c765e1353ad394627575ec4b7b18b23d9e7e9ea8cda9b31da5a7880c7407",
    {0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0x38,0x02,0,0,0x0f,0x11,0xb4,0x24,0xd0,0x01,0,0,0x0f,0x11,0xbc,0x24,0xe0}};
struct Sample {character_rename::Sample character;uintptr_t costume=0;std::vector<std::array<uintptr_t,3>> local; a::CostumeEditState value;};
struct Pending {a::Binding binding;std::string snapshot;a::CostumeEditIntent intent;};
struct Operation {a::Binding binding;a::Request request;Sample before,after;a::Evidence evidence;bool attempted=false,returned=false,confirmed=false,retired=false,prepared=false,seed_attempted=false;uint32_t calls=0;std::string cause;};
std::optional<Pending> pending;std::map<std::string,Operation> operations;
bool entry(void*,void*);
bool reject(const std::string& error){++rejected;if(cause!=error){cause=error;LOG_ERROR("[CostumeEdit] {}",error);}return false;}
json pose_json(const a::CostumePose& v){return {{"rotation",v.rotation},{"position",v.position},{"scale",v.scale}};}
json record_json(const a::CostumeRecord& v){json parts=json::array();for(const auto& p:v.parts){json attachments=json::array();for(const auto& row:p.attachments)attachments.push_back({{"name",row.name},{"pose",pose_json(row.pose)}});
    parts.push_back({{"bone",p.bone},{"model",p.model},{"pose",pose_json(p.pose)},{"model_scale",p.model_scale},{"attachments",attachments},{"colors",p.colors}});}return {{"name",v.name},{"skeleton",v.skeleton},{"parts",parts},{"flag",v.flag}};}
bool code(Memory& memory,std::string& error){
    std::string bytes(ui_code.bytes,'\0'),hash;if(!memory.copy(state.image+ui_code.rva,bytes.data(),bytes.size(),error))return false;
    if(original){MT2FW_MT2_RouteReceipt receipt{sizeof receipt};const npc_quest_list_ui_codes::Owned owner{uintptr_t(&entry),uintptr_t(original),generation};
        if(!quest_list::read_route(state.image+ui_code.rva,receipt,error)||!npc_quest_list_ui_codes::receipt(receipt,state.image+ui_code.rva,ui_code,owner,error)||std::memcmp(bytes.data(),receipt.current,32))return fail(error,error.empty()?"Costume exact owned route differs":error);
        std::array<unsigned char,20> trampoline{};uintptr_t resume=0;constexpr std::array<unsigned char,6> continuation{0xff,0x25,0,0,0,0};
        if(!memory.copy(uintptr_t(original),trampoline.data(),trampoline.size(),error))return false;std::memcpy(&resume,trampoline.data()+12,8);
        if(!std::equal(ui_code.prefix.begin(),ui_code.prefix.begin()+6,trampoline.begin())||!std::equal(continuation.begin(),continuation.end(),trampoline.begin()+6)||resume!=state.image+ui_code.rva+6)return fail(error,"Costume exact original six-byte instruction boundary differs");
        std::memcpy(bytes.data(),receipt.pristine,32);if(!quest_list::verify_code(state.image+ui_code.rva,bytes.data(),std::min<size_t>(bytes.size(),4096),error))return false;}
    if(!quest_bridge::sha256(bytes,hash,error)||hash!=ui_code.sha)return fail(error,error.empty()?"Costume original outer UI transaction body differs":error);
    for(const auto& pin:costume_edit_codes::helpers){bytes.assign(pin.size,'\0');if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error)||!quest_bridge::sha256(bytes,hash,error))return false;
        if(hash!=pin.sha)return fail(error,"Costume original preparation/ownership helper body differs RVA="+std::to_string(pin.rva));}
    return true;
}
bool read_record(Memory& memory,uintptr_t object,a::CostumeRecord& value,std::string& error){
    auto read=[&](uintptr_t base,size_t offset,auto& out,std::string& e){return field(memory,base,offset,out,e);};
    auto text=[&](uintptr_t address,std::string& out,std::string& e){return npc_quest_records::read_string(memory,address,0,"Costume authored string",out,e);};
    return costume_record_reader::record(read,text,state.image,object,value,error);
}
bool read_categories(Memory& memory,uintptr_t owner,std::vector<std::string>& values,std::string& error){
    auto read=[&](uintptr_t base,size_t offset,auto& value,std::string& e){return field(memory,base,offset,value,e);};
    auto text=[&](uintptr_t address,std::string& value,std::string& e){return npc_quest_records::read_string(memory,address,0,"Costume weapon category",value,e);};
    return costume_weapon_read::categories(read,text,owner+0x38,values,error);
}
bool read_weapon_command(Memory& memory,uintptr_t record,a::CostumeWeaponIntent& intent,std::string& error){
    auto read=[&](uintptr_t base,size_t offset,auto& value,std::string& e){return field(memory,base,offset,value,e);};
    auto text=[&](uintptr_t address,std::string& value,std::string& e){return npc_quest_records::read_string(memory,address,0,"Costume weapon command string",value,e);};
    return costume_weapon_read::command(read,text,record,intent.category,intent.enabled,error);
}
uint32_t weapon_original(uintptr_t owner,const a::CostumeWeaponIntent& intent,uint32_t& calls,bool& returned)noexcept{
    const quest_list::QuestTitleView category{intent.category.c_str(),intent.category.size()};
    __try{++calls;reinterpret_cast<void(*)(void*,const quest_list::QuestTitleView&)>(state.image+(intent.enabled?0x91060:0x910e0))(reinterpret_cast<void*>(owner+0x38),category);returned=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t weapon_dirty(uintptr_t outer)noexcept{__try{reinterpret_cast<void(*)(void*,bool)>(state.image+0x6b1c40)(reinterpret_cast<void*>(outer),true);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool weapon_dirty_readback(Memory& memory,uintptr_t outer,std::string& error){
    for(size_t offset:{size_t(0x328),size_t(0x330)}){uintptr_t button=0;uint8_t disabled=1;
        if(!field(memory,outer,offset,button,error))return false;
        if(button&&(!field(memory,button,0x46d,disabled,error)||disabled))return fail(error,error.empty()?"Costume weapon original dirty-button readback differs":error);}
    return true;
}
a::CharacterRenameIntent owner_intent(const a::CostumeEditIntent& intent){a::CharacterRenameIntent out;out.kind=intent.kind;out.id=intent.id;return out;}
bool sample(Memory& memory,const a::Binding& binding,const std::string& snapshot,const a::CostumeEditIntent& intent,Sample& out,std::string& error){
    out={};const auto owner=owner_intent(intent);if(!character_rename::sample(memory,binding,snapshot,owner,out.character,error))return false;out.value.owner=out.character.value;
    json rows=json::array();std::set<uintptr_t> pointers;bool found=false;
    for(size_t kind=0;kind<5;++kind){const auto& types=out.character.arrays[kind];for(size_t id=0;id<types[1];++id){uintptr_t type=0,object=0;a::CostumeRecord record;
        if(!field(memory,types[0],id*8,type,error)||!field(memory,type,0xf0,object,error)||!object||!pointers.insert(object).second||!read_record(memory,object,record,error))return fail(error,error.empty()?"Costume actual Character-owned record identity differs":error);
        std::vector<std::string> categories;if(!read_categories(memory,type,categories,error))return false;
        out.local.push_back({type,object,0});rows.push_back({{"kind",kind},{"id",id},{"record",record_json(record)},{"weapon_categories",categories}});++out.value.count;
        if(int32_t(kind)==intent.kind&&int32_t(id)==intent.id){found=true;out.costume=object;out.value.record=record;out.value.weapon_categories=std::move(categories);}}}
    if(!found)return fail(error,"Costume canonical Character owner absent");return quest_bridge::sha256(rows.dump(),out.value.all_sha256,error);
}
bool window(Memory& memory,uintptr_t& outer,uintptr_t& live,uintptr_t& draft,uintptr_t& owner,std::string& error){
    uintptr_t named=0,view=0,ap=0;const auto seh=character_rename::find_window(named);
    if(seh||!named||!field(memory,named,0,ap,error)||ap!=state.image+0x173a7f0||!field(memory,named,0x4e8,view,error)||!field(memory,view,0,ap,error)||ap!=state.image+0x1736770||
       !field(memory,view,0x3b0,outer,error)||!outer||!field(memory,outer,0,ap,error)||ap!=state.image+0x1722a70||!field(memory,outer,0x318,live,error)||!field(memory,outer,0x320,draft,error)||!field(memory,outer,0x340,owner,error))return fail(error,error.empty()?"Costume actual named Window/View/editor differs SEH="+std::to_string(seh):error);
    if(draft&&(draft==live||!field(memory,draft,0,ap,error)||ap!=state.image+0x16c0210))return fail(error,error.empty()?"Costume original owned draft address point differs":error);return true;
}
uint32_t setup(uintptr_t outer,uintptr_t live,uintptr_t owner)noexcept{__try{
    // Bind the native editor owner as SetupFor does, then use its owned Reload transaction.
    // Calling full SetupFor here would also unlock ArtStore entries outside this action.
    *reinterpret_cast<uintptr_t*>(outer+0x340)=owner;
    reinterpret_cast<void(*)(void*,void*)>(state.image+0x6b4400)(reinterpret_cast<void*>(outer),reinterpret_cast<void*>(live));return 0;
}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t transform(void* object,const a::CostumePose& pose)noexcept{__try{
    reinterpret_cast<void(*)(void*,const void*,const void*,const void*)>(state.image+0x9ff370)(object,pose.rotation.data(),pose.position.data(),pose.scale.data());return 0;
}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t assign(uintptr_t draft,const void* source)noexcept{__try{
    reinterpret_cast<void*(*)(void*,const void*)>(state.image+0x29f40)(reinterpret_cast<void*>(draft),source);
    reinterpret_cast<void(*)(void*)>(state.image+0x2d020)(reinterpret_cast<void*>(draft));return 0;
}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool prepare(uintptr_t draft,const a::CostumeRecord& record,std::string& error){
    costume_native_view::Borrowed input;uint32_t failure=0;auto build=[&](void* object,const a::CostumePose& pose){if(!failure)failure=transform(object,pose);};input.build(state.image,record,build);
    if(failure)return fail(error,"Costume original transform preparation SEH="+std::to_string(failure));const auto seh=assign(draft,input.costume.data());return !seh||fail(error,"Costume original owned draft assignment/rebuild SEH="+std::to_string(seh));
}
bool same_local(const Sample& before,const Sample& after){return character_rename::same_local(before.character,after.character)&&before.costume==after.costume&&before.local==after.local;}
bool same_menu_identity(const Sample& before,const Sample& after){return same_local(before,after);}
bool entry(void* receiver,void* record)try{
    if(!original){fault=true;return reject("Costume original unavailable");}if(!net::session_wire::connected())return original(receiver,record);
    Memory memory;std::string error;a::ui_diagnostic::Api api;bool relevant=false;if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error)){fault=true;return reject(error);}
    auto seh=quest_list::label(api.label_equals,record,"Apply",relevant);if(seh){fault=true;return reject("Costume Apply label SEH="+std::to_string(seh));}
    bool weapon=false;if(!relevant){seh=quest_list::label(api.label_equals,record,"WeaponCategory",weapon);if(seh){fault=true;return reject("Costume weapon label SEH="+std::to_string(seh));}}if(!relevant&&!weapon)return original(receiver,record);
    if(replaying){if(uintptr_t(receiver)!=replay_receiver||original_calls){fault=true;return reject("Costume original-once receiver/count differs");}++original_calls;return original(receiver,record);}
    const auto controller=typed_action_runtime::controller_status();a::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!ready||fault||pending||building_status::pending_ui()||dungeon_config::pending_ui()||dungeon_sound::pending_ui()||dungeon_gizmo::pending_ui()||dungeon_palette::pending_ui()||dungeon_light::pending_ui()||quest_list::pending_ui()||character_rename::pending_ui()||ability_edit::pending_ui()||GetCurrentThreadId()!=state.thread||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||!code(memory,error)||!typed_action_runtime::network_binding(snapshot,binding,error))return reject(error.empty()?"Costume capture requires idle bound Mainthread Controller":error);
    uintptr_t outer=0,live=0,draft=0,owner=0;if(!window(memory,outer,live,draft,owner,error)||outer!=uintptr_t(receiver)||!live||!draft)return reject(error.empty()?"Costume captured Window current draft absent":error);
    if(!owner)return original(receiver,record);
    a::CostumeEditIntent intent;if(!field(memory,owner,0x2c,intent.kind,error)||!field(memory,owner,0x30,intent.id,error))return reject(error);
    Sample before;if(!read_record(memory,weapon?live:draft,intent.record,error)||!sample(memory,binding,snapshot,intent,before,error)||before.costume!=live||before.character.selected!=owner)return reject(error.empty()?"Costume captured owner/draft census differs":error);
    if(weapon){a::CostumeWeaponIntent checkbox;if(!read_weapon_command(memory,uintptr_t(record),checkbox,error))return reject(error);std::vector<std::string> after;if(!a::costume_weapon_after(before.value.weapon_categories,checkbox,after))return reject("Costume weapon checkbox is unchanged");intent.weapon_category=std::move(checkbox);
        seh=weapon_dirty(outer);if(seh||!weapon_dirty_readback(memory,outer,error)){fault=true;return reject(error.empty()?"Costume weapon original dirty tail SEH="+std::to_string(seh):error);}}
    if(!quest_bridge::sha256(record_json(before.value.record).dump(),intent.before_sha256,error))return reject(error);pending=Pending{binding,snapshot,intent};++captures;return true;
}catch(const std::exception& exception){fault=true;return reject(std::string("Costume capture boundary: ")+exception.what());}catch(...){fault=true;return reject("Costume capture non-standard exception");}
}
bool pending_ui(){return pending.has_value()||replaying;}
bool retire_lobby(std::string& e){
    if(!ready){e.clear();return true;}
    if(GetCurrentThreadId()!=state.thread)return fail(e,"Costume edit lobby retirement requires Mainthread");
    native::WorldStatus world;if(!native::read_world_status(world,e))return false;
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0)return fail(e,"Costume edit lobby retirement requires actual idle Title");
    if(replaying)return fail(e,"Costume edit lobby retirement cannot interrupt original execution");
    pending.reset();operations.clear();replay_receiver=0;original_calls=0;
    fault=false;cause.clear();last_action.clear();e.clear();return true;
}
bool enable(std::string& error){
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(error,"Costume edit requires bound Character Mainthread");
    if(ready){for(const auto& [id,op]:operations)if(!op.retired)return fail(error,"Costume edit retained unretired operation="+id);if(fault||pending_ui())return fail(error,"Costume edit retained unresolved capture");Memory memory;if(!code(memory,error))return false;operations.clear();return true;}
    if(installed)return fail(error,"Costume edit earlier installation failed: "+cause);installed=true;Memory memory;if(!code(memory,error))return false;
    auto status=MH_CreateHook(reinterpret_cast<void*>(state.image+ui_code.rva),reinterpret_cast<void*>(&entry),reinterpret_cast<void**>(&original));
    if(status!=MH_OK||!original)return fail(error,"Costume edit prepare failed status="+std::to_string(status)+" cause="+mt2mp::hooks::framework::process_backend().error());
    status=MH_EnableHook(reinterpret_cast<void*>(state.image+ui_code.rva));if(status!=MH_OK)return fail(error,"Costume edit enable failed status="+std::to_string(status));
    MT2FW_MT2_RouteReceipt receipt{sizeof receipt};if(!quest_list::read_route(state.image+ui_code.rva,receipt,error))return false;generation=receipt.generation;if(!code(memory,error))return false;ready=true;return true;
}
void tick(){if(!pending)return;const auto saved=*pending;pending.reset();std::string error;a::Binding binding;
    if(!ready||fault||saved.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(saved.snapshot,binding,error)||binding!=saved.binding){fault=true;reject(error.empty()?"Costume captured world changed":error);return;}
    const auto id=action_runtime::uuid(error);if(id.empty()||!typed_action_runtime::submit_costume_edit(id,saved.intent,error)){fault=true;reject(error);return;}++submitted;last_action=id;
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    if(!ready||fault||GetCurrentThreadId()!=state.thread||request.action!=a::Action::EditCharacterCostume){error="Costume edit runtime unavailable";return a::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="Costume edit already attempted; no replay";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();if(!controller.value("pending",false)||controller.value("locked",true)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Costume edit requires exact pending Controller";return a::Outcome::RejectedNoEffect;}
    Memory memory;Sample before;const auto& intent=request.costume_edit;std::string prior_sha;
    if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,intent,before,error)||!quest_bridge::sha256(record_json(before.value.record).dump(),prior_sha,error))return a::Outcome::RejectedNoEffect;
    if(prior_sha!=intent.before_sha256){error="Costume edit actual current record differs from captured prior";return a::Outcome::RejectedNoEffect;}
    if(!a::costume_edit_wire_fits(request,before.value,before.character.money,error))return a::Outcome::RejectedNoEffect;
    if(authority){auto portable=before.value;portable.owner.global_random=authority->costume_edit_before.owner.global_random;if(portable!=authority->costume_edit_before||before.character.money!=authority->before){error="Costume Client actual prior owner/census/ledger differs from Host";return a::Outcome::RejectedNoEffect;}}
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;
    const auto unknown=[&](const std::string& why){replaying=false;replay_receiver=0;error=op.cause=why.empty()?"Costume edit failed without cause":why;return a::Outcome::Unknown;};
    uintptr_t outer=0;
    if(!intent.weapon_category){
    uintptr_t live=0,draft=0,owner=0;if(!window(memory,outer,live,draft,owner,error))return unknown(error);
    op.prepared=true;auto seh=setup(outer,before.costume,before.character.selected);if(seh)return unknown("Costume original owned draft ReloadCostume SEH="+std::to_string(seh));
    if(!window(memory,outer,live,draft,owner,error)||live!=before.costume||owner!=before.character.selected||!draft)return unknown(error.empty()?"Costume original prepared owner/draft differs":error);
    if(!prepare(draft,intent.record,error))return unknown(error);
    a::CostumeRecord actual;if(!read_record(memory,draft,actual,error)||actual!=intent.record)return unknown(error.empty()?"Costume prepared actual owned draft differs":error);
    Sample prepared;if(!sample(memory,binding,request.snapshot_uuid,intent,prepared,error))return unknown(error);auto portable=prepared.value;portable.owner.global_random=before.value.owner.global_random;
    if(!same_menu_identity(before,prepared)||portable!=before.value)return unknown("Costume owned UI preparation changed live gameplay");
    }else if(intent.record!=before.value.record)return unknown("Costume weapon intent changed an unrelated live Costume");
    const auto desired_rng=authority?authority->costume_edit_before.owner.global_random:before.value.owner.global_random;op.seed_attempted=true;const auto seed=action_runtime::seed(state.image,desired_rng);if(seed)return unknown("Costume Host RNG adoption SEH="+std::to_string(seed));
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.before,error)||op.before.value.owner.global_random!=desired_rng)return unknown(error.empty()?"Costume Host RNG readback differs":error);
    op.evidence.before=op.before.character.money;op.evidence.costume_edit_before=op.before.value;
    if(intent.weapon_category){op.attempted=true;
        const auto native=weapon_original(op.before.character.selected,*intent.weapon_category,op.calls,op.returned);
        if(native||!op.returned||op.calls!=1)return unknown("Costume weapon original category setter did not return once SEH="+std::to_string(native));
    }else{
    a::ui_diagnostic::Api api;a::ui_diagnostic::Window command_window;command_window.window=outer;if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return unknown(error);api.command=reinterpret_cast<Command>(state.image+ui_code.rva);api.destroy_label="Apply";
    a::ui_diagnostic::Receipt receipt;op.attempted=true;replaying=true;replay_receiver=outer;original_calls=0;const bool returned=a::ui_diagnostic::process_calls().command(api,command_window,receipt,error);op.calls=original_calls;op.returned=receipt.ui_command_returned;replaying=false;replay_receiver=0;
    if(!returned||!op.returned||op.calls!=1||!receipt.record_cleanup_confirmed)return unknown(error.empty()?"Costume original transaction lacks exact once/cleanup receipt":error);
    }
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.after,error)||!same_local(op.before,op.after)||!a::costume_edit_transition(intent,op.before.value,op.after.value))return unknown(error.empty()?"Costume original effect/owner/census/ledger/Undo differs":error);
    op.evidence.after=op.after.character.money;op.evidence.costume_edit_after=op.after.value;if(authority){auto expected=*authority;expected.retirement_confirmed=false;if(op.evidence!=expected)return unknown("Costume Client original result differs from Host");}
    op.confirmed=true;return a::Outcome::Confirmed;
}catch(const std::exception& exception){replaying=false;replay_receiver=0;error=std::string("Costume perform boundary: ")+exception.what();return a::Outcome::Unknown;}catch(...){replaying=false;replay_receiver=0;error="Costume perform non-standard exception";return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& result,std::string& error){const auto found=operations.find(request.action_uuid);
    if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.returned||found->second.calls!=1)return fail(error,"Costume observation lacks exact confirmed original-once receipt");
    const auto& op=found->second;Memory memory;Sample now;if(!sample(memory,binding,request.snapshot_uuid,request.costume_edit,now,error)||!same_local(op.after,now)||now.value!=op.after.value)return fail(error,error.empty()?"Costume current result differs before ACK":error);result=op.evidence;result.retirement_confirmed=op.retired;return true;}
bool retire(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence ignored;if(!observe(binding,request,ignored,error))return false;auto& op=operations.at(request.action_uuid);if(op.retired)return fail(error,"Costume receipt already retired");op.retired=true;return true;}
json status(){json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"prepared",op.prepared},{"attempted",op.attempted},{"returned",op.returned},{"original_calls",op.calls},{"confirmed",op.confirmed},{"retired",op.retired},{"seed_attempted",op.seed_attempted},{"cause",op.cause}};return {{"enabled",ready},{"fault",fault},{"pending_ui",pending_ui()},{"captures",captures},{"submitted",submitted},{"rejected",rejected},{"last_action",last_action},{"cause",cause},{"operations",rows}};}
}
