namespace character_rename {
namespace {
using Command=a::ui_diagnostic::UiCommand;
Command original=nullptr;uint64_t generation=0,captures=0,submitted=0,rejected=0;bool installed=false,ready=false,fault=false,replaying=false;
uintptr_t replay_receiver=0;uint8_t replay_property=0;uint32_t original_calls=0;std::string cause,last_action;
struct Sample {
    host::WorldState world;uintptr_t manager=0,selected=0;uint64_t uid=0;
    std::vector<std::array<uintptr_t,3>> local;std::array<std::array<uintptr_t,3>,5> arrays{};
    a::CharacterRenameState value;a::Ledger money;undo_probe::UndoWitness undo;
};
struct Pending {a::Binding binding;std::string snapshot;a::CharacterRenameIntent intent;};
struct Operation {a::Binding binding;a::Request request;Sample before,after;a::Evidence evidence;
    bool attempted=false,returned=false,confirmed=false,retired=false,seed_attempted=false;uint32_t calls=0;std::string cause;};
std::optional<Pending> pending;std::map<std::string,Operation> operations;
bool entry(void*,void*);
bool reject(const std::string& error){++rejected;if(cause!=error){cause=error;LOG_ERROR("[CharacterRename] {}",error);}return false;}
constexpr npc_quest_list_ui_codes::Code ui_code{0x689e80,4212,"f007ff46880dc66f06dd5a89f5c875c8a501a28051004c0fc0a216633cd979a8",
    {0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0x28,
     0x04,0,0,0x0f,0x11,0xb4,0x24,0,0x04,0,0,0x0f,0x11,0xbc,0x24,0x10}};
bool code(Memory& m,std::string& error){
    std::string bytes(ui_code.bytes,'\0'),hash;if(!m.copy(state.image+ui_code.rva,bytes.data(),bytes.size(),error)||!quest_bridge::sha256(bytes,hash,error))return false;
    if(original){MT2FW_MT2_RouteReceipt receipt{sizeof receipt};npc_quest_list_ui_codes::Owned owner{uintptr_t(&entry),uintptr_t(original),generation};
        if(!quest_list::read_route(state.image+ui_code.rva,receipt,error)||!npc_quest_list_ui_codes::receipt(receipt,state.image+ui_code.rva,ui_code,owner,error)||
           std::memcmp(bytes.data(),receipt.current,32))return fail(error,error.empty()?"Character rename exact owned route differs":error);
        std::array<unsigned char,20> trampoline{};uintptr_t resume=0;constexpr std::array<unsigned char,6> continuation{0xff,0x25,0,0,0,0};
        if(!m.copy(uintptr_t(original),trampoline.data(),trampoline.size(),error))return false;std::memcpy(&resume,trampoline.data()+12,8);
        if(!std::equal(ui_code.prefix.begin(),ui_code.prefix.begin()+6,trampoline.begin())||
           !std::equal(continuation.begin(),continuation.end(),trampoline.begin()+6)||resume!=state.image+ui_code.rva+6)
            return fail(error,"Character rename retained original six-byte instruction boundary differs");
        std::memcpy(bytes.data(),receipt.pristine,32);if(!quest_bridge::sha256(bytes,hash,error))return false;
        if(!quest_list::verify_code(state.image+ui_code.rva,bytes.data(),4096,error))return false;
    }
    if(hash!=ui_code.sha)return fail(error,"Character rename immutable outer UI transaction body differs");
    for(const auto& pin:character_rename_codes::helpers){
        bytes.assign(pin.size,'\0');if(!m.copy(state.image+pin.rva,bytes.data(),bytes.size(),error)||!quest_bridge::sha256(bytes,hash,error))return false;
        if(!hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error))
            return fail(error,"Character rename original transaction/preparation body differs RVA="+std::to_string(pin.rva)+" original="+error);}
    return true;
}
const char* stat_label(uint8_t property){switch(property){case 1:return "SetHealth";case 2:return "SetMana";case 3:return "SetRage";case 4:return "SetSpeed";default:return "DoRename";}}
bool stat_read(Memory& memory,uintptr_t object,a::CharacterRenameState& value,std::string& error){
    for(size_t set=0;set<2;++set){const auto offset=set?0x80:0x50;uintptr_t data=0;int32_t count=0,capacity=0;
        if(!field(memory,object,offset+8,data,error)||!field(memory,object,offset+0x10,count,error)||!field(memory,object,offset+0x14,capacity,error)||
           count<3||count>10000||capacity<count||!data)return fail(error,error.empty()?"Character original attribute-set bounds differ":error);
        std::set<uintptr_t> pointers;
        for(size_t index=0;index<3;++index){uintptr_t attribute=0,ap=0;if(!field(memory,data,index*8,attribute,error)||!attribute||!pointers.insert(attribute).second||
               !field(memory,attribute,0,ap,error)||ap!=state.image+0x172e8d0||!field(memory,attribute,0x28,value.attributes[set][index],error)||!std::isfinite(value.attributes[set][index]))
                return fail(error,error.empty()?"Character original attribute identity/value differs":error);}}
    uintptr_t phases=0;int32_t capacity=0;
    if(!field(memory,object,0x120,value.speed,error)||!std::isfinite(value.speed)||!field(memory,object,0x160,phases,error)||
       !field(memory,object,0x168,value.boss_phases,error)||!field(memory,object,0x16c,capacity,error)||value.boss_phases<0||value.boss_phases>10000||capacity<value.boss_phases||(value.boss_phases&&!phases))
        return fail(error,error.empty()?"Character original speed/Boss phase owner differs":error);
    if(value.boss_phases){uintptr_t phase=0,ap=0;if(!field(memory,phases,0,phase,error)||!phase||!field(memory,phase,0,ap,error)||ap!=state.image+0x16ddb00||!field(memory,phase,0x58,value.boss_health,error))
        return fail(error,error.empty()?"Character original first Boss phase health binding differs":error);}
    return true;
}
bool read_input(Memory& memory,uintptr_t view,uintptr_t editor,a::CharacterRenameIntent& intent,std::string& error){
    if(!intent.property)return npc_quest_records::read_string(memory,editor,0x2d0,"Character rename input",intent.name,error);
    uintptr_t slider=0;int32_t raw=0;const auto offset=0x3e8+size_t(intent.property-1)*8;
    if(!field(memory,view,offset,slider,error)||!slider||!field(memory,slider,0x4d0,raw,error))return fail(error,error.empty()?"Character original stat Slider absent":error);
    const auto seh=quest_list::external_value(slider,raw,intent.value);
    if(seh||!std::isfinite(intent.value)||intent.value<0||intent.value>1000000.f)return fail(error,"Character original stat Slider conversion differs SEH="+std::to_string(seh));
    return true;
}
uint32_t set_stat(uintptr_t slider,float value)noexcept{
    __try{reinterpret_cast<void(*)(void*,float,bool)>(state.image+0x7bc040)(reinterpret_cast<void*>(slider),value,false);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool prepare_stat(Memory& memory,uintptr_t view,const a::CharacterRenameIntent& intent,std::string& error){
    uintptr_t slider=0,binding=0;const auto offset=0x3e8+size_t(intent.property-1)*8;
    if(!field(memory,view,offset,slider,error)||!slider||!field(memory,slider,0x420,binding,error)||binding||!field(memory,slider,0x428,binding,error)||binding)
        return fail(error,error.empty()?"Character stat Slider absent or has a direct gameplay binding":error);
    const auto seh=set_stat(slider,intent.value);if(seh)return fail(error,"Character original stat preparation SEH="+std::to_string(seh));
    return true;
}

bool sample(Memory& memory,const a::Binding& binding,const std::string& snapshot,const a::CharacterRenameIntent& intent,Sample& out,std::string& error){
    out={};host::LookupApi api;host::NativeOrigin origin;
    if(!host::resolve_lookup(memory.process,state.image,exe_sha,api,error))return false;
    host::Reader reader(memory.process,origin,state.image,state.thread,api);
    if(!reader.current(out.world,true,error)||!action_runtime::bound(out.world,binding,snapshot,error))return false;
    uintptr_t ap=0,owned=0;const auto native_state=uintptr_t(out.world.origin.state);
    if(!field(memory,state.image,0x13df7a0,out.manager,error)||!field(memory,native_state,0x168,owned,error)||owned!=out.manager||
       !field(memory,out.manager,0,ap,error)||ap!=state.image+0x1732430)return fail(error,error.empty()?"Character rename current manager ownership differs":error);
    character_definition_identity::Census definitions;
    auto definition_read=[&](uintptr_t object,size_t offset,auto& value,std::string& cause){return field(memory,object,offset,value,cause);};
    if(!character_definition_identity::read(definition_read,state.image,native_state,out.manager,definitions,error))return false;
    json rows=json::array(),others=json::array(),names=json::array();std::set<uintptr_t> pointers;bool selected=false;
    for(int32_t kind=0;kind<5;++kind){const auto offset=0x80+size_t(kind)*0x50;uintptr_t data=0;int32_t count=0,capacity=0;
        if(!field(memory,out.manager,offset+8,data,error)||!field(memory,out.manager,offset+0x10,count,error)||!field(memory,out.manager,offset+0x14,capacity,error)||
           count<0||count>1000000||capacity<count||(count&&!data))return fail(error,error.empty()?"Character rename manager census bounds differ":error);
        out.arrays[size_t(kind)]={data,uintptr_t(count),uintptr_t(capacity)};
        for(int32_t id=0;id<count;++id){uintptr_t object=0,parent=0;uint64_t uid=0;int32_t actual_kind=-1,actual_id=-1,day=0;uint8_t deleted=2,edited=2,released=2;
            std::string name,definition;
            if(!field(memory,data,size_t(id)*8,object,error)||!object||!pointers.insert(object).second||!field(memory,object,0,ap,error)||ap!=state.image+0x1705990||
               !field(memory,object,0x18,uid,error)||!field(memory,object,0x2c,actual_kind,error)||!field(memory,object,0x30,actual_id,error)||actual_kind!=kind||actual_id!=id||
               !field(memory,object,0x490,deleted,error)||deleted>1||!field(memory,object,0x491,edited,error)||edited>1||!field(memory,object,0x492,released,error)||released>1||
               !field(memory,object,0x2e0,day,error)||!field(memory,object,0x4a0,parent,error)||
               !npc_quest_records::read_string(memory,object,0xb0,"Character name",name,error)||
               !npc_quest_records::read_string(memory,object,0x248,"Character definition name",definition,error))return fail(error,error.empty()?"Character rename type identity/flags/name differ":error);
            const character_definition_identity::Definition* definition_identity=nullptr;
            if(!character_definition_identity::find(definitions,object,uid,definition_identity,error)||definition_identity->parent!=parent)return fail(error,error.empty()?"Character sampled parent/current definition identity differs":error);
            std::string parent_key;
            if(parent){const character_definition_identity::Definition* base=nullptr;if(!character_definition_identity::find(definitions,parent,definition_identity->parent_uid,base,error))return false;parent_key=base->key;}
            out.local.push_back({object,uintptr_t(uid),parent});
            json row={{"kind",kind},{"id",id},{"name",name},{"definition_name",definition},{"deleted",bool(deleted)},{"edited",bool(edited)},{"released",bool(released)},{"rename_day",day}};
            a::CharacterRenameState stats;if(!stat_read(memory,object,stats,error))return false;
            names.push_back({{"kind",kind},{"id",id},{"name",name},{"definition_name",definition},{"deleted",bool(deleted)},{"parent_key",parent_key}});
            row["parent_key"]=parent_key;row["attributes"]=stats.attributes;row["speed"]=stats.speed;row["boss_health"]=stats.boss_health;row["boss_phases"]=stats.boss_phases;
            rows.push_back(row);++out.value.count;
            if(kind==intent.kind&&id==intent.id){if(deleted||!uid)return fail(error,"Character rename selected type is destroyed or has zero full UID");selected=true;out.selected=object;out.uid=uid;
                out.value.name=name;out.value.definition_name=definition;out.value.edited=edited!=0;out.value.released=released!=0;out.value.rename_day=day;out.value.attributes=stats.attributes;out.value.speed=stats.speed;out.value.boss_health=stats.boss_health;out.value.boss_phases=stats.boss_phases;}
            else others.push_back(row);
        }
    }
    if(!selected)return fail(error,"Character rename selected current CharacterTypeID absent");
    for(size_t kind=0;kind<5;++kind){uintptr_t data=0;int32_t count=0,capacity=0;const auto offset=0x80+kind*0x50;const auto& before=out.arrays[kind];
        if(!field(memory,out.manager,offset+8,data,error)||data!=before[0]||!field(memory,out.manager,offset+0x10,count,error)||uintptr_t(count)!=before[1]||
           !field(memory,out.manager,offset+0x14,capacity,error)||uintptr_t(capacity)!=before[2])return fail(error,error.empty()?"Character rename manager census changed":error);}
    native::SimulationState money;uintptr_t random=0,release_manager=0,release=0;
    if(!native::read_simulation_state(money,error)||!undo_probe::read_undo(memory.process,state.image,out.undo,error)||out.undo.active_graph||
       !field(memory,state.image,0x164b250,random,error)||!field(memory,random,0,out.value.global_random,error)||
       !field(memory,state.image,0x13df4f0,release_manager,error)||!field(memory,release_manager,0,ap,error)||ap!=state.image+0x1714ed0||
       !field(memory,release_manager,0x38,release,error)||!field(memory,release,0,ap,error)||ap!=state.image+0x16c12b0||
       !field(memory,release,0x120,out.value.release_edits,error))return fail(error,error.empty()?"Character rename ledger/Undo/release ownership differs":error);
    out.money=ledger(money);out.value.day=money.clock.flat_day;out.value.undo_count=out.undo.undo.size();
    return quest_bridge::sha256(rows.dump(),out.value.all_sha256,error)&&quest_bridge::sha256((intent.property?names:others).dump(),out.value.others_sha256,error);
}
uint32_t find_window(uintptr_t& window)noexcept{
    __try{const char* label="info_charactertype";const quest_list::QuestTitleView text{label,18};
        window=reinterpret_cast<uintptr_t(*)(const quest_list::QuestTitleView&)>(state.image+0x81aa90)(text);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t name_in_use(uintptr_t manager,const char* text,size_t length,bool& result)noexcept{
    __try{const quest_list::QuestTitleView name{text,length};result=reinterpret_cast<bool(*)(void*,const quest_list::QuestTitleView&)>(state.image+0x9e690)(reinterpret_cast<void*>(manager),name);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t set_window(uintptr_t window,uintptr_t object,uintptr_t view,uintptr_t edit,const char* text,size_t length)noexcept{
    __try{reinterpret_cast<void(*)(void*,void*)>(state.image+0x68b8b0)(reinterpret_cast<void*>(window),reinterpret_cast<void*>(object));
        const quest_list::QuestTitleView title{text,length};reinterpret_cast<void(*)(void*,const quest_list::QuestTitleView&)>(state.image+0x7f1630)(reinterpret_cast<void*>(edit),title);
        (void)view;return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool window(Memory& m,uintptr_t expected,uintptr_t& view,uintptr_t& editor,uintptr_t& outer,std::string& error){
    const auto seh=find_window(outer);uintptr_t ap=0,selected=0;if(seh||!outer||!field(m,outer,0,ap,error)||ap!=state.image+0x173a7f0||
       !field(m,outer,0x4e8,view,error)||!field(m,view,0,ap,error)||ap!=state.image+0x1736770||
       !field(m,view,0x460,selected,error)||(expected&&selected!=expected)||!field(m,view,0x360,editor,error)||!editor)
        return fail(error,error.empty()?"Character rename live named native Window/View/editor binding differs SEH="+std::to_string(seh):error);
    return true;
}
bool entry(void* receiver,void* record)try{
    if(!original){fault=true;reject("Character rename original unavailable");return false;}
    if(!net::session_wire::connected())return original(receiver,record);
    bool membership=false;std::string membership_error;
    if(!character_membership::capture_variant(receiver,record,membership,membership_error)){reject(membership_error);return false;}
    if(membership)return true;
    bool adjusted=false;std::string adjustment_error;
    if(!ability_edit::capture_adjustment(receiver,record,adjusted,adjustment_error)){reject(adjustment_error);return false;}
    if(adjusted)return true;
    Memory memory;std::string error;a::ui_diagnostic::Api api;bool relevant=false;
    if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error)){fault=true;reject(error);return false;}
    uint8_t property=0;auto seh=quest_list::label(api.label_equals,record,"DoRename",relevant);
    if(!relevant&&!seh)for(uint8_t field=1;field<=4;++field){seh=quest_list::label(api.label_equals,record,stat_label(field),relevant);if(relevant){property=field;break;}if(seh)break;}if(seh){fault=true;reject("Character rename label SEH="+std::to_string(seh));return false;}
    if(!relevant)return original(receiver,record);
    if(replaying){if(uintptr_t(receiver)!=replay_receiver||replay_property!=property||original_calls){fault=true;reject("Character rename original-once receiver/count differs");return false;}
        ++original_calls;return original(receiver,record);}
    const auto controller=typed_action_runtime::controller_status();a::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!ready||fault||pending||costume_edit::pending_ui()||dungeon_config::pending_ui()||dungeon_sound::pending_ui()||dungeon_gizmo::pending_ui()||dungeon_palette::pending_ui()||dungeon_light::pending_ui()||building_status::pending_ui()||quest_list::pending_ui()||ability_edit::pending_ui()||GetCurrentThreadId()!=state.thread||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||
       !code(memory,error)||!typed_action_runtime::network_binding(snapshot,binding,error)){reject(error.empty()?"Character rename capture requires idle bound Mainthread Controller":error);return false;}
    uintptr_t object=0,view=0,editor=0,outer=0;a::CharacterRenameIntent intent;intent.property=property;
    if(!field(memory,uintptr_t(receiver),0x460,object,error)||!field(memory,object,0x2c,intent.kind,error)||!field(memory,object,0x30,intent.id,error)||
       !window(memory,object,view,editor,outer,error)||view!=uintptr_t(receiver)||!read_input(memory,view,editor,intent,error)){
        reject(error.empty()?"Character rename captured selected type/Window differs":error);return false;}
    Sample before;if(!sample(memory,binding,snapshot,intent,before,error)||before.selected!=object){reject(error);return false;}intent.before=before.value.name;
    if(!intent.property&&intent.before==intent.name)return original(receiver,record);
    if(intent.name.size()>1024){reject("Character rename title outside native ordered payload bound");return false;}
    bool duplicate=false;const auto query=intent.property?0:name_in_use(before.manager,intent.name.c_str(),intent.name.size(),duplicate);
    if(query){fault=true;reject("Character rename original name-availability query SEH="+std::to_string(query));return false;}
    if(duplicate)return original(receiver,record);
    pending=Pending{binding,snapshot,intent};++captures;return true;
}catch(const std::exception& exception){fault=true;reject(std::string("Character rename capture boundary: ")+exception.what());return false;}
catch(...){fault=true;reject("Character rename capture non-standard exception");return false;}
bool same_local(const Sample& before,const Sample& after){return same_world(before.world,after.world)&&before.manager==after.manager&&before.selected==after.selected&&
    before.uid==after.uid&&before.local==after.local&&before.arrays==after.arrays&&before.undo==after.undo&&before.money==after.money;}
}
bool pending_ui(){return pending.has_value()||replaying;}
bool retire_lobby(std::string& e){
    if(!ready){e.clear();return true;}
    if(GetCurrentThreadId()!=state.thread)return fail(e,"Character rename lobby retirement requires Mainthread");
    native::WorldStatus world;if(!native::read_world_status(world,e))return false;
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0)return fail(e,"Character rename lobby retirement requires actual idle Title");
    if(replaying)return fail(e,"Character rename lobby retirement cannot interrupt original execution");
    pending.reset();operations.clear();replay_property=0;replay_receiver=0;original_calls=0;
    fault=false;cause.clear();last_action.clear();e.clear();return true;
}
bool enable(std::string& error){
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(error,"Character rename requires bound Monster/character Mainthread");
    if(ready){for(const auto& [id,op]:operations)if(!op.retired)return fail(error,"Character rename retained unretired operation="+id);
        if(fault||pending_ui())return fail(error,"Character rename retained unresolved capture");Memory memory;if(!code(memory,error))return false;operations.clear();return true;}
    if(installed)return fail(error,"Character rename earlier installation failed: "+cause);installed=true;
    Memory memory;if(!code(memory,error))return false;
    auto status=MH_CreateHook(reinterpret_cast<void*>(state.image+ui_code.rva),reinterpret_cast<void*>(&entry),reinterpret_cast<void**>(&original));
    if(status!=MH_OK||!original)return fail(error,"Character rename prepare failed status="+std::to_string(status)+" cause="+mt2mp::hooks::framework::process_backend().error());
    MT2FW_MT2_RouteReceipt receipt{sizeof receipt};if(!quest_list::read_route(state.image+ui_code.rva,receipt,error))return false;generation=receipt.generation;
    status=MH_EnableHook(reinterpret_cast<void*>(state.image+ui_code.rva));if(status!=MH_OK)return fail(error,"Character rename enable failed status="+std::to_string(status));
    if(!code(memory,error))return false;ready=true;return true;
}
void tick(){if(!pending)return;const auto saved=*pending;pending.reset();std::string error;a::Binding binding;
    if(!ready||fault||saved.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(saved.snapshot,binding,error)||binding!=saved.binding){fault=true;reject(error.empty()?"Character rename captured world changed":error);return;}
    const auto id=action_runtime::uuid(error);if(id.empty()||!typed_action_runtime::submit_character_rename(id,saved.intent,error)){fault=true;reject(error);return;}++submitted;last_action=id;
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    if(!ready||fault||GetCurrentThreadId()!=state.thread||(request.action<a::Action::RenameCharacter||request.action>a::Action::SetCharacterSpeed)){error="Character rename runtime unavailable";return a::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="Character rename already attempted; no replay";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();if(!controller.value("pending",false)||controller.value("locked",true)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Character rename requires exact pending Controller";return a::Outcome::RejectedNoEffect;}
    Memory memory;Sample before;const auto& intent=request.character_rename;
    if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,intent,before,error)||before.value.name!=intent.before)return a::Outcome::RejectedNoEffect;
    bool duplicate=false;const auto query=intent.property?0:name_in_use(before.manager,intent.name.c_str(),intent.name.size(),duplicate);
    if(query||duplicate){error="Character rename original name-availability query rejected SEH="+std::to_string(query);return a::Outcome::RejectedNoEffect;}
    if(authority){auto portable=before.value;portable.global_random=authority->character_rename_before.global_random;
        if(portable!=authority->character_rename_before||before.money!=authority->before){error="Character rename Client actual prior census/ledger differs from Host";return a::Outcome::RejectedNoEffect;}}
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;
    const auto unknown=[&](const std::string& why){replaying=false;replay_receiver=0;error=op.cause=why.empty()?"Character rename failed without cause":why;return a::Outcome::Unknown;};
    const auto desired_rng=authority?authority->character_rename_before.global_random:before.value.global_random;
    uintptr_t view=0,editor=0,outer=0;if(!window(memory,0,view,editor,outer,error))return unknown(error);
    auto seh=set_window(outer,before.selected,view,editor,intent.name.c_str(),intent.name.size());if(seh)return unknown("Character original menu preparation SEH="+std::to_string(seh));
    if(intent.property&&!prepare_stat(memory,view,intent,error))return unknown(error);
    Sample prepared;if(!sample(memory,binding,request.snapshot_uuid,intent,prepared,error))return unknown(error);
    auto prepared_state=prepared.value;prepared_state.global_random=before.value.global_random;
    if(!same_local(before,prepared)||prepared_state!=before.value||!window(memory,before.selected,view,editor,outer,error))return unknown(error.empty()?"Character rename normal menu preparation changed gameplay":error);
    auto prepared_intent=intent;if(!read_input(memory,view,editor,prepared_intent,error)||prepared_intent!=intent)return unknown(error.empty()?"Character rename prepared native editor differs":error);
    op.seed_attempted=true;const auto seed=action_runtime::seed(state.image,desired_rng);if(seed)return unknown("Character rename original Host RNG adoption SEH="+std::to_string(seed));
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.before,error)||op.before.value.global_random!=desired_rng)return unknown(error.empty()?"Character rename Host RNG readback differs":error);
    op.evidence.before=op.before.money;op.evidence.character_rename_before=op.before.value;
    a::ui_diagnostic::Api api;a::ui_diagnostic::Window command_window;command_window.window=view;
    if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return unknown(error);api.command=reinterpret_cast<Command>(state.image+ui_code.rva);api.destroy_label=stat_label(intent.property);
    a::ui_diagnostic::Receipt receipt;op.attempted=true;replaying=true;replay_receiver=view;replay_property=intent.property;original_calls=0;
    const bool returned=a::ui_diagnostic::process_calls().command(api,command_window,receipt,error);op.calls=original_calls;op.returned=receipt.ui_command_returned;replaying=false;replay_receiver=0;
    if(!returned||!op.returned||op.calls!=1||!receipt.record_cleanup_confirmed)return unknown(error.empty()?"Character rename original transaction lacks exact once/cleanup receipt":error);
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.after,error)||!same_local(op.before,op.after)||
       !a::character_rename_transition(intent,op.before.value,op.after.value))return unknown(error.empty()?"Character rename original effect/census/ledger/Undo/release differs":error);
    op.evidence.after=op.after.money;op.evidence.character_rename_after=op.after.value;
    if(authority){auto expected=*authority;expected.retirement_confirmed=false;if(op.evidence!=expected)return unknown("Character rename Client original result differs from Host");}
    op.confirmed=true;return a::Outcome::Confirmed;
}catch(const std::exception& exception){replaying=false;replay_receiver=0;error=std::string("Character rename perform boundary: ")+exception.what();return a::Outcome::Unknown;}
catch(...){replaying=false;replay_receiver=0;error="Character rename perform non-standard exception";return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& result,std::string& error){
    const auto found=operations.find(request.action_uuid);if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.returned||found->second.calls!=1)
        return fail(error,"Character rename observation lacks exact confirmed original-once receipt");
    const auto& op=found->second;Memory memory;Sample now;if(!sample(memory,binding,request.snapshot_uuid,request.character_rename,now,error)||
       !same_local(op.after,now)||now.value!=op.after.value)return fail(error,error.empty()?"Character rename current result differs before ACK":error);
    result=op.evidence;result.retirement_confirmed=op.retired;return true;
}
bool retire(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence ignored;if(!observe(binding,request,ignored,error))return false;
    auto& op=operations.at(request.action_uuid);if(op.retired)return fail(error,"Character rename receipt already retired");op.retired=true;return true;}
json status(){json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"attempted",op.attempted},{"returned",op.returned},{"original_calls",op.calls},{"confirmed",op.confirmed},{"retired",op.retired},{"seed_attempted",op.seed_attempted},{"cause",op.cause}};
    return {{"enabled",ready},{"fault",fault},{"pending_ui",pending_ui()},{"captures",captures},{"submitted",submitted},{"rejected",rejected},{"last_action",last_action},{"cause",cause},{"operations",rows}};}
}
