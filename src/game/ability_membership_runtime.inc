namespace ability_membership {
namespace {
using Command=a::AbilityMembershipCommand;
using New=bool(*)(void*,void*);using Remove=void(*)(void*);using Dock=void(*)(void*,void*,float,bool);
New original_new=nullptr;Remove original_remove=nullptr;Dock original_dock=nullptr;
std::array<uint64_t,3> generations{};bool installed=false,ready=false,fault=false,executing=false;
uint64_t captures=0,submitted=0,rejected=0;std::string cause,last_action;
struct Sample {character_membership::Sample current;a::AbilityMembershipState value;std::vector<std::array<uintptr_t,3>> local;};
struct Pending {a::Binding binding;std::string snapshot;a::AbilityMembershipIntent intent;};
struct NativeObjects:ability_receiver_cleanup::State {
    alignas(16) std::array<unsigned char,0x348> view{};
    alignas(16) std::array<unsigned char,0x390> slot{};
    a::ui_diagnostic::NativeRecordStorage record{};
    uint64_t source_uid=0;ability_receiver_cleanup::Receipt cleanup;
    bool primary_attempted=false,primary_returned=false,primary_accepted=false;
    uint32_t primary_exception=0,cleanup_exception=0;
};
struct Operation {a::Binding binding;a::Request request;Sample before,after;a::Evidence evidence;
    bool seed_attempted=false,attempted=false,returned=false,confirmed=false,retired=false;uint32_t calls=0;
    NativeObjects native;std::string cause;};
std::optional<Pending> pending;std::map<std::string,Operation> operations;
bool new_entry(void*,void*);void remove_entry(void*);void dock_entry(void*,void*,float,bool);
bool reject(const std::string& error){++rejected;if(cause!=error){cause=error;LOG_ERROR("[AbilityMembership] {}",error);}return false;}
bool code(Memory& memory,std::string& error){
    if(!character_membership::code(memory,error)||!ability_edit::code(memory,error))return false;
    const std::array<uintptr_t,3> rvas{0x651990,0x565400,0x567d70};
    const std::array<uintptr_t,3> originals{uintptr_t(original_new),uintptr_t(original_remove),uintptr_t(original_dock)};
    const std::array<uintptr_t,3> detours{uintptr_t(&new_entry),uintptr_t(&remove_entry),uintptr_t(&dock_entry)};
    constexpr std::array<size_t,3> copied{6,11,6};
    for(const auto& pin:ability_membership_codes::helpers){std::vector<unsigned char> bytes(pin.bytes);
        if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error))return false;
        const auto route=std::find(rvas.begin(),rvas.end(),pin.rva);
        if(route!=rvas.end()){const size_t index=size_t(route-rvas.begin());
            if(originals[index]){MT2FW_MT2_RouteReceipt receipt{sizeof receipt};
                if(!quest_list::read_route(state.image+pin.rva,receipt,error))return false;
                if(receipt.state!=MT2FW_MT2_ROUTE_ENABLED||receipt.detour!=detours[index]||receipt.trampoline!=originals[index]||
                   receipt.generation!=generations[index]||std::memcmp(bytes.data(),receipt.current,32))
                    return fail(error,"Ability membership exact current route ownership differs RVA="+std::to_string(pin.rva));
                std::vector<unsigned char> trampoline(copied[index]+14);uintptr_t resume=0;
                constexpr std::array<unsigned char,6> continuation{0xff,0x25,0,0,0,0};
                if(!memory.copy(originals[index],trampoline.data(),trampoline.size(),error))return false;
                std::memcpy(&resume,trampoline.data()+copied[index]+6,8);
                if(!std::equal(receipt.pristine,receipt.pristine+copied[index],trampoline.begin())||
                   !std::equal(continuation.begin(),continuation.end(),trampoline.begin()+copied[index])||resume!=state.image+pin.rva+copied[index])
                    return fail(error,"Ability membership exact original instruction boundary differs RVA="+std::to_string(pin.rva));
                std::memcpy(bytes.data(),receipt.pristine,32);
                if(!quest_list::verify_code(state.image+pin.rva,bytes.data(),bytes.size(),error))return false;}}
        if(mt2mp::sha256(bytes)!=pin.sha)return fail(error,"Ability membership full original body differs RVA="+std::to_string(pin.rva));
    }return true;
}
json slots_json(const a::AbilityMembershipState& value){json rows=json::array();
    for(const auto& slot:value.slots)rows.push_back({{"record",ability_edit::record_json(slot.record)},{"display_slot",slot.display_slot}});return rows;
}
bool digest(const Sample& sample,std::string& sha,std::string& error){
    return quest_bridge::sha256(json{{"manager",sample.value.complete.manager_sha},{"slots",slots_json(sample.value)}}.dump(),sha,error);
}
bool sample(Memory& memory,const a::Binding& binding,const std::string& snapshot,const a::AbilityMembershipIntent& intent,
    Sample& out,std::string& error,const a::Request* request=nullptr){
    out={};a::CharacterMembershipIntent owner;owner.kind=intent.kind;owner.id=intent.id;
    if(!character_membership::sample(memory,binding,snapshot,owner,out.current,error,nullptr,request))return false;
    if(out.current.value.selected.deleted)return fail(error,"Ability membership selected Character definition is deleted");
    out.value.complete=out.current.value;uintptr_t data=0;int32_t count=0,capacity=0;
    if(!field(memory,out.current.selected,0x130,data,error)||!field(memory,out.current.selected,0x138,count,error)||
       !field(memory,out.current.selected,0x13c,capacity,error))return false;
    if(count<0||count>10000||capacity<count||(count&&!data))return fail(error,"Ability membership actual owner-array bounds differ");
    std::set<uintptr_t> seen;
    for(int32_t i=0;i<count;++i){uintptr_t object=0,proxy=0,registered=0;uint64_t uid=0;a::AbilityMembershipSlot slot;
        if(!field(memory,data,size_t(i)*8,object,error))return false;
        if(!object||!seen.insert(object).second)return fail(error,"Ability current native owner prototype is absent or duplicated");
        if(!field(memory,object,0x18,uid,error)||
           !field(memory,object,0x40,proxy,error)||!field(memory,object,0x208,slot.display_slot,error)||
           !ability_edit::read_record(memory,object,slot.record,error))return false;
        const auto seh=lookup(out.current.api.find,uid,registered);
        if(seh||!uid||registered!=object||slot.display_slot< -1)return fail(error,"Ability membership current prototype full UID/order differs SEH="+std::to_string(seh));
        if(proxy){uintptr_t target=0;if(!field(memory,proxy,0,target,error))return false;
            if(target!=object)return fail(error,"Ability membership actual prototype weak proxy differs");}
        out.local.push_back({object,uintptr_t(uid),proxy});out.value.slots.push_back(std::move(slot));}
    uintptr_t final_data=0;int32_t final_count=0,final_capacity=0;
    if(!field(memory,out.current.selected,0x130,final_data,error)||!field(memory,out.current.selected,0x138,final_count,error)||
       !field(memory,out.current.selected,0x13c,final_capacity,error))return false;
    if(final_data!=data||final_count!=count||final_capacity!=capacity)return fail(error,"Ability membership actual owner array changed during read");return true;
}
struct GameString {uintptr_t data=0;size_t size=0;std::array<unsigned char,16> storage{};};
uint32_t query_name(uintptr_t owner,const quest_list::QuestTitleView& input,GameString& result)noexcept{
    __try{reinterpret_cast<void(*)(void*,void*,const quest_list::QuestTitleView&)>(state.image+0x95300)(&result,reinterpret_cast<void*>(owner),input);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t destroy_string(GameString& value)noexcept{
    __try{reinterpret_cast<void(*)(void*)>(state.image+0x11bd1e0)(&value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool copy_name(Memory& memory,uintptr_t owner,const std::string& source,std::string& result,std::string& error){
    const quest_list::QuestTitleView input{source.data(),source.size()};GameString value;const auto seh=query_name(owner,input,value);
    if(seh)return fail(error,"Ability original unique-name query SEH="+std::to_string(seh));
    const bool read=npc_quest_records::read_string(memory,uintptr_t(&value),0,"Ability original unique-name result",result,error);
    const auto cleanup=destroy_string(value);
    if(cleanup)return fail(error,(error.empty()?std::string{}:error+"; ")+"Ability original unique-name result cleanup SEH="+std::to_string(cleanup));
    if(!read)return false;return (!result.empty()&&result.size()<=4096)||fail(error,"Ability original unique name exceeds the existing authored text bound");
}
bool icon(Memory& memory,uintptr_t object,uintptr_t& prototype,uintptr_t& owner,bool& prefab,std::string& error){
    uintptr_t ap=0,proxy=0,target=0;uint8_t flag=2;
    if(!field(memory,object,0,ap,error)||!field(memory,object,0x320,prototype,error)||!field(memory,object,0x328,proxy,error)||
       !field(memory,object,0x330,owner,error)||!field(memory,object,0x399,flag,error))return false;
    constexpr uintptr_t icon_ap=0x1717790;
    if(ap!=state.image+icon_ap||!prototype||!proxy||flag>1||!field(memory,proxy,0,target,error)||target!=prototype)
        return fail(error,error.empty()?"Ability captured concrete Icon/weak binding differs":error);
    prefab=flag!=0;return true;
}
bool queue(uintptr_t owner,a::AbilityMembershipIntent intent,uintptr_t prototype,std::string& error){
    const auto controller=typed_action_runtime::controller_status();a::Binding binding;const auto snapshot=net::current_snapshot_id();Memory memory;
    if(!ready||fault||pending||executing||GetCurrentThreadId()!=state.thread||!controller.value("started",false)||
       controller.value("locked",true)||controller.value("pending",true)||!code(memory,error)||!typed_action_runtime::network_binding(snapshot,binding,error))
        return fail(error,error.empty()?"Ability membership capture requires idle bound Mainthread Controller":error);
    if(!field(memory,owner,0x2c,intent.kind,error)||!field(memory,owner,0x30,intent.id,error))return false;
    Sample before;if(!sample(memory,binding,snapshot,intent,before,error)||before.current.selected!=owner)
        return fail(error,error.empty()?"Ability captured Character owner differs from its current global definition":error);
    if(intent.command==Command::Remove||intent.command==Command::Dock){
        for(size_t i=0;i<before.local.size();++i)if(before.local[i][0]==prototype){if(intent.slot!=-1)return fail(error,"Ability captured prototype has duplicate current owner slots");intent.slot=int32_t(i);}
        if(intent.slot<0)return fail(error,"Ability captured prototype is absent from its current Character owner");}
    if(intent.command==Command::Create&&before.local.size()>=12)return fail(error,"Ability native NewAbility limit reached: current="+std::to_string(before.local.size())+" maximum=12");
    if(intent.command==Command::Copy&&(!ability_edit::read_record(memory,prototype,intent.prefab,error)||
       !copy_name(memory,owner,intent.prefab.text[0],intent.copy_name,error)))return false;
    if(!digest(before,intent.before_sha256,error))return false;
    pending=Pending{binding,snapshot,std::move(intent)};++captures;return true;
}
bool new_entry(void* receiver,void* record)try{
    if(!original_new){fault=true;return reject("Ability New original is unavailable");}
    if(!net::session_wire::connected()||executing)return original_new(receiver,record);
    Memory memory;std::string error;a::ui_diagnostic::Api api;bool relevant=false;
    if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error)){fault=true;return reject(error);}
    const auto seh=quest_list::label(api.label_equals,record,"NewAbility",relevant);
    if(seh){fault=true;return reject("Ability New label SEH="+std::to_string(seh));}if(!relevant)return original_new(receiver,record);
    uintptr_t tokens=0,owner=0,ap=0;int32_t count=0,capacity=0,type=0,level=0;std::string label;
    if(!field(memory,uintptr_t(receiver),0,ap,error)||!field(memory,uintptr_t(receiver),0x340,owner,error)||
       !field(memory,uintptr_t(record),0x38,tokens,error)||!field(memory,uintptr_t(record),0x40,count,error)||
       !field(memory,uintptr_t(record),0x44,capacity,error))return reject(error);
    if(ap!=state.image+0x171ca70||count!=2||capacity<count||!tokens)return reject("Ability New actual view/token bounds differ");
    if(!npc_quest_records::read_string(memory,tokens,8,"Ability New first native token",label,error)||!field(memory,tokens,0x30,type,error)||
       !field(memory,tokens,0x58,level,error))return reject(error);
    if(label!="Level"||type!=4||level<0)return reject("Ability New requires the original Level integer command");
    a::AbilityMembershipIntent intent;intent.command=Command::Create;intent.level=level;
    return queue(owner,std::move(intent),0,error)||reject(error);
}catch(const std::exception& exception){fault=true;return reject(std::string("Ability New capture boundary: ")+exception.what());}
catch(...){fault=true;return reject("Ability New capture non-standard exception");}
void remove_entry(void* receiver)try{
    if(!original_remove){fault=true;reject("Ability Remove original is unavailable");return;}
    if(!net::session_wire::connected()||executing){original_remove(receiver);return;}
    Memory memory;uintptr_t prototype=0,owner=0;bool prefab=false;std::string error;
    if(!icon(memory,uintptr_t(receiver),prototype,owner,prefab,error)){reject(error);return;}
    if(prefab){original_remove(receiver);return;}
    a::AbilityMembershipIntent intent;intent.command=Command::Remove;if(!queue(owner,std::move(intent),prototype,error))reject(error);
}catch(const std::exception& exception){fault=true;reject(std::string("Ability Remove capture boundary: ")+exception.what());}
catch(...){fault=true;reject("Ability Remove capture non-standard exception");}
void dock_entry(void* receiver,void* button,float seconds,bool animate)try{
    if(!original_dock){fault=true;reject("Ability Dock original is unavailable");return;}
    if(!net::session_wire::connected()||executing||!button){original_dock(receiver,button,seconds,animate);return;}
    Memory memory;uintptr_t owner=0,source_owner=0,prototype=0,ap=0;bool prefab=false;std::string error;
    if(!field(memory,uintptr_t(receiver),0,ap,error)||!field(memory,uintptr_t(receiver),0x340,owner,error)){reject(error);return;}
    if(!owner){original_dock(receiver,button,seconds,animate);return;}
    if(ap!=state.image+0x1717a00||!icon(memory,uintptr_t(button),prototype,source_owner,prefab,error)){reject(error.empty()?"Ability Dock actual native slot class differs":error);return;}
    if(!prefab&&source_owner!=owner){reject("Ability Dock non-prefab source has a different actual Character owner");return;}
    a::AbilityMembershipIntent intent;intent.command=prefab?Command::Copy:Command::Dock;
    if(!field(memory,uintptr_t(receiver),0x348,intent.display_slot,error)||!field(memory,uintptr_t(receiver),0x34c,intent.level,error)){reject(error);return;}
    if(intent.display_slot<0||intent.level<0){reject("Ability Dock original authored display slot/level is negative");return;}
    if(!prefab){int32_t old_slot=-1,old_level=-1;if(!field(memory,prototype,0x208,old_slot,error)||!field(memory,prototype,0x200,old_level,error)){reject(error);return;}
        if(old_slot==intent.display_slot&&old_level==intent.level){original_dock(receiver,button,seconds,animate);return;}}
    if(!queue(owner,std::move(intent),prototype,error))reject(error);
}catch(const std::exception& exception){fault=true;reject(std::string("Ability Dock capture boundary: ")+exception.what());}
catch(...){fault=true;reject("Ability Dock capture non-standard exception");}
}
bool pending_ui(){return pending.has_value()||executing;}
bool enable_impl(std::string& error){
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(error,"Ability membership requires its installed current Mainthread");
    if(ready){for(const auto& [id,op]:operations)if(!op.retired)return fail(error,"Ability membership retained unretired operation="+id);
        if(fault||pending_ui())return fail(error,"Ability membership retained unresolved capture");Memory memory;if(!code(memory,error))return false;operations.clear();return true;}
    if(installed)return fail(error,"Ability membership previous installation failed: "+cause);installed=true;Memory memory;if(!code(memory,error))return false;
    const std::array<uintptr_t,3> rvas{0x651990,0x565400,0x567d70};
    const std::array<void*,3> detours{reinterpret_cast<void*>(&new_entry),reinterpret_cast<void*>(&remove_entry),reinterpret_cast<void*>(&dock_entry)};
    const std::array<void**,3> originals{reinterpret_cast<void**>(&original_new),reinterpret_cast<void**>(&original_remove),reinterpret_cast<void**>(&original_dock)};
    for(size_t i=0;i<rvas.size();++i){const auto status=MH_CreateHook(reinterpret_cast<void*>(state.image+rvas[i]),detours[i],originals[i]);
        if(status!=MH_OK||!*originals[i])return fail(error,"Ability membership original route prepare failed RVA="+std::to_string(rvas[i])+" status="+std::to_string(status)+" cause="+mt2mp::hooks::framework::process_backend().error());
        MT2FW_MT2_RouteReceipt receipt{sizeof receipt};if(!quest_list::read_route(state.image+rvas[i],receipt,error))return false;generations[i]=receipt.generation;
        const auto enabled=MH_EnableHook(reinterpret_cast<void*>(state.image+rvas[i]));
        if(enabled!=MH_OK)return fail(error,"Ability membership original route enable failed RVA="+std::to_string(rvas[i])+" status="+std::to_string(enabled));}
    if(!code(memory,error))return false;ready=true;return true;
}
bool enable(std::string& error){const bool enabled=enable_impl(error);
    if(!enabled&&cause!=error){cause=error;LOG_ERROR("[AbilityMembership] initialization failed: {}",error);}return enabled;
}
bool retire_lobby(std::string& error){
    if(!ready){error.clear();return true;}native::WorldStatus world;
    if(GetCurrentThreadId()!=state.thread||!native::read_world_status(world,error))return fail(error,error.empty()?"Ability membership lobby retirement requires actual Mainthread":error);
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0||executing)
        return fail(error,"Ability membership lobby retirement requires idle actual Title");
    pending.reset();operations.clear();fault=false;cause.clear();last_action.clear();error.clear();return true;
}
void tick(){if(!pending)return;const auto captured=*pending;pending.reset();a::Binding actual;std::string error;
    if(fault||captured.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(captured.snapshot,actual,error)||actual!=captured.binding){fault=true;reject(error.empty()?"Ability membership captured World changed":error);return;}
    const auto id=action_runtime::uuid(error);if(id.empty()){fault=true;reject(error);return;}
    if(!typed_action_runtime::submit_ability_membership(id,captured.intent,error)){fault=true;reject(error);return;}++submitted;last_action=id;
}
namespace {
uint32_t construct_native(NativeObjects& objects,Command command,uintptr_t owner,uintptr_t prototype,int32_t level,int32_t display_slot,const char* text)noexcept{
    __try{
        if(command==Command::Create){std::memcpy(objects.view.data()+0x340,&owner,sizeof owner);
            reinterpret_cast<void(*)(void*,const char*)>(state.image+0x967370)(&objects.record,text);objects.record_constructed=true;return 0;}
        if(command==Command::Copy){objects.source=reinterpret_cast<uintptr_t(*)(size_t)>(state.image+0xa02670)(0x210);
            reinterpret_cast<void(*)(void*)>(state.image+0x15350)(reinterpret_cast<void*>(objects.source));objects.source_constructed=true;
            objects.source_uid=*reinterpret_cast<const uint64_t*>(objects.source+0x18);prototype=objects.source;}
        objects.icon=reinterpret_cast<uintptr_t(*)(size_t)>(state.image+0xa02670)(0x410);
        reinterpret_cast<void(*)(void*,void*,void*)>(state.image+0x565ad0)(reinterpret_cast<void*>(objects.icon),reinterpret_cast<void*>(prototype),command==Command::Copy?nullptr:reinterpret_cast<void*>(owner));
        objects.icon_constructed=true;
        if(command!=Command::Remove){reinterpret_cast<void(*)(void*,int32_t,int32_t,void*)>(state.image+0x566940)(objects.slot.data(),96,96,reinterpret_cast<void*>(owner));objects.slot_constructed=true;
            std::memcpy(objects.slot.data()+0x348,&display_slot,sizeof display_slot);std::memcpy(objects.slot.data()+0x34c,&level,sizeof level);}
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t primary_native(NativeObjects& objects,Command command)noexcept{
    __try{objects.primary_attempted=true;
        if(command==Command::Create)objects.primary_accepted=original_new(objects.view.data(),&objects.record);
        else if(command==Command::Remove){original_remove(reinterpret_cast<void*>(objects.icon));objects.primary_accepted=true;}
        else {original_dock(objects.slot.data(),reinterpret_cast<void*>(objects.icon),0,false);objects.primary_accepted=true;}
        objects.primary_returned=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t destroy_native(uintptr_t object,uintptr_t rva,size_t allocation,bool constructed)noexcept{
    __try{
        if(!constructed)reinterpret_cast<void(*)(void*,size_t)>(state.image+0x1264b60)(reinterpret_cast<void*>(object),allocation);
        else if(rva)reinterpret_cast<void(*)(void*)>(state.image+rva)(reinterpret_cast<void*>(object));
        else {const auto ap=*reinterpret_cast<const uintptr_t*>(object);
            reinterpret_cast<void(*)(void*)>(*reinterpret_cast<const uintptr_t*>(ap+8))(reinterpret_cast<void*>(object));}
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool same_current(const Sample& before,const Sample& after){return same_world(before.current.world,after.current.world)&&before.current.selected==after.current.selected&&
    before.current.uid==after.current.uid&&before.current.money==after.current.money&&before.current.users.rows==after.current.users.rows;}
bool finish_cleanup(Operation& op,Memory& memory,std::string& error){
    if(op.native.cleanup_attempted)return op.native.cleanup_confirmed||fail(error,"Ability private native cleanup already failed; retry is forbidden");
    struct Port {
        NativeObjects& value;
        bool slot_owner(uintptr_t& out,std::string&){std::memcpy(&out,value.slot.data()+0x380,sizeof out);return true;}
        bool run(uintptr_t pointer,uintptr_t rva,size_t allocation,bool constructed,std::string& error){
            value.cleanup_exception=destroy_native(pointer,rva,allocation,constructed);
            return !value.cleanup_exception||fail(error,"Ability original private receiver cleanup SEH="+std::to_string(value.cleanup_exception));}
        bool destroy_slot(std::string& error){return run(uintptr_t(value.slot.data()),0x568970,0,true,error);}
        bool destroy_icon(uintptr_t pointer,bool constructed,std::string& error){return run(pointer,0,0x410,constructed,error);}
        bool destroy_source(uintptr_t pointer,bool constructed,std::string& error){return run(pointer,0,0x210,constructed,error);}
        bool destroy_record(std::string& error){return run(uintptr_t(&value.record),0x9676a0,0,true,error);}
    } port{op.native};
    if(!ability_receiver_cleanup::finish(port,op.native,op.native.cleanup,error))return false;
    if(op.native.source_uid){uintptr_t registered=0;const auto seh=lookup(op.before.current.api.find,op.native.source_uid,registered);
        if(seh||registered){op.native.cleanup_confirmed=false;return fail(error,"Ability private copied source UID survived original cleanup SEH="+std::to_string(seh));}}
    (void)memory;return true;
}
bool wire_fits(const a::Request& request,const Sample& before,std::string& error){
    return a::ability_membership_wire_fits(request,before.value,before.current.money,error);
}
a::Outcome boundary_failure(const a::Request& request,const std::string& why,std::string& error){
    auto found=operations.find(request.action_uuid);error=why;
    if(found!=operations.end()){auto& op=found->second;
        if(!op.native.cleanup_attempted){Memory memory;std::string cleanup_error;executing=true;
            if(!finish_cleanup(op,memory,cleanup_error))error+="; "+cleanup_error;}
        op.cause=error;}
    executing=false;fault=true;return a::Outcome::Unknown;
}
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    if(request.action!=a::Action::EditAbilityMembership||operations.contains(request.action_uuid)||!ready||fault||executing||pending||GetCurrentThreadId()!=state.thread)
        return fail(error,"Ability membership execution lacks its fresh ready original-once Mainthread action"),a::Outcome::RejectedNoEffect;
    Memory memory;Sample before;const auto& intent=request.ability_membership;std::string current_sha;
    if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,intent,before,error,&request)||!digest(before,current_sha,error)||current_sha!=intent.before_sha256||
       !wire_fits(request,before,error))return a::Outcome::RejectedNoEffect;
    if((intent.command==Command::Create&&before.local.size()>=12)||((intent.command==Command::Remove||intent.command==Command::Dock)&&
       (intent.slot<0||size_t(intent.slot)>=before.local.size())))return fail(error,"Ability original action current owner count/slot is unavailable"),a::Outcome::RejectedNoEffect;
    if(intent.command==Command::Copy){std::string expected;if(!copy_name(memory,before.current.selected,intent.prefab.text[0],expected,error)||expected!=intent.copy_name)
        return fail(error,error.empty()?"Ability original current unique prefab name changed":error),a::Outcome::RejectedNoEffect;}
    if(authority){auto portable=before.value;portable.complete.global_random=authority->ability_membership_before.complete.global_random;
        if(portable!=authority->ability_membership_before||before.current.money!=authority->before)
            return fail(error,"Ability membership Client complete actual pre-state differs from Host"),a::Outcome::RejectedNoEffect;}
    auto& op=operations[request.action_uuid];op.binding=binding;op.request=request;op.before=std::move(before);
    const auto unknown=[&](const std::string& why){std::string cleanup_error;
        if(!op.native.cleanup_attempted&&!finish_cleanup(op,memory,cleanup_error))op.cause=why+"; "+cleanup_error;else op.cause=why;
        executing=false;fault=true;error=op.cause;return a::Outcome::Unknown;};
    op.seed_attempted=true;const auto desired=authority?authority->ability_membership_before.complete.global_random:op.before.value.complete.global_random;
    const auto seed=action_runtime::seed(state.image,desired);if(seed)return unknown("Ability membership Host RNG adoption SEH="+std::to_string(seed));
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.before,error,&request)||op.before.value.complete.global_random!=desired)return unknown(error.empty()?"Ability current preparation RNG differs":error);
    const auto text="NewAbility Level "+std::to_string(intent.level);
    const uintptr_t prototype=(intent.command==Command::Remove||intent.command==Command::Dock)?op.before.local[size_t(intent.slot)][0]:0;
    executing=true;const auto prepared=construct_native(op.native,intent.command,op.before.current.selected,prototype,intent.level,intent.display_slot,text.c_str());
    if(prepared)return unknown("Ability original private native receiver construction SEH="+std::to_string(prepared));
    if(intent.command==Command::Copy){const auto stored=ability_edit::prepare(op.native.source,intent.prefab);
        if(stored)return unknown("Ability original private prefab preparation SEH="+std::to_string(stored));
        a::AbilityRecord actual;if(!ability_edit::read_record(memory,op.native.source,actual,error)||actual!=intent.prefab)return unknown(error.empty()?"Ability private original prefab input differs":error);}
    Sample prepared_current;if(!sample(memory,binding,request.snapshot_uuid,intent,prepared_current,error,&request)||
       !same_current(op.before,prepared_current)||op.before.value!=prepared_current.value)return unknown(error.empty()?"Ability private native preparation changed the real world":error);
    op.evidence.before=op.before.current.money;op.evidence.ability_membership_before=op.before.value;
    op.attempted=true;op.calls=1;op.native.primary_exception=primary_native(op.native,intent.command);
    op.returned=op.native.primary_returned;
    if(op.native.primary_exception||!op.returned||!op.native.primary_accepted)return unknown("Ability original transaction failed or was not accepted SEH="+std::to_string(op.native.primary_exception));
    if(!finish_cleanup(op,memory,error))return unknown(error);executing=false;
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.after,error,&request)||!same_current(op.before,op.after)||
       !a::ability_membership_transition(intent,op.before.value,op.after.value))return unknown(error.empty()?"Ability original complete post-state violates its exact authoring transition":error);
    if(intent.command==Command::Remove){uintptr_t removed=0;const auto seh=lookup(op.before.current.api.find,op.before.local[size_t(intent.slot)][1],removed);
        if(seh||removed)return unknown("Ability original removed full UID remains registered SEH="+std::to_string(seh));}
    op.evidence.after=op.after.current.money;op.evidence.ability_membership_after=op.after.value;
    if(authority){auto expected=*authority;expected.retirement_confirmed=false;if(op.evidence!=expected)return unknown("Ability membership Client complete original result differs from Host");}
    op.confirmed=true;return a::Outcome::Confirmed;
}catch(const std::exception& exception){return boundary_failure(request,std::string("Ability membership native boundary: ")+exception.what(),error);}
catch(...){return boundary_failure(request,"Ability membership native non-standard exception",error);}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& out,std::string& error){
    const auto found=operations.find(request.action_uuid);
    if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||
       !found->second.returned||found->second.calls!=1||!found->second.native.cleanup_confirmed)
        return fail(error,"Ability observation lacks exact confirmed original-once and private cleanup receipts");
    const auto& op=found->second;Memory memory;Sample actual;
    if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,request.ability_membership,actual,error,&request)||
       !same_world(op.after.current.world,actual.current.world)||actual.value!=op.after.value||actual.current.money!=op.after.current.money)
        return fail(error,error.empty()?"Ability current complete native result changed before ACK":error);
    out=op.evidence;out.retirement_confirmed=op.retired;return true;
}
bool retire(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence ignored;
    if(!observe(binding,request,ignored,error))return false;auto& op=operations.at(request.action_uuid);
    if(op.retired)return fail(error,"Ability original receipt already retired");op.retired=true;return true;
}
json status(){json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"attempted",op.attempted},{"returned",op.returned},
    {"original_calls",op.calls},{"confirmed",op.confirmed},{"retired",op.retired},{"cleanup_attempted",op.native.cleanup_attempted},
    {"cleanup_confirmed",op.native.cleanup_confirmed},{"primary_exception",op.native.primary_exception},{"cleanup_exception",op.native.cleanup_exception},{"cause",op.cause}};
    return {{"enabled",ready},{"fault",fault},{"pending_ui",pending_ui()},{"captures",captures},{"submitted",submitted},{"rejected",rejected},
        {"last_action",last_action},{"cause",cause},{"operations",rows}};
}
}
