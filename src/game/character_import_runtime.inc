namespace character_membership {
namespace {
using ImportOriginal=void(*)(void*,const void*);
ImportOriginal original_import=nullptr;uint64_t import_generation=0;
struct ImportTail {bool armed=false,preserve=false;uintptr_t view=0,target=0;uint32_t calls=0;};
ImportTail import_tail;character_import_native::Receipt last_import_input;std::string import_cause;
void import_entry(void*,const void*);
bool import_route(Memory& memory,std::string& error){
    if(!original_import)return fail(error,"Character image import original route is unavailable");
    MT2FW_MT2_RouteReceipt receipt{sizeof receipt};
    if(!quest_list::read_route(state.image+0x689510,receipt,error))return false;
    if(receipt.detour!=uintptr_t(&import_entry)||receipt.trampoline!=uintptr_t(original_import)||
       receipt.generation!=import_generation||receipt.state!=MT2FW_MT2_ROUTE_ENABLED)
        return fail(error,"Character image import active original route ownership differs");
    constexpr size_t copied=6;std::array<unsigned char,copied+14> trampoline{};uintptr_t resume=0;
    constexpr std::array<unsigned char,6> jump{0xff,0x25,0,0,0,0};
    if(!memory.copy(uintptr_t(original_import),trampoline.data(),trampoline.size(),error))return false;
    std::memcpy(&resume,trampoline.data()+copied+6,8);
    if(!std::equal(receipt.pristine,receipt.pristine+copied,trampoline.begin())||
       !std::equal(jump.begin(),jump.end(),trampoline.begin()+copied)||resume!=state.image+0x689510+copied)
        return fail(error,"Character image import original six-byte instruction boundary differs");
    return true;
}
bool import_view(Memory& memory,uintptr_t requested,character_import_view::State& out,std::string& error){
    out={};uintptr_t editor=0,outer=0,ap=0;
    if(!character_rename::window(memory,0,out.view,editor,outer,error)||out.view!=requested||
       !field(memory,out.view,0x458,out.base,error)||!field(memory,out.view,0x460,out.selected,error)||
       !field(memory,out.view,0x3b8,out.prefabs,error)||!out.prefabs||
       !field(memory,out.prefabs,0,ap,error)||ap!=state.image+0x1731f40||!field(memory,out.prefabs,0x358,out.notice,error))
        return fail(error,error.empty()?"Character image import current View/owned Prefabs pane differs":error);
    return true;
}
uint32_t import_write(const character_import_view::State& value)noexcept{
    __try{std::memcpy(reinterpret_cast<void*>(value.prefabs+0x358),&value.notice,sizeof value.notice);
        std::memcpy(reinterpret_cast<void*>(value.view+0x460),&value.selected,sizeof value.selected);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t import_original(uintptr_t view,const character_import_native::Image& image,bool& returned)noexcept{
    __try{original_import(reinterpret_cast<void*>(view),image.storage.data());returned=true;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct ImportCalls {
    Memory& memory;uintptr_t view;const character_import_native::Image& image;bool original_returned=false;uint32_t calls=0;
    bool read(character_import_view::State& value,std::string& error){return import_view(memory,view,value,error);}
    bool write(const character_import_view::State& value,std::string& error){const auto seh=import_write(value);
        return !seh||fail(error,"Character image import scoped presentation write SEH="+std::to_string(seh));}
    bool returned()const{return original_returned;}
    bool attempted()const{return calls!=0;}
    bool invoke(bool preserve,std::string& error){
        if(import_tail.armed)return fail(error,"Character image import presentation scope is already armed");
        character_import_view::State actual;if(!read(actual,error))return false;
        import_tail={true,preserve,view,actual.selected,0};++calls;const auto seh=import_original(view,image,original_returned);
        const auto tail=import_tail;import_tail={};
        if(seh||!original_returned||tail.calls!=1||fault)return fail(error,"Character image import original/presentation tail SEH="+std::to_string(seh)+
            " returned="+std::to_string(original_returned)+" tails="+std::to_string(tail.calls)+(cause.empty()?std::string{}:" cause="+cause));
        return true;
    }
};
void import_entry(void* receiver,const void* image)try{
    if(!original_import){fault=true;reject("Character image import original unavailable");return;}
    if(!net::session_wire::connected()){original_import(receiver,image);return;}
    const auto caller=uintptr_t(_ReturnAddress())-state.image;
    std::string error;Memory memory;a::Binding binding;const auto snapshot=net::current_snapshot_id();
    const auto controller=typed_action_runtime::controller_status();
    if((caller!=0x689cf4&&caller!=0xc14a67&&caller!=0xc14fd1)||!ready||fault||pending||executing||
       GetCurrentThreadId()!=state.thread||!controller.value("started",false)||controller.value("locked",true)||controller.value("pending",true)||
       !code(memory,error)||!import_route(memory,error)||!typed_action_runtime::network_binding(snapshot,binding,error)){
        reject(error.empty()?"Character image import capture requires its original File/URL caller and idle current Mainthread Controller":error);return;}
    character_import_view::State view;if(!import_view(memory,uintptr_t(receiver),view,error)){reject(error);return;}
    a::CharacterMembershipIntent intent;intent.command=a::CharacterMembershipCommand::Import;
    if(!view.selected||!field(memory,view.selected,0x2c,intent.kind,error)||!field(memory,view.selected,0x30,intent.id,error)){reject(error.empty()?"Character image import has no actual selected definition":error);return;}
    Sample before;if(!sample(memory,binding,snapshot,intent,before,error)||before.selected!=view.selected||before.value.selected.deleted){reject(error.empty()?"Character image import selected lifetime differs":error);return;}
    ValueMemory values(memory);
    if(!character_import_native::read(state.image,values,uintptr_t(image),intent.compressed_record,last_import_input,error)){
        import_cause=error;reject(error);return;}
    Sample after;if(!sample(memory,binding,snapshot,intent,after,error)||before.value!=after.value||before.money!=after.money||
       before.users!=after.users||!same_world(before.world,after.world)){
        reject(error.empty()?"Character image import input capture changed complete current ownership":error);return;}
    intent.before_sha=before.value.manager_sha;pending=Pending{binding,snapshot,std::move(intent),uintptr_t(receiver)};++captures;import_cause.clear();
}catch(const std::exception& exception){fault=true;reject(std::string("Character image import capture boundary: ")+exception.what());}
catch(...){fault=true;reject("Character image import capture non-standard exception");}
bool import_same_local(const Sample& before,const Sample& after,std::string& error){
    if(!same_world(before.world,after.world)||before.manager!=after.manager||before.selected!=after.selected||before.uid!=after.uid||
       before.users!=after.users||before.undo!=after.undo||before.money!=after.money)
        return fail(error,"Character image import replaced current World/definition/user ownership or changed complete Ledger/Undo");
    if(before.definitions.definitions.size()!=after.definitions.definitions.size())return fail(error,"Character image import changed global definition membership");
    for(size_t i=0;i<before.definitions.definitions.size();++i){const auto& old=before.definitions.definitions[i];const auto& fresh=after.definitions.definitions[i];
        if(old.object!=fresh.object||old.uid!=fresh.uid||old.parent!=fresh.parent||old.parent_uid!=fresh.parent_uid||
           old.kind!=fresh.kind||old.id!=fresh.id||old.deleted!=fresh.deleted||old.owning_collection!=fresh.owning_collection)
            return fail(error,"Character image import changed an existing definition lifetime/owning collection");}
    return true;
}
a::Outcome perform_import(Memory& memory,const a::Binding& binding,const a::Request& request,const a::Evidence* authority,const Sample& before,std::string& error){
    uintptr_t view=0,editor=0,outer=0;
    if(!import_route(memory,error)||!ability_edit::view_route(memory,error)||!character_rename::window(memory,0,view,editor,outer,error))return a::Outcome::RejectedNoEffect;
    character_import_view::State saved;if(!import_view(memory,view,saved,error))return a::Outcome::RejectedNoEffect;
    const auto local=local_views.find(request.action_uuid);const bool preserve=local==local_views.end();
    if(!preserve&&(local->second!=view||saved.selected!=before.selected)){error="Character image import initiating selection changed before ordered execution";return a::Outcome::RejectedNoEffect;}
    ValueMemory values(memory);character_import_native::Image image;character_import_native::Receipt input;
    const bool created=character_import_native::create(state.image,values,request.character_membership.compressed_record,image,input,error);
    if(!created){const auto original_error=error;std::string cleanup;const bool cleaned=character_import_native::discard(state.image,image,input,cleanup);
        error=original_error;if(!cleaned){if(!error.empty())error+="; ";error+=cleanup;}
        last_import_input=input;import_cause=error;return input.unknown?a::Outcome::Unknown:a::Outcome::RejectedNoEffect;}
    auto& op=operations.try_emplace(request.action_uuid).first->second;op.binding=binding;op.request=request;
    const auto unknown=[&](const std::string& why){executing=false;import_tail={};error=op.cause=why.empty()?"Character image import failed without cause":why;return a::Outcome::Unknown;};
    const auto cleanup=[&](){std::string failure;const auto ok=character_import_native::discard(state.image,image,input,failure);
        last_import_input=input;if(!ok){if(!error.empty())error+="; ";error+=failure;}return ok;};
    op.seed_attempted=true;const auto random=authority?authority->character_membership_before.global_random:before.value.global_random;
    const auto seeded=action_runtime::seed(state.image,random);
    if(seeded){error="Character image import Host RNG adoption SEH="+std::to_string(seeded);cleanup();return unknown(error);}
    if(!sample(memory,binding,request.snapshot_uuid,request.character_membership,op.before,error,nullptr,&request)||op.before.value.global_random!=random||
       op.before.value.manager_sha!=request.character_membership.before_sha){if(error.empty())error="Character image import prepared ownership/RNG differs";cleanup();return unknown(error);}
    op.evidence.before=op.before.money;op.evidence.character_membership_before=op.before.value;
    ImportCalls calls{memory,view,image};character_import_view::Receipt presentation;executing=true;
    const bool returned=character_import_view::run(calls,saved,op.before.selected,preserve,presentation,error);
    executing=false;op.attempted=calls.calls!=0;op.calls=calls.calls;op.returned=calls.original_returned;
    const bool cleaned=cleanup();
    if(!returned||!cleaned)return unknown(error);
    if(!sample(memory,binding,request.snapshot_uuid,request.character_membership,op.after,error,&op.before,&request)||
       !import_same_local(op.before,op.after,error)||!a::character_membership_transition(request.character_membership,op.before.value,op.after.value))
        return unknown(error.empty()?"Character image import original complete definition/Art/Actor/Quest readback differs":error);
    op.evidence.after=op.after.money;op.evidence.character_membership_after=op.after.value;
    if(authority){auto expected=std::make_unique<a::Evidence>(*authority);expected->retirement_confirmed=false;
        if(op.evidence!=*expected)return unknown("Character image import Client original complete result differs from Host");}
    op.confirmed=true;return a::Outcome::Confirmed;
}
}
bool import_presentation(void* view,void* selected,uintptr_t caller,bool& handled,std::string& error){
    handled=false;if(!import_tail.armed)return true;handled=true;
    if(GetCurrentThreadId()!=state.thread||uintptr_t(view)!=import_tail.view||uintptr_t(selected)!=import_tail.target||
       caller!=state.image+0x68997a||import_tail.calls++){
        fault=true;return fail(error,"Character image import scoped presentation receiver/selected/caller/thread/count differs");}
    if(!import_tail.preserve)ability_edit::original_view_select(view,selected);return true;
}
bool enable_import(std::string& error){
    Memory memory;if(original_import)return import_route(memory,error);
    const auto status=MH_CreateHook(reinterpret_cast<void*>(state.image+0x689510),reinterpret_cast<void*>(&import_entry),reinterpret_cast<void**>(&original_import));
    if(status!=MH_OK||!original_import)return fail(error,"Character image import prepare failed status="+std::to_string(status)+" cause="+hooks::framework::process_backend().error());
    MT2FW_MT2_RouteReceipt receipt{sizeof receipt};if(!quest_list::read_route(state.image+0x689510,receipt,error))return false;import_generation=receipt.generation;
    const auto enabled=MH_EnableHook(reinterpret_cast<void*>(state.image+0x689510));
    if(enabled!=MH_OK)return fail(error,"Character image import enable failed status="+std::to_string(enabled));
    return import_route(memory,error);
}
json import_status(){const auto& value=last_import_input;
    return {{"decoder_attempted",value.decoder_attempted},{"decoder_returned",value.decoder_returned},{"decoder_cleaned",value.decoder_cleaned},
        {"payload_complete",value.payload_complete},{"image_constructed",value.image_constructed},{"image_encoded",value.image_encoded},
        {"image_roundtrip",value.image_roundtrip},{"image_cleaned",value.image_cleaned},{"borrowed_constructed",value.borrowed_constructed},
        {"borrowed_cleaned",value.borrowed_cleaned},{"unknown",value.unknown},{"exception",value.exception},{"cause",import_cause.empty()?value.cause:import_cause}};
}
}
