namespace ability_edit {
namespace {
using Command=a::ui_diagnostic::UiCommand;
Command original=nullptr;uint64_t generation=0,captures=0,submitted=0,rejected=0;bool installed=false,ready=false,fault=false,replaying=false;
uintptr_t replay_receiver=0;uint32_t original_calls=0;std::string cause,last_action;
constexpr std::array<size_t,8> string_offsets{0x48,0x68,0x88,0xa8,0xc8,0xf8,0x118,0x138};
constexpr npc_quest_list_ui_codes::Code ui_code{0x5714c0,3374,"c58e4309398a34ca2ee0e15494adc64f7590f76184654bddb2d9cbb280e2df2e",
    {0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xa8,0,0,0,0x0f,0x11,0xb4,0x24,0x90,0,0,0,0x48,0x89,0xce,0x48,0x89,0xd1,0x48,0x8d,0x5c,0x24,0x50,0x48,0x89}};
struct Sample {character_rename::Sample character;uintptr_t prototype=0,proxy=0;uint64_t uid=0;std::vector<std::array<uintptr_t,3>> local; a::AbilityEditState value;};
struct Pending {a::Binding binding;std::string snapshot;a::AbilityEditIntent intent;uintptr_t view=0;};
struct Operation {a::Binding binding;a::Request request;Sample before,after;a::Evidence evidence;bool attempted=false,returned=false,confirmed=false,retired=false,prepared=false,seed_attempted=false;uint32_t calls=0;std::string cause;};
std::optional<Pending> pending;std::map<std::string,Operation> operations;
bool entry(void*,void*);
bool view_route(Memory&,std::string&);
bool reject(const std::string& error){++rejected;if(cause!=error){cause=error;LOG_ERROR("[AbilityEdit] {}",error);}return false;}
json record_json(const a::AbilityRecord& v){json costs=json::array(),effects=json::array();
    for(size_t n=0;n<2;++n)for(const auto& row:n?v.effects:v.costs)(n?effects:costs).push_back({{"integers",row.integers},{"values",row.values},{"count",row.count}});
    return {{"text",v.text},{"item_value",v.item_value},{"item_kind",v.item_kind},{"item_flag",v.item_flag},{"selectors",v.selectors},{"color",v.color},{"timing",v.timing},{"flags",v.flags},{"final_values",v.final_values},{"level",v.level},{"final_flag",v.final_flag},{"costs",costs},{"effects",effects}};
}
bool code(Memory& memory,std::string& error){
    std::string bytes(ui_code.bytes,'\0'),hash;if(!memory.copy(state.image+ui_code.rva,bytes.data(),bytes.size(),error))return false;
    if(original){MT2FW_MT2_RouteReceipt receipt{sizeof receipt};const npc_quest_list_ui_codes::Owned owner{uintptr_t(&entry),uintptr_t(original),generation};
        if(!quest_list::read_route(state.image+ui_code.rva,receipt,error)||!npc_quest_list_ui_codes::receipt(receipt,state.image+ui_code.rva,ui_code,owner,error)||std::memcmp(bytes.data(),receipt.current,32))return fail(error,error.empty()?"Ability exact owned route differs":error);
        std::array<unsigned char,25> trampoline{};uintptr_t resume=0;constexpr std::array<unsigned char,6> continuation{0xff,0x25,0,0,0,0};
        if(!memory.copy(uintptr_t(original),trampoline.data(),trampoline.size(),error))return false;std::memcpy(&resume,trampoline.data()+17,8);
        if(!std::equal(ui_code.prefix.begin(),ui_code.prefix.begin()+11,trampoline.begin())||!std::equal(continuation.begin(),continuation.end(),trampoline.begin()+11)||resume!=state.image+ui_code.rva+11)return fail(error,"Ability exact original eleven-byte instruction boundary differs");
        std::memcpy(bytes.data(),receipt.pristine,32);if(!quest_list::verify_code(state.image+ui_code.rva,bytes.data(),bytes.size(),error))return false;}
    if(!quest_bridge::sha256(bytes,hash,error)||hash!=ui_code.sha)return fail(error,error.empty()?"Ability original outer UI transaction body differs":error);
    for(const auto& pin:ability_edit_codes::helpers){bytes.assign(pin.size,'\0');if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error)||!quest_bridge::sha256(bytes,hash,error))return false;
        if(!hooks::framework::verify_original_hash(state.image+pin.rva,bytes.data(),bytes.size(),pin.sha,error))return fail(error,"Ability original preparation/ownership helper body differs RVA="+std::to_string(pin.rva)+" original="+error);}
    return true;
}
bool read_record(Memory& memory,uintptr_t object,a::AbilityRecord& value,std::string& error){
    value={};uintptr_t ap=0;if(!field(memory,object,0,ap,error)||ap!=state.image+0x171c930)return fail(error,error.empty()?"Ability prototype address point differs":error);
    for(size_t i=0;i<string_offsets.size();++i)if(!npc_quest_records::read_string(memory,object,string_offsets[i],"Ability native authored string",value.text[i],error)||value.text[i].size()>4096)return fail(error,error.empty()?"Ability authored string exceeds ordered bound":error);
    uint8_t flag=2;if(!field(memory,object,0xe8,value.item_value,error)||!std::isfinite(value.item_value)||!field(memory,object,0xec,value.item_kind,error)||!field(memory,object,0xf0,flag,error)||flag>1)return fail(error,error.empty()?"Ability native item scalar differs":error);value.item_flag=flag!=0;
    if(!field(memory,object,0x15c,value.selectors,error)||!field(memory,object,0x170,value.color,error)||!field(memory,object,0x180,value.timing,error)||!field(memory,object,0x1f8,value.final_values,error)||!field(memory,object,0x200,value.level,error)||!field(memory,object,0x204,flag,error)||flag>1)return fail(error,error.empty()?"Ability authored native fixed record differs":error);value.final_flag=flag!=0;
    for(size_t i=0;i<5;++i){if(!field(memory,object,0x1f0+i,flag,error)||flag>1)return fail(error,error.empty()?"Ability authored native boolean differs":error);value.flags[i]=flag!=0;}
    for(float number:value.color)if(!std::isfinite(number))return fail(error,"Ability color is non-finite");
    for(float number:value.timing)if(!std::isfinite(number))return fail(error,"Ability timing is non-finite");for(float number:value.final_values)if(!std::isfinite(number))return fail(error,"Ability final scalar is non-finite");
    std::set<uintptr_t> pointers;
    for(size_t group=0;group<2;++group){const auto offset=group?0x1c0:0x190;uintptr_t data=0;int32_t count=0,capacity=0;
        if(!field(memory,object,offset+8,data,error)||!field(memory,object,offset+0x10,count,error)||!field(memory,object,offset+0x14,capacity,error)||count<0||count>64||capacity<count||(count&&!data))return fail(error,error.empty()?"Ability owned cost/effect bounds differ":error);
        for(int32_t i=0;i<count;++i){uintptr_t effect=0;uint64_t uid=0;a::AbilityEffectRecord row;
            if(!field(memory,data,size_t(i)*8,effect,error)||!effect||!pointers.insert(effect).second||!field(memory,effect,0,ap,error)||ap!=state.image+0x1704e90||!field(memory,effect,0x18,uid,error)||!uid||
               !field(memory,effect,0x20,row.integers,error)||!field(memory,effect,0x2c,row.values,error)||!field(memory,effect,0x34,row.count,error)||row.integers[0]<0||row.integers[0]>=8||!std::isfinite(row.values[0])||!std::isfinite(row.values[1]))return fail(error,error.empty()?"Ability actual owned effect full identity/record differs":error);
            (group?value.effects:value.costs).push_back(row);}
        uintptr_t again=0;int32_t size=0,cap=0;if(!field(memory,object,offset+8,again,error)||again!=data||!field(memory,object,offset+0x10,size,error)||size!=count||!field(memory,object,offset+0x14,cap,error)||cap!=capacity)return fail(error,error.empty()?"Ability owned store changed during read":error);}
    return true;
}
a::CharacterRenameIntent owner_intent(const a::AbilityEditIntent& intent){a::CharacterRenameIntent out;out.kind=intent.kind;out.id=intent.id;return out;}
bool sample(Memory& memory,const a::Binding& binding,const std::string& snapshot,const a::AbilityEditIntent& intent,Sample& out,std::string& error){
    out={};const auto owner=owner_intent(intent);if(!character_rename::sample(memory,binding,snapshot,owner,out.character,error))return false;out.value.owner=out.character.value;
    json rows=json::array();std::set<uintptr_t> pointers;bool found=false;
    for(size_t kind=0;kind<5;++kind){const auto& types=out.character.arrays[kind];
        for(size_t id=0;id<types[1];++id){uintptr_t type=0,data=0;int32_t count=0,capacity=0;
            if(!field(memory,types[0],id*8,type,error)||!field(memory,type,0x130,data,error)||!field(memory,type,0x138,count,error)||!field(memory,type,0x13c,capacity,error)||count<0||count>10000||capacity<count||(count&&!data))return fail(error,error.empty()?"Ability actual CharacterType slot bounds differ":error);
            for(int32_t slot=0;slot<count;++slot){uintptr_t object=0,proxy=0;uint64_t uid=0;a::AbilityRecord record;
                if(!field(memory,data,size_t(slot)*8,object,error)||!object||!pointers.insert(object).second||!field(memory,object,0x18,uid,error)||!uid||!field(memory,object,0x40,proxy,error)||!read_record(memory,object,record,error))return fail(error,error.empty()?"Ability prototype current owner/full UID differs":error);
                if(proxy){uintptr_t target=0;if(!field(memory,proxy,0,target,error)||target!=object)return fail(error,error.empty()?"Ability native weak proxy is not live":error);}
                out.local.push_back({object,uintptr_t(uid),proxy});rows.push_back({{"kind",kind},{"id",id},{"slot",slot},{"record",record_json(record)}});++out.value.count;
                if(int32_t(kind)==intent.kind&&int32_t(id)==intent.id&&slot==intent.slot){found=true;out.prototype=object;out.uid=uid;out.proxy=proxy;out.value.record=record;}}
            uintptr_t again=0;int32_t size=0,cap=0;if(!field(memory,type,0x130,again,error)||again!=data||!field(memory,type,0x138,size,error)||size!=count||!field(memory,type,0x13c,cap,error)||cap!=capacity)return fail(error,error.empty()?"Ability actual owner slots changed during read":error);}}
    if(!found)return fail(error,"Ability current selected canonical owner/slot absent");return quest_bridge::sha256(rows.dump(),out.value.all_sha256,error);
}
uint32_t find_window(uintptr_t& window)noexcept{__try{const quest_list::QuestTitleView name{"ability_edit",12};window=reinterpret_cast<uintptr_t(*)(const quest_list::QuestTitleView&)>(state.image+0x81aa90)(name);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool window(Memory& memory,uintptr_t& outer,uintptr_t& live,uintptr_t& draft,uintptr_t& owner,std::string& error){
    const auto seh=find_window(outer);uintptr_t ap=0,proxy=0,target=0;
    if(seh||!outer||!field(memory,outer,0,ap,error)||ap!=state.image+0x1720f30||!field(memory,outer,0x520,live,error)||!field(memory,outer,0x510,draft,error)||
       !field(memory,outer,0x530,owner,error)||!field(memory,outer,0x528,proxy,error)||(live&&(!proxy||!field(memory,proxy,0,target,error)||target!=live)))return fail(error,error.empty()?"Ability exact named Window/current weak binding differs SEH="+std::to_string(seh):error);
    if(draft){if(draft==live||!field(memory,draft,0,ap,error)||ap!=state.image+0x171c930)return fail(error,error.empty()?"Ability native owned editor draft differs":error);}
    return true;
}
uint32_t simplify(uintptr_t draft)noexcept{__try{reinterpret_cast<void(*)(void*)>(state.image+0x14e90)(reinterpret_cast<void*>(draft));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t setup(uintptr_t outer,uintptr_t prototype,uintptr_t owner)noexcept{__try{reinterpret_cast<void(*)(void*,void*,void*)>(state.image+0x570a80)(reinterpret_cast<void*>(outer),reinterpret_cast<void*>(prototype),reinterpret_cast<void*>(owner));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
uint32_t prepare(uintptr_t draft,const a::AbilityRecord& record)noexcept{
    __try{
        for(size_t group=0;group<2;++group){const size_t offset=group?0x1c0:0x190;const auto& rows=group?record.effects:record.costs;
            int32_t count=*reinterpret_cast<const int32_t*>(draft+offset+0x10);
            while(size_t(count)>rows.size()){const auto data=*reinterpret_cast<const uintptr_t*>(draft+offset+8);const auto effect=*reinterpret_cast<const uintptr_t*>(data+size_t(count-1)*8);
                reinterpret_cast<void(*)(void*,void*)>(state.image+(group?0x14dd0:0x14d40))(reinterpret_cast<void*>(draft),reinterpret_cast<void*>(effect));count=*reinterpret_cast<const int32_t*>(draft+offset+0x10);}
            while(size_t(count)<rows.size()){const auto effect=reinterpret_cast<uintptr_t(*)(size_t)>(state.image+0xa02670)(0x38);
                reinterpret_cast<void(*)(void*)>(state.image+0x67e0)(reinterpret_cast<void*>(effect));
                reinterpret_cast<void(*)(void*,void*)>(state.image+(group?0x14d10:0x14ce0))(reinterpret_cast<void*>(draft),reinterpret_cast<void*>(effect));count=*reinterpret_cast<const int32_t*>(draft+offset+0x10);}
            const auto data=*reinterpret_cast<const uintptr_t*>(draft+offset+8);
            for(size_t i=0;i<rows.size();++i){const auto effect=*reinterpret_cast<const uintptr_t*>(data+i*8);std::array<unsigned char,0x38> source{};
                std::memcpy(source.data()+0x20,rows[i].integers.data(),12);std::memcpy(source.data()+0x2c,rows[i].values.data(),8);std::memcpy(source.data()+0x34,&rows[i].count,4);
                reinterpret_cast<void*(*)(void*,const void*)>(state.image+0x6820)(reinterpret_cast<void*>(effect),source.data());}}
        for(size_t i=0;i<8;++i){const quest_list::QuestTitleView text{record.text[i].c_str(),record.text[i].size()};reinterpret_cast<void(*)(void*,const quest_list::QuestTitleView&)>(state.image+0x11bfce0)(reinterpret_cast<void*>(draft+string_offsets[i]),text);}
        std::memcpy(reinterpret_cast<void*>(draft+0xe8),&record.item_value,4);std::memcpy(reinterpret_cast<void*>(draft+0xec),&record.item_kind,4);*reinterpret_cast<uint8_t*>(draft+0xf0)=record.item_flag?1:0;
        std::memcpy(reinterpret_cast<void*>(draft+0x15c),record.selectors.data(),20);std::memcpy(reinterpret_cast<void*>(draft+0x170),record.color.data(),16);std::memcpy(reinterpret_cast<void*>(draft+0x180),record.timing.data(),12);
        for(size_t i=0;i<5;++i)*reinterpret_cast<uint8_t*>(draft+0x1f0+i)=record.flags[i]?1:0;
        std::memcpy(reinterpret_cast<void*>(draft+0x1f8),record.final_values.data(),8);std::memcpy(reinterpret_cast<void*>(draft+0x200),&record.level,4);*reinterpret_cast<uint8_t*>(draft+0x204)=record.final_flag?1:0;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool same_local(const Sample& before,const Sample& after){return character_rename::same_local(before.character,after.character)&&before.prototype==after.prototype&&before.uid==after.uid&&before.proxy==after.proxy&&before.local==after.local;}
bool same_menu_identity(const Sample& before,const Sample& after){
    if(!character_rename::same_local(before.character,after.character)||before.prototype!=after.prototype||before.uid!=after.uid||before.local.size()!=after.local.size())return false;
    for(size_t i=0;i<before.local.size();++i)if(before.local[i][0]!=after.local[i][0]||before.local[i][1]!=after.local[i][1])return false;
    return true;
}
#include "ability_adjustment_runtime.inc"
bool entry(void* receiver,void* record)try{
    if(!original){fault=true;return reject("Ability original unavailable");}if(!net::session_wire::connected())return original(receiver,record);
    Memory memory;std::string error;a::ui_diagnostic::Api api;bool relevant=false;if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error)){fault=true;return reject(error);}
    auto seh=quest_list::label(api.label_equals,record,"Apply",relevant);if(seh){fault=true;return reject("Ability Apply label SEH="+std::to_string(seh));}if(!relevant)return original(receiver,record);
    if(replaying){if(uintptr_t(receiver)!=replay_receiver||original_calls){fault=true;return reject("Ability original-once receiver/count differs");}++original_calls;return original(receiver,record);}
    const auto controller=typed_action_runtime::controller_status();a::Binding binding;const auto snapshot=net::current_snapshot_id();
    if(!ready||fault||pending||costume_edit::pending_ui()||dungeon_config::pending_ui()||dungeon_sound::pending_ui()||dungeon_gizmo::pending_ui()||dungeon_palette::pending_ui()||dungeon_light::pending_ui()||building_status::pending_ui()||quest_list::pending_ui()||character_rename::pending_ui()||GetCurrentThreadId()!=state.thread||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||!code(memory,error)||!typed_action_runtime::network_binding(snapshot,binding,error))return reject(error.empty()?"Ability capture requires idle bound Mainthread Controller":error);
    uintptr_t outer=0,live=0,draft=0,owner=0;if(!window(memory,outer,live,draft,owner,error)||outer!=uintptr_t(receiver)||!live||!draft)return reject(error.empty()?"Ability captured Window current draft absent":error);
    if(!owner)return original(receiver,record);
    a::AbilityEditIntent intent;if(!field(memory,owner,0x2c,intent.kind,error)||!field(memory,owner,0x30,intent.id,error))return reject(error);
    uintptr_t data=0;int32_t count=0;if(!field(memory,owner,0x130,data,error)||!field(memory,owner,0x138,count,error)||count<0||count>10000)return reject(error.empty()?"Ability captured owner slot bounds differ":error);
    for(int32_t i=0;i<count;++i){uintptr_t object=0;if(!field(memory,data,size_t(i)*8,object,error))return reject(error);if(object==live){if(intent.slot!=-1)return reject("Ability selected prototype has duplicate owner slots");intent.slot=i;}}
    if(intent.slot<0)return reject("Ability captured selected prototype is not in its actual Character owner slots");
    seh=simplify(draft);if(seh){fault=true;return reject("Ability normal owned draft Simplify SEH="+std::to_string(seh));}
    Sample before;if(!read_record(memory,draft,intent.record,error)||!sample(memory,binding,snapshot,intent,before,error)||before.prototype!=live||before.character.selected!=owner)return reject(error.empty()?"Ability captured owner/draft census differs":error);
    if(intent.record.effects.empty())return original(receiver,record);
    if(!quest_bridge::sha256(record_json(before.value.record).dump(),intent.before_sha256,error))return reject(error);pending=Pending{binding,snapshot,intent};++captures;return true;
}catch(const std::exception& exception){fault=true;return reject(std::string("Ability capture boundary: ")+exception.what());}catch(...){fault=true;return reject("Ability capture non-standard exception");}
}
bool capture_adjustment(void* receiver,void* record,bool& handled,std::string& error)try{
    const bool accepted=capture_adjustment_impl(receiver,record,handled,error);if(!accepted)reject(error);return accepted;
}catch(const std::exception& exception){fault=true;error=std::string("Ability adjustment capture boundary: ")+exception.what();reject(error);return false;}
catch(...){fault=true;error="Ability adjustment capture non-standard exception";reject(error);return false;}
bool pending_ui(){return pending.has_value()||replaying||view_tail.armed;}
bool retire_lobby(std::string& e){
    if(!ready){e.clear();return true;}
    if(GetCurrentThreadId()!=state.thread)return fail(e,"Ability edit lobby retirement requires Mainthread");
    native::WorldStatus world;if(!native::read_world_status(world,e))return false;
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0)return fail(e,"Ability edit lobby retirement requires actual idle Title");
    if(replaying||view_tail.armed)return fail(e,"Ability edit lobby retirement cannot interrupt original execution");
    pending.reset();operations.clear();adjustment_views.clear();view_tail={};replay_receiver=0;original_calls=0;
    fault=false;cause.clear();last_action.clear();e.clear();return true;
}
bool enable(std::string& error){
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(error,"Ability edit requires bound Character Mainthread");
    if(ready){for(const auto& [id,op]:operations)if(!op.retired)return fail(error,"Ability edit retained unretired operation="+id);if(fault||pending_ui())return fail(error,"Ability edit retained unresolved capture");Memory memory;if(!code(memory,error)||!view_route(memory,error))return false;operations.clear();adjustment_views.clear();return true;}
    if(installed)return fail(error,"Ability edit earlier installation failed: "+cause);installed=true;Memory memory;if(!code(memory,error))return false;
    auto status=MH_CreateHook(reinterpret_cast<void*>(state.image+ui_code.rva),reinterpret_cast<void*>(&entry),reinterpret_cast<void**>(&original));
    if(status!=MH_OK||!original)return fail(error,"Ability edit prepare failed status="+std::to_string(status)+" cause="+mt2mp::hooks::framework::process_backend().error());
    status=MH_EnableHook(reinterpret_cast<void*>(state.image+ui_code.rva));if(status!=MH_OK)return fail(error,"Ability edit enable failed status="+std::to_string(status));
    MT2FW_MT2_RouteReceipt receipt{sizeof receipt};if(!quest_list::read_route(state.image+ui_code.rva,receipt,error))return false;generation=receipt.generation;
    status=MH_CreateHook(reinterpret_cast<void*>(state.image+0x689100),reinterpret_cast<void*>(&view_select_entry),reinterpret_cast<void**>(&original_view_select));
    if(status!=MH_OK||!original_view_select)return fail(error,"Ability adjustment presentation prepare failed status="+std::to_string(status)+" cause="+mt2mp::hooks::framework::process_backend().error());
    status=MH_EnableHook(reinterpret_cast<void*>(state.image+0x689100));
    if(status!=MH_OK)return fail(error,"Ability adjustment presentation enable failed status="+std::to_string(status));
    if(!quest_list::read_route(state.image+0x689100,receipt,error))return false;view_generation=receipt.generation;
    if(!code(memory,error)||!view_route(memory,error))return false;ready=true;return true;
}
void tick(){if(!pending)return;const auto saved=*pending;pending.reset();std::string error;a::Binding binding;
    if(!ready||fault||saved.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(saved.snapshot,binding,error)||binding!=saved.binding){fault=true;reject(error.empty()?"Ability captured world changed":error);return;}
    const auto id=action_runtime::uuid(error);if(!id.empty()&&saved.intent.adjustment)adjustment_views.emplace(id,saved.view);if(id.empty()||!typed_action_runtime::submit_ability_edit(id,saved.intent,error)){fault=true;reject(error);return;}++submitted;last_action=id;
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    if(!ready||fault||GetCurrentThreadId()!=state.thread||request.action!=a::Action::EditCharacterAbility){error="Ability edit runtime unavailable";return a::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="Ability edit already attempted; no replay";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();if(!controller.value("pending",false)||controller.value("locked",true)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Ability edit requires exact pending Controller";return a::Outcome::RejectedNoEffect;}
    Memory memory;Sample before;const auto& intent=request.ability_edit;std::string prior_sha;
    if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,intent,before,error)||!quest_bridge::sha256(record_json(before.value.record).dump(),prior_sha,error))return a::Outcome::RejectedNoEffect;
    if(prior_sha!=intent.before_sha256){error="Ability edit actual current record differs from captured prior";return a::Outcome::RejectedNoEffect;}
    if(!a::ability_edit_wire_fits(request,before.value,before.character.money,error))return a::Outcome::RejectedNoEffect;
    if(authority){auto portable=before.value;portable.owner.global_random=authority->ability_edit_before.owner.global_random;if(portable!=authority->ability_edit_before||before.character.money!=authority->before){error="Ability Client actual prior owner/census/ledger differs from Host";return a::Outcome::RejectedNoEffect;}}
    if(intent.adjustment)return perform_adjustment(memory,binding,request,authority,before,error);
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;
    const auto unknown=[&](const std::string& why){replaying=false;replay_receiver=0;error=op.cause=why.empty()?"Ability edit failed without cause":why;return a::Outcome::Unknown;};
    uintptr_t outer=0,live=0,draft=0,owner=0;if(!window(memory,outer,live,draft,owner,error))return unknown(error);
    op.prepared=true;auto seh=setup(outer,before.prototype,before.character.selected);if(seh)return unknown("Ability original owned draft SetupForCharacterType SEH="+std::to_string(seh));
    if(!window(memory,outer,live,draft,owner,error)||live!=before.prototype||owner!=before.character.selected||!draft)return unknown(error.empty()?"Ability original prepared owner/draft differs":error);
    seh=prepare(draft,intent.record);if(seh)return unknown("Ability original owned draft preparation SEH="+std::to_string(seh));
    a::AbilityRecord actual;if(!read_record(memory,draft,actual,error)||actual!=intent.record)return unknown(error.empty()?"Ability prepared actual owned draft differs":error);
    seh=simplify(draft);if(seh)return unknown("Ability prepared original Simplify SEH="+std::to_string(seh));if(!read_record(memory,draft,actual,error)||actual!=intent.record)return unknown(error.empty()?"Ability prepared draft is not original Simplify canonical form":error);
    Sample prepared;if(!sample(memory,binding,request.snapshot_uuid,intent,prepared,error))return unknown(error);auto portable=prepared.value;portable.owner.global_random=before.value.owner.global_random;
    if(!same_menu_identity(before,prepared)||portable!=before.value)return unknown("Ability owned UI preparation changed live gameplay");
    const auto desired_rng=authority?authority->ability_edit_before.owner.global_random:before.value.owner.global_random;op.seed_attempted=true;const auto seed=action_runtime::seed(state.image,desired_rng);if(seed)return unknown("Ability Host RNG adoption SEH="+std::to_string(seed));
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.before,error)||op.before.value.owner.global_random!=desired_rng)return unknown(error.empty()?"Ability Host RNG readback differs":error);
    op.evidence.before=op.before.character.money;op.evidence.ability_edit_before=op.before.value;
    a::ui_diagnostic::Api api;a::ui_diagnostic::Window command_window;command_window.window=outer;if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return unknown(error);api.command=reinterpret_cast<Command>(state.image+ui_code.rva);api.destroy_label="Apply";
    a::ui_diagnostic::Receipt receipt;op.attempted=true;replaying=true;replay_receiver=outer;original_calls=0;const bool returned=a::ui_diagnostic::process_calls().command(api,command_window,receipt,error);op.calls=original_calls;op.returned=receipt.ui_command_returned;replaying=false;replay_receiver=0;
    if(!returned||!op.returned||op.calls!=1||!receipt.record_cleanup_confirmed)return unknown(error.empty()?"Ability original transaction lacks exact once/cleanup receipt":error);
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.after,error)||!same_local(op.before,op.after)||!a::ability_edit_transition(intent,op.before.value,op.after.value))return unknown(error.empty()?"Ability original effect/owner/census/ledger/Undo differs":error);
    op.evidence.after=op.after.character.money;op.evidence.ability_edit_after=op.after.value;if(authority){auto expected=*authority;expected.retirement_confirmed=false;if(op.evidence!=expected)return unknown("Ability Client original result differs from Host");}
    op.confirmed=true;return a::Outcome::Confirmed;
}catch(const std::exception& exception){replaying=false;replay_receiver=0;error=std::string("Ability perform boundary: ")+exception.what();return a::Outcome::Unknown;}catch(...){replaying=false;replay_receiver=0;error="Ability perform non-standard exception";return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& result,std::string& error){const auto found=operations.find(request.action_uuid);
    if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.returned||found->second.calls!=1)return fail(error,"Ability observation lacks exact confirmed original-once receipt");
    const auto& op=found->second;Memory memory;Sample now;if(!sample(memory,binding,request.snapshot_uuid,request.ability_edit,now,error)||!same_local(op.after,now)||now.value!=op.after.value)return fail(error,error.empty()?"Ability current result differs before ACK":error);result=op.evidence;result.retirement_confirmed=op.retired;return true;}
bool retire(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence ignored;if(!observe(binding,request,ignored,error))return false;auto& op=operations.at(request.action_uuid);if(op.retired)return fail(error,"Ability receipt already retired");op.retired=true;return true;}
json status(){json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"prepared",op.prepared},{"attempted",op.attempted},{"returned",op.returned},{"original_calls",op.calls},{"confirmed",op.confirmed},{"retired",op.retired},{"seed_attempted",op.seed_attempted},{"cause",op.cause}};return {{"enabled",ready},{"fault",fault},{"pending_ui",pending_ui()},{"captures",captures},{"submitted",submitted},{"rejected",rejected},{"last_action",last_action},{"cause",cause},{"operations",rows}};}
}
