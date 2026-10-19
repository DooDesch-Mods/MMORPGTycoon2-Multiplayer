#include "character_membership_sample.inc"
namespace character_membership {
namespace {
using Confirm=void(*)(void*);
Confirm original=nullptr;uint64_t generation=0,captures=0,submitted=0,rejected=0;
bool installed=false,ready=false,fault=false,executing=false;std::string cause,last_action;
struct Pending {a::Binding binding;std::string snapshot;a::CharacterMembershipIntent intent;uintptr_t view=0;};
struct Operation {a::Binding binding;a::Request request;Sample before,after;a::Evidence evidence;
    remove_diagnostics::CharacterBossRetirement bosses;bool attempted=false,returned=false,seed_attempted=false,confirmed=false,retired=false;
    character_membership_npc_completion::Receipt npc_completion;
    uint32_t calls=0;int32_t ordinal=-1;uintptr_t initiating_view=0;std::string cause;};
std::optional<Pending> pending;std::map<std::string,Operation> operations;std::map<std::string,uintptr_t> local_views;
void confirm_entry(void*);
bool reject(const std::string& error){++rejected;if(cause!=error){cause=error;LOG_ERROR("[CharacterMembership] {}",error);}return false;}
bool code(Memory& memory,std::string& error){
    for(const auto& pin:character_membership_codes::helpers){std::vector<unsigned char> bytes(pin.bytes);
        if(!memory.copy(state.image+pin.rva,bytes.data(),bytes.size(),error))return false;
        if(pin.rva==0x684250&&original){MT2FW_MT2_RouteReceipt receipt{sizeof receipt};
            if(!quest_list::read_route(state.image+pin.rva,receipt,error))return false;
            if(receipt.detour!=uintptr_t(&confirm_entry)||receipt.trampoline!=uintptr_t(original)||receipt.generation!=generation||receipt.state!=MT2FW_MT2_ROUTE_ENABLED)
                return fail(error,"Character confirmed-delete active original route ownership differs");
            constexpr size_t copied=8;std::array<unsigned char,copied+14> trampoline{};uintptr_t resume=0;
            constexpr std::array<unsigned char,6> continuation{0xff,0x25,0,0,0,0};
            if(!memory.copy(uintptr_t(original),trampoline.data(),trampoline.size(),error))return false;
            std::memcpy(&resume,trampoline.data()+copied+6,8);
            if(!std::equal(receipt.pristine,receipt.pristine+copied,trampoline.begin())||
               !std::equal(continuation.begin(),continuation.end(),trampoline.begin()+copied)||resume!=state.image+pin.rva+copied)
                return fail(error,"Character confirmed-delete exact original instruction boundary differs");}
        if(mt2mp::sha256(bytes)==pin.sha)continue;
        if(pin.rva!=0x684250&&pin.rva!=0x689e80&&pin.rva!=0x2d42c0)return fail(error,"Character membership original body differs RVA="+std::to_string(pin.rva));
        MT2FW_MT2_RouteReceipt receipt{sizeof receipt};
        if(!quest_list::read_route(state.image+pin.rva,receipt,error))return false;
        if(bytes.size()<32||std::memcmp(bytes.data(),receipt.current,32))return fail(error,"Character membership live route receipt differs RVA="+std::to_string(pin.rva));
        std::memcpy(bytes.data(),receipt.pristine,32);
        if(mt2mp::sha256(bytes)!=pin.sha||!quest_list::verify_code(state.image+pin.rva,bytes.data(),bytes.size(),error))
            return fail(error,error.empty()?"Character membership pristine full body differs RVA="+std::to_string(pin.rva):error);
        if(pin.rva==0x684250&&original&&(receipt.detour!=uintptr_t(&confirm_entry)||receipt.trampoline!=uintptr_t(original)||receipt.generation!=generation))
            return fail(error,"Character confirmed-delete route owner/trampoline/generation differs");
    }return true;
}
uint32_t name_used(uintptr_t manager,const std::string& text,bool& used)noexcept{
    __try{const quest_list::QuestTitleView value{text.data(),text.size()};
        used=reinterpret_cast<bool(*)(void*,const quest_list::QuestTitleView&)>(state.image+0x9e690)(reinterpret_cast<void*>(manager),value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool next_name(uintptr_t manager,const std::string& base,std::string& name,std::string& error){
    for(int32_t suffix=1;suffix<INT32_MAX;++suffix){name=base+" "+std::to_string(suffix);
        if(name.size()>1024)return fail(error,"Character variant generated name exceeds native ordered text bound");
        bool used=false;const auto seh=name_used(manager,name,used);if(seh)return fail(error,"Character original name query SEH="+std::to_string(seh));
        if(!used)return true;}
    return fail(error,"Character original positive variant-name sequence is exhausted");
}
bool new_token(Memory& memory,uintptr_t record,bool& value,std::string& error){
    value=false;uintptr_t tokens=0;int32_t count=0,capacity=0,type=0;
    if(!field(memory,record,0x38,tokens,error)||!field(memory,record,0x40,count,error)||!field(memory,record,0x44,capacity,error))return false;
    if(count<0||capacity<count||(count&&!tokens))return fail(error,"Character SetVariant original token array bounds differ");
    if(!count)return true;
    if(!field(memory,tokens,0,type,error))return false;
    if(type!=1&&type!=2)return true;
    std::string token;if(!npc_quest_records::read_string(memory,tokens,8,"Character SetVariant token",token,error))return false;
    value=token=="new";return true;
}
bool queue(uintptr_t view,a::CharacterMembershipCommand command,std::string& error){
    const auto controller=typed_action_runtime::controller_status();a::Binding binding;const auto snapshot=net::current_snapshot_id();
    Memory memory;
    if(!ready||fault||pending||executing||GetCurrentThreadId()!=state.thread||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||
       !code(memory,error)||!typed_action_runtime::network_binding(snapshot,binding,error))
        return fail(error,error.empty()?"Character membership capture requires idle bound Mainthread Controller":error);
    uintptr_t ap=0,object=0;a::CharacterMembershipIntent intent;intent.command=command;
    if(!field(memory,view,0,ap,error)||ap!=state.image+0x1736770||!field(memory,view,command==a::CharacterMembershipCommand::CreateVariant?0x458:0x460,object,error)||
       !object||!field(memory,object,0x2c,intent.kind,error)||!field(memory,object,0x30,intent.id,error))return fail(error,error.empty()?"Character membership current native View/selected type differs":error);
    Sample before;if(!sample(memory,binding,snapshot,intent,before,error))return false;
    if(before.selected!=object||before.value.selected.deleted)return fail(error,"Character membership captured View selected type lifetime differs");
    if(command==a::CharacterMembershipCommand::CreateVariant&&(before.value.selected.parent>=0||!next_name(before.manager,before.value.selected.name,intent.name,error)))
        return fail(error,error.empty()?"Character variant creation requires its current base type":error);
    intent.before_sha=before.value.manager_sha;pending=Pending{binding,snapshot,intent,view};++captures;error.clear();return true;
}
void confirm_entry(void* receiver)try{
    if(!original){fault=true;reject("Character confirmed delete original unavailable");return;}
    if(!net::session_wire::connected()){original(receiver);return;}
    std::string error;if(!queue(uintptr_t(receiver),a::CharacterMembershipCommand::Destroy,error))reject(error);
}catch(const std::exception& exception){fault=true;reject(std::string("Character confirmed delete boundary: ")+exception.what());}
catch(...){fault=true;reject("Character confirmed delete non-standard exception");}
uint32_t create_original(uintptr_t object,const std::string& text,int32_t& ordinal)noexcept{
    __try{const quest_list::QuestTitleView value{text.data(),text.size()};
        ordinal=reinterpret_cast<int32_t(*)(void*,const quest_list::QuestTitleView&)>(state.image+0x97a90)(reinterpret_cast<void*>(object),value);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t destroy_original(uintptr_t collection,uintptr_t object)noexcept{
    __try{reinterpret_cast<void(*)(void*,void*)>(state.image+0x9bb90)(reinterpret_cast<void*>(collection),reinterpret_cast<void*>(object));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t show_created(uintptr_t view,uintptr_t object)noexcept{
    __try{reinterpret_cast<void(*)(void*,void*)>(state.image+0x689100)(reinterpret_cast<void*>(view),reinterpret_cast<void*>(object));
        reinterpret_cast<void(*)(void*)>(state.image+0x683c50)(reinterpret_cast<void*>(view));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool same_create(const Sample& before,const Sample& after,int32_t created,std::string& error){
    if(!same_world(before.world,after.world)||before.manager!=after.manager||before.selected!=after.selected||before.uid!=after.uid||before.users.rows!=after.users.rows||
       before.undo!=after.undo||before.money!=after.money)return fail(error,"Character original CreateVariant changed current World, existing user membership, ledger or Undo");
    for(const auto& old:before.definitions.definitions){if(old.kind==before.value.selected.kind&&old.id==created)continue;
        const auto found=std::find_if(after.definitions.definitions.begin(),after.definitions.definitions.end(),[&](const auto& row){return row.kind==old.kind&&row.id==old.id;});
        if(found==after.definitions.definitions.end()||found->object!=old.object||found->uid!=old.uid||found->parent!=old.parent||found->deleted!=old.deleted||
           found->owning_collection!=old.owning_collection)return fail(error,"Character original CreateVariant replaced an unrelated global definition lifetime");
    }return true;
}
uint32_t remove_npc_original(uintptr_t manager,uintptr_t object)noexcept{
    __try{reinterpret_cast<void(*)(void*,void*)>(state.image+0x297110)(reinterpret_cast<void*>(manager),reinterpret_cast<void*>(object));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool complete_npcs(Memory& memory,const Sample& before,character_membership_npc_completion::Receipt& receipt,std::string& error){
    namespace completion=character_membership_npc_completion;
    struct Port {
        Memory& memory;const Sample& before;
        bool read(const completion::Pool& expected,completion::Pool& out,std::string& error){
            out={};npc_census::State current;if(!npc_census::read(memory,state.image,expected.region,current,error))return false;
            out.region=expected.region;out.manager=current.manager;out.next_tag=current.next_local_tag;
            for(const auto& row:current.rows){uintptr_t found=0;const auto seh=lookup(before.api.find,row.local_uid,found);
                if(seh||found!=row.local_pointer)return fail(error,"Character deferred NPC full current pool FindUID differs SEH="+std::to_string(seh));
                out.rows.push_back({row.local_pointer,row.local_uid,row.type});}return true;
        }
        bool marked(const completion::Pool& pool,const completion::Member& row,bool& marked,std::string& error){
            marked=false;uintptr_t ap=0,region=0,destroy=0,registered=0;uint64_t uid=0;int32_t type=-1;uint8_t queued=2;
            if(!field(memory,row.pointer,0,ap,error)||!field(memory,row.pointer,0x18,uid,error)||!field(memory,row.pointer,0x178,region,error)||
               !field(memory,row.pointer,0x80c,type,error)||!field(memory,row.pointer,0x894,queued,error)||!field(memory,ap,8,destroy,error))return false;
            const auto seh=lookup(before.api.find,row.uid,registered);
            if(seh||registered!=row.pointer||uid!=row.uid||region!=pool.region||type!=row.type||ap!=state.image+0x1742980||destroy!=state.image+0x3c65c0||queued>1)
                return fail(error,"Character deferred NPC exact current Region/type/AP/D0/full UID differs SEH="+std::to_string(seh));
            marked=queued==1;return true;
        }
        bool remove(const completion::Pool& pool,const completion::Member& row,bool& attempted,std::string& error){
            attempted=false;bool queued=false;if(!marked(pool,row,queued,error))return false;
            if(!queued)return fail(error,"Character original NPC pending flag changed before original removal");
            attempted=true;const auto seh=remove_npc_original(pool.manager,row.pointer);
            return !seh||fail(error,"Character original deferred RemoveNPC SEH="+std::to_string(seh));
        }
        bool registered(uint64_t uid,bool& present,std::string& error){uintptr_t pointer=0;const auto seh=lookup(before.api.find,uid,pointer);
            if(seh)return fail(error,"Character original deferred NPC FindUID SEH="+std::to_string(seh));present=pointer!=0;return true;}
    } port{memory,before};
    std::vector<completion::Member> targets;
    for(const auto& old:before.npc_retirements){const auto pool=std::find_if(before.npc_pools.begin(),before.npc_pools.end(),[&](const auto& value){return value.region==old.region;});
        if(pool==before.npc_pools.end())return fail(error,"Character affected NPC pre-state has no complete original Region pool");
        const auto member=std::find_if(pool->rows.begin(),pool->rows.end(),[&](const auto& value){return value.pointer==old.pointer&&value.uid==old.uid;});
        if(member==pool->rows.end())return fail(error,"Character affected NPC is absent from its complete original Region pool");targets.push_back(*member);}
    return completion::finish(port,before.npc_pools,targets,receipt,error);
}
bool retire_native_users(Memory& memory,const a::Binding& binding,const a::Request& request,const Sample& before,std::string& error){
    std::set<uint64_t> removed;for(const auto& user:before.affected){uintptr_t pointer=0;const auto seh=lookup(before.api.find,user.uid,pointer);
        if(seh||pointer)return fail(error,"Character original user destruction is incomplete UID="+std::to_string(user.uid)+" registered="+std::to_string(bool(pointer))+" SEH="+std::to_string(seh));
        removed.insert(user.uid);}
    if(!action_runtime::confirm_character_membership_retirements(binding,request,before.npc_retirements,error))return false;
    auto next=identity::registry;std::map<uintptr_t,monster_saved_identity::Pool> managers;
    struct SavedMemory {Memory& source;bool read(uintptr_t p,void* out,size_t bytes,std::string& e){return source.copy(p,out,bytes,e);}} source{memory};
    const auto unlinked=[](uintptr_t,int32_t& link,std::string&){link=0;return true;};
    for(const auto& [key,entry]:before.monster_aliases){if(entry.retired||!removed.contains(entry.uid))continue;
        if(!managers.contains(entry.scope.manager)&&!monster_saved_identity::observe(source,state.image,entry.scope.manager,unlinked,managers[entry.scope.manager],error))return false;
        for(const auto& actual:managers.at(entry.scope.manager).records)if(actual.uid==entry.uid||actual.pointer==entry.pointer)
            return fail(error,"Character original Monster user remains in its complete native manager");
        if(!next.retire(key,entry,error))return false;
    }
    identity::registry=std::move(next);return true;
}
}
bool capture_variant(void* receiver,void* record,bool& handled,std::string& error){
    handled=false;if(!net::session_wire::connected())return true;Memory memory;a::ui_diagnostic::Api api;bool label=false;
    if(!a::ui_diagnostic::resolve_api(memory,state.image,exe_sha,api,error))return false;
    const auto seh=quest_list::label(api.label_equals,record,"SetVariant",label);
    if(seh)return fail(error,"Character membership SetVariant label SEH="+std::to_string(seh));if(!label)return true;
    bool is_new=false;if(!new_token(memory,uintptr_t(record),is_new,error))return false;if(!is_new)return true;
    handled=true;const bool accepted=queue(uintptr_t(receiver),a::CharacterMembershipCommand::CreateVariant,error);if(!accepted)reject(error);return accepted;
}
bool pending_ui(){return pending.has_value()||executing;}
bool enable(std::string& error){
    if(!state.ready||GetCurrentThreadId()!=state.thread)return fail(error,"Character membership requires current installed Monster Mainthread");
    if(ready){for(const auto& [id,op]:operations)if(!op.retired)return fail(error,"Character membership retained unretired original operation="+id);
        if(fault||pending_ui())return fail(error,"Character membership retained unresolved capture");Memory memory;if(!code(memory,error))return false;operations.clear();local_views.clear();return true;}
    if(installed)return fail(error,"Character membership previous installation failed: "+cause);installed=true;Memory memory;if(!code(memory,error))return false;
    auto status=MH_CreateHook(reinterpret_cast<void*>(state.image+0x684250),reinterpret_cast<void*>(&confirm_entry),reinterpret_cast<void**>(&original));
    if(status!=MH_OK||!original)return fail(error,"Character confirmed-delete prepare failed status="+std::to_string(status)+" cause="+mt2mp::hooks::framework::process_backend().error());
    MT2FW_MT2_RouteReceipt receipt{sizeof receipt};if(!quest_list::read_route(state.image+0x684250,receipt,error))return false;generation=receipt.generation;
    status=MH_EnableHook(reinterpret_cast<void*>(state.image+0x684250));if(status!=MH_OK)return fail(error,"Character confirmed-delete enable failed status="+std::to_string(status));
    if(!code(memory,error))return false;ready=true;return true;
}
bool retire_lobby(std::string& error){
    if(!ready){error.clear();return true;}native::WorldStatus world;
    if(GetCurrentThreadId()!=state.thread||!native::read_world_status(world,error))return fail(error,error.empty()?"Character membership lobby retirement requires actual Mainthread":error);
    if(world.active_mode_index!=2||world.title_transition_pending||world.saves_in_progress>0||executing)return fail(error,"Character membership lobby retirement requires idle actual Title");
    pending.reset();operations.clear();local_views.clear();fault=false;cause.clear();last_action.clear();error.clear();return true;
}
void tick(){if(!pending)return;const auto captured=*pending;pending.reset();a::Binding actual;std::string error;
    if(fault||captured.snapshot!=net::current_snapshot_id()||!typed_action_runtime::network_binding(captured.snapshot,actual,error)||actual!=captured.binding){fault=true;reject(error.empty()?"Character membership captured World changed":error);return;}
    const auto id=action_runtime::uuid(error);if(id.empty()){fault=true;reject(error);return;}
    local_views.emplace(id,captured.view);
    if(!typed_action_runtime::submit_character_membership(id,captured.intent,error)){fault=true;reject(error);return;}++submitted;last_action=id;
}
a::Outcome perform(const a::Binding& binding,const a::Request& request,const a::Evidence* authority,std::string& error)try{
    if(!ready||fault||GetCurrentThreadId()!=state.thread||request.action!=a::Action::EditCharacterMembership){error="Character membership runtime unavailable";return a::Outcome::RejectedNoEffect;}
    if(operations.contains(request.action_uuid)){error="Character membership original already attempted; no replay";return a::Outcome::Unknown;}
    const auto controller=typed_action_runtime::controller_status();
    if(!controller.value("pending",false)||controller.value("locked",true)||controller.value("action_uuid",std::string{})!=request.action_uuid){error="Character membership requires its exact pending Controller";return a::Outcome::RejectedNoEffect;}
    const auto& intent=request.character_membership;Memory memory;Sample before;
    if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,intent,before,error,nullptr,&request))return a::Outcome::RejectedNoEffect;
    if(before.value.selected.deleted||before.value.manager_sha!=intent.before_sha){error="Character membership selected lifetime or complete captured manager revision changed";return a::Outcome::RejectedNoEffect;}
    if(!a::character_membership_wire_fits(request,before.value,before.money,error))return a::Outcome::RejectedNoEffect;
    if(intent.command==a::CharacterMembershipCommand::CreateVariant){std::string expected;
        if(before.value.selected.parent>=0||!next_name(before.manager,before.value.selected.name,expected,error)||expected!=intent.name){if(error.empty())error="Character original next unused variant name differs";return a::Outcome::RejectedNoEffect;}}
    if(authority){auto portable=before.value;portable.global_random=authority->character_membership_before.global_random;
        if(portable!=authority->character_membership_before||before.money!=authority->before){error="Character membership Client current complete prior state differs from Host";return a::Outcome::RejectedNoEffect;}}
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;
    const auto unknown=[&](const std::string& why){executing=false;error=op.cause=why.empty()?"Character membership original failed without cause":why;return a::Outcome::Unknown;};
    op.seed_attempted=true;const auto desired=authority?authority->character_membership_before.global_random:before.value.global_random;
    const auto seed=action_runtime::seed(state.image,desired);if(seed)return unknown("Character membership Host RNG adoption SEH="+std::to_string(seed));
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.before,error,nullptr,&request)||op.before.value.global_random!=desired||op.before.value.manager_sha!=intent.before_sha)return unknown(error.empty()?"Character membership prepared current state differs":error);
    op.evidence.before=op.before.money;op.evidence.character_membership_before=op.before.value;
    const auto selected=std::find_if(op.before.definitions.definitions.begin(),op.before.definitions.definitions.end(),[&](const auto& value){return value.object==op.before.selected;});
    if(selected==op.before.definitions.definitions.end())return unknown("Character membership original selected definition witness absent");
    if(intent.command==a::CharacterMembershipCommand::Destroy){std::vector<std::pair<uintptr_t,uint64_t>> bosses;
        for(const auto& user:op.before.affected)if(user.family==member::Family::Boss)bosses.emplace_back(user.object,user.uid);
        if(!remove_diagnostics::prepare_character_boss_retirement(binding,request,bosses,op.bosses,error))return unknown(error);}
    executing=true;op.attempted=true;++op.calls;
    const auto seh=intent.command==a::CharacterMembershipCommand::CreateVariant?create_original(op.before.selected,intent.name,op.ordinal):destroy_original(selected->owning_collection,op.before.selected);
    executing=false;if(seh)return unknown("Character original membership transaction SEH="+std::to_string(seh));op.returned=true;
    if(intent.command==a::CharacterMembershipCommand::Destroy&&(!complete_npcs(memory,op.before,op.npc_completion,error)||!retire_native_users(memory,binding,request,op.before,error)||
       !remove_diagnostics::confirm_character_boss_retirement(binding,request,op.bosses,error)))return unknown(error);
    if(!sample(memory,binding,request.snapshot_uuid,intent,op.after,error,&op.before,&request)||!a::character_membership_transition(intent,op.before.value,op.after.value))
        return unknown(error.empty()?"Character original CreateVariant/Destroy complete readback differs":error);
    if(intent.command==a::CharacterMembershipCommand::CreateVariant){
        if(op.ordinal!=int32_t(op.before.value.selected.variants.size())||!op.after.value.created||!same_create(op.before,op.after,op.after.value.created->id,error))return unknown(error.empty()?"Character original variant ordinal or preserved users differs":error);
        const auto view=local_views.find(request.action_uuid);
        if(view!=local_views.end()){uintptr_t ap=0,base=0;
            if(!field(memory,view->second,0,ap,error)||ap!=state.image+0x1736770||!field(memory,view->second,0x458,base,error)||base!=op.before.selected)
                return unknown(error.empty()?"Character initiating native View lifetime changed before selecting its new variant":error);
            const auto created=std::find_if(op.after.definitions.definitions.begin(),op.after.definitions.definitions.end(),[&](const auto& value){return value.kind==intent.kind&&value.id==op.after.value.created->id;});
            if(created==op.after.definitions.definitions.end())return unknown("Character original new global slot absent before native UI selection");
            const auto ui_seh=show_created(view->second,created->object);if(ui_seh)return unknown("Character native initiating variant selection/Rename SEH="+std::to_string(ui_seh));}}
    op.evidence.after=op.after.money;op.evidence.character_membership_after=op.after.value;
    if(authority){auto expected=*authority;expected.retirement_confirmed=false;if(op.evidence!=expected)return unknown("Character membership Client original complete result differs from Host");}
    op.confirmed=true;return a::Outcome::Confirmed;
}catch(const std::exception& exception){executing=false;error=std::string("Character membership perform boundary: ")+exception.what();return a::Outcome::Unknown;}
catch(...){executing=false;error="Character membership perform non-standard exception";return a::Outcome::Unknown;}
bool observe(const a::Binding& binding,const a::Request& request,a::Evidence& result,std::string& error){
    const auto found=operations.find(request.action_uuid);
    if(found==operations.end()||found->second.binding!=binding||found->second.request!=request||!found->second.confirmed||!found->second.returned||found->second.calls!=1)
        return fail(error,"Character membership observation lacks its exact confirmed original-once receipt");
    const auto& op=found->second;Memory memory;Sample current;
    if(!code(memory,error)||!sample(memory,binding,request.snapshot_uuid,request.character_membership,current,error,&op.before,&request)||
       !same_world(op.after.world,current.world)||op.after.value!=current.value||op.after.money!=current.money)
        return fail(error,error.empty()?"Character membership current complete result changed before ACK":error);
    result=op.evidence;result.retirement_confirmed=op.retired;return true;
}
bool retired_keys(const a::Binding& binding,const a::Request& request,std::vector<std::string>& keys,std::string& error){
    keys.clear();a::Evidence ignored;if(!observe(binding,request,ignored,error))return false;keys=operations.at(request.action_uuid).bosses.retired_keys;return true;
}
bool retire(const a::Binding& binding,const a::Request& request,std::string& error){a::Evidence ignored;if(!observe(binding,request,ignored,error))return false;
    auto& op=operations.at(request.action_uuid);if(op.retired)return fail(error,"Character membership original receipt already retired");op.retired=true;local_views.erase(request.action_uuid);return true;
}
json status(){json rows=json::object();for(const auto& [id,op]:operations)rows[id]={{"attempted",op.attempted},{"returned",op.returned},{"original_calls",op.calls},
    {"confirmed",op.confirmed},{"retired",op.retired},{"cause",op.cause},{"npc_completion",{{"attempted",op.npc_completion.attempted},
        {"calls",op.npc_completion.calls},{"returned",op.npc_completion.returned},{"confirmed",op.npc_completion.confirmed},{"cause",op.npc_completion.cause}}}};
    return {{"enabled",ready},{"fault",fault},{"pending_ui",pending_ui()},{"captures",captures},{"submitted",submitted},{"rejected",rejected},{"last_action",last_action},{"cause",cause},{"operations",rows}};
}
}
