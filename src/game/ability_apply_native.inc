using ApplySetup=void(*)(void*,void*);
using ApplyEvent=void(*)(void*,const void*);
using ApplyWindowDestroy=void(*)(void*);
ApplySetup original_apply_setup=nullptr;ApplyEvent original_apply_event=nullptr;ApplyWindowDestroy original_apply_window_destroy=nullptr;
uint64_t apply_setup_generation=0,apply_event_generation=0,apply_window_destroy_generation=0;
bool proposal_release(uintptr_t outer,std::string& error);
bool proposal_active(){
    if(!ready||!net::session_wire::connected())return false;
    if(GetCurrentThreadId()!=state.thread)return true;
    const auto controller=typed_action_runtime::controller_status();
    return controller.value("started",false)&&!controller.value("lobby_retiring",false);
}
struct ApplyTail {bool armed=false;uintptr_t outer=0,live=0;uint32_t setup=0,event=0;bool failed=false;};
ApplyTail apply_tail;
void apply_setup_entry(void* outer,void* live){
    if(apply_tail.armed&&uintptr_t(outer)==apply_tail.outer){
        if(GetCurrentThreadId()!=state.thread||uintptr_t(live)!=apply_tail.live||
           uintptr_t(_ReturnAddress())!=state.image+0x570b63||apply_tail.setup++){
            apply_tail.failed=true;fault=true;reject("Ability Apply SetupFor presentation receiver/target/caller/thread/count differs");}
        return;
    }
    if(proposal_active()){
        std::string error;if(!proposal_release(uintptr_t(outer),error)){fault=true;reject("Ability old proposal replacement protection failed: "+error);return;}}
    original_apply_setup(outer,live);
}
void apply_window_destroy_entry(void* outer){
    if(proposal_active()){
        std::string error;if(!proposal_release(uintptr_t(outer),error)){fault=true;reject("Ability old proposal destruction protection failed: "+error);}}
    // The deleting wrapper frees this window after D1 returns. Always finish native
    // destruction; a protection failure remains a fault, never a successful cleanup.
    original_apply_window_destroy(outer);
}
void apply_event_entry(void* outer,const void* event){
    if(apply_tail.armed&&uintptr_t(outer)==apply_tail.outer){
        Memory memory;std::string error;int32_t type=0;uintptr_t target=0;
        if(!field(memory,uintptr_t(event),0,type,error)||(type==40&&!field(memory,uintptr_t(event),0xb0,target,error))){
            apply_tail.failed=true;fault=true;reject(error);return;}
        if(type==40&&target==apply_tail.live){
            if(GetCurrentThreadId()!=state.thread||apply_tail.event++){
                apply_tail.failed=true;fault=true;reject("Ability Apply own Event40 presentation thread/count differs");}
            return;
        }
    }
    original_apply_event(outer,event);
}
bool apply_route(Memory& memory,std::string& error){
    for(size_t i=0;i<3;++i){const uintptr_t rva=i==2?0x569550:(i?0x5721f0:0x570900);
        const auto original_address=i==2?uintptr_t(original_apply_window_destroy):(i?uintptr_t(original_apply_event):uintptr_t(original_apply_setup));
        if(!original_address)return fail(error,"Ability Apply presentation route unavailable");
        MT2FW_MT2_RouteReceipt receipt{sizeof receipt};
        if(!quest_list::read_route(state.image+rva,receipt,error))return false;
        if(receipt.detour!=(i==2?uintptr_t(&apply_window_destroy_entry):(i?uintptr_t(&apply_event_entry):uintptr_t(&apply_setup_entry)))||receipt.trampoline!=original_address||
           receipt.generation!=(i==2?apply_window_destroy_generation:(i?apply_event_generation:apply_setup_generation))||receipt.state!=MT2FW_MT2_ROUTE_ENABLED)
            return fail(error,"Ability Apply presentation route ownership differs RVA="+std::to_string(rva));
        const size_t copied=i==2?5:(i?8:6);std::array<unsigned char,22> bytes{};uintptr_t resume=0;
        if(!memory.copy(original_address,bytes.data(),copied+14,error))return false;
        std::memcpy(&resume,bytes.data()+copied+6,8);constexpr std::array<unsigned char,6> jump{0xff,0x25,0,0,0,0};
        if(!std::equal(receipt.pristine,receipt.pristine+copied,bytes.begin())||
           !std::equal(jump.begin(),jump.end(),bytes.begin()+copied)||resume!=state.image+rva+copied)
            return fail(error,"Ability Apply original instruction boundary differs RVA="+std::to_string(rva));
    }
    return true;
}
bool install_apply_routes(std::string& error){
    for(size_t i=0;i<3;++i){const uintptr_t rva=i==2?0x569550:(i?0x5721f0:0x570900);
        auto status=MH_CreateHook(reinterpret_cast<void*>(state.image+rva),i==2?reinterpret_cast<void*>(&apply_window_destroy_entry):(i?reinterpret_cast<void*>(&apply_event_entry):reinterpret_cast<void*>(&apply_setup_entry)),
            i==2?reinterpret_cast<void**>(&original_apply_window_destroy):(i?reinterpret_cast<void**>(&original_apply_event):reinterpret_cast<void**>(&original_apply_setup)));
        if(status!=MH_OK)return fail(error,"Ability Apply presentation prepare failed RVA="+std::to_string(rva)+" status="+std::to_string(status)+" cause="+hooks::framework::process_backend().error());
        status=MH_EnableHook(reinterpret_cast<void*>(state.image+rva));
        if(status!=MH_OK)return fail(error,"Ability Apply presentation enable failed RVA="+std::to_string(rva)+" status="+std::to_string(status));
        MT2FW_MT2_RouteReceipt receipt{sizeof receipt};if(!quest_list::read_route(state.image+rva,receipt,error))return false;
        (i==2?apply_window_destroy_generation:(i?apply_event_generation:apply_setup_generation))=receipt.generation;
    }
    Memory memory;return apply_route(memory,error);
}
bool apply_slots(Memory& memory,uintptr_t outer,ability_apply_lease::Slots& out,std::string& error){
    out.outer=outer;return field(memory,outer,0x510,out.draft,error)&&field(memory,outer,0x520,out.live,error)&&
        field(memory,outer,0x528,out.proxy,error)&&field(memory,outer,0x530,out.owner,error)&&field(memory,outer,0x606,out.dirty,error);
}
uint32_t apply_write_slots(const ability_apply_lease::Slots& value)noexcept{
    __try{
        *reinterpret_cast<uintptr_t*>(value.outer+0x510)=value.draft;
        *reinterpret_cast<uintptr_t*>(value.outer+0x520)=value.live;
        *reinterpret_cast<uintptr_t*>(value.outer+0x528)=value.proxy;
        *reinterpret_cast<uintptr_t*>(value.outer+0x530)=value.owner;
        std::memcpy(reinterpret_cast<void*>(value.outer+0x606),value.dirty.data(),2);return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t apply_clone(uintptr_t source,uintptr_t& draft,bool& constructed)noexcept{
    __try{draft=reinterpret_cast<uintptr_t(*)(size_t)>(state.image+0xa02670)(0x210);
        if(!draft)return ERROR_NOT_ENOUGH_MEMORY;
        reinterpret_cast<void(*)(void*,const void*)>(state.image+0x16230)(reinterpret_cast<void*>(draft),reinterpret_cast<const void*>(source));
        constructed=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t apply_acquire_proxy(uintptr_t live,uintptr_t& proxy,bool& acquired)noexcept{
    __try{
        proxy=*reinterpret_cast<uintptr_t*>(live+0x40);
        if(!proxy){proxy=reinterpret_cast<uintptr_t(*)(size_t)>(state.image+0xa02670)(16);if(!proxy)return ERROR_NOT_ENOUGH_MEMORY;
            *reinterpret_cast<uintptr_t*>(proxy)=live+0x38;*reinterpret_cast<int32_t*>(proxy+8)=0;
            *reinterpret_cast<uintptr_t*>(live+0x40)=proxy;}
        if(*reinterpret_cast<uintptr_t*>(proxy)!=live+0x38||*reinterpret_cast<int32_t*>(proxy+8)<0||*reinterpret_cast<int32_t*>(proxy+8)==INT32_MAX)return ERROR_INVALID_DATA;
        // SetupFor57095e performs this inline atomic AddRef; no callable AddRef exists.
        InterlockedIncrement(reinterpret_cast<volatile LONG*>(proxy+8));acquired=true;return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
using ApplyFindUid=uintptr_t(*)(uint64_t);
using ApplyDestroy=void(*)(void*);
uint32_t apply_relinquish_copy(uintptr_t draft,uintptr_t live,uint64_t uid,uintptr_t ability_class,ApplyFindUid find)noexcept{
    __try{
        if(!draft||!live||draft==live||!uid||!find||
           *reinterpret_cast<const uintptr_t*>(draft)!=ability_class||*reinterpret_cast<const uintptr_t*>(live)!=ability_class||
           *reinterpret_cast<const uint64_t*>(draft+0x18)!=uid||*reinterpret_cast<const uint64_t*>(live+0x18)!=uid||
           *reinterpret_cast<const uintptr_t*>(draft+0x40)||find(uid)!=live||find(uint64_t{1}<<32)!=0)return ERROR_INVALID_DATA;
        // Copy construction duplicates the UID without registering the draft. Base destruction
        // erases by low32 UID alone. The nonzero lookup above also proves key0 is unowned
        // before this private copy relinquishes its borrowed identity.
        *reinterpret_cast<uint64_t*>(draft+0x18)=0;
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t apply_destroy_owned(uintptr_t draft,uintptr_t live,uint64_t uid,uintptr_t ability_class,
                             ApplyFindUid find,ApplyDestroy destroy)noexcept{
    if(!destroy)return ERROR_INVALID_DATA;
    const auto checked=apply_relinquish_copy(draft,live,uid,ability_class,find);if(checked)return checked;
    __try{destroy(reinterpret_cast<void*>(draft));return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t apply_destroy(uintptr_t draft,uintptr_t live,uint64_t uid)noexcept{
    return apply_destroy_owned(draft,live,uid,state.image+0x171c930,
        reinterpret_cast<ApplyFindUid>(state.image+0x47ce20),reinterpret_cast<ApplyDestroy>(state.image+0x140c0));
}
uint32_t apply_release(uintptr_t proxy)noexcept{
    __try{reinterpret_cast<void(*)(void*)>(state.image+0xbfa700)(reinterpret_cast<void*>(proxy));return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t proposal_clear_copy(uintptr_t draft,uintptr_t live,uint64_t uid)noexcept{
    return apply_relinquish_copy(draft,live,uid,state.image+0x171c930,reinterpret_cast<ApplyFindUid>(state.image+0x47ce20));
}
#include "ability_proposal_lifetime.inc"
