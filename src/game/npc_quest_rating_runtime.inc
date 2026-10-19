namespace rating_authority {
using RatingCall=void(*)(uintptr_t,uintptr_t);
RatingCall original=nullptr;
bool attempted=false,ready=false;
thread_local bool entered=false;
std::array<unsigned char,32> installed_entry{};
std::array<unsigned char,64> trampoline{};
std::string failure;
uint64_t host_completed=0,client_suppressed=0,failures=0;
json last=nullptr;
void entry(uintptr_t,uintptr_t);
bool code(Memory& m,bool hooked,std::string& e){
    struct Code {uintptr_t rva;size_t size;const char* sha;};
    constexpr Code values[]{
        {0x4d6a10,400,"f797d65603f47aa77b48ae8361604a1efec366953303007e13887870bbc1d4d1"},
        {0x4d6c00,192,"b2b4ef433836335fa6de391315d0398860a1bd3c20ff1e70a62ea55feade575c"},
        {0x4d8ed0,384,"e1fb929baf777778a5f6f071935203c62ffe0b212993636a2999c48b3a8fd48a"},
        {0x4d9260,480,"390294cb409016c055a297603acc1be5a41cb6e3ac6c3f48f7c36ce77fde7e55"},
        {0x4d4bf0,1072,"ad468ba264eabdf340f7fa479628b4e713442275afe32b4a6507fa733d71cce2"}
    };
    for(const auto& c:values){std::string bytes(c.size,'\0'),sha;
        if(!m.copy(state.image+c.rva,bytes.data(),bytes.size(),e))return false;
        if(c.rva==0x4d6a10&&hooked){
            std::array<unsigned char,32> actual_entry{};std::array<unsigned char,64> actual_trampoline{};
            if(!original||!m.copy(state.image+c.rva,actual_entry.data(),actual_entry.size(),e)||actual_entry!=installed_entry||
               !m.copy(uintptr_t(original),actual_trampoline.data(),actual_trampoline.size(),e)||actual_trampoline!=trampoline||
               !quest_bridge::sha256(bytes.substr(5),sha,e)||sha!="72aae1f855f15dd4b3ceb5f40509a7bb0ba0baf3be640d570c93f708d67fc316")
                return fail(e,e.empty()?"Quest rating owned entry/trampoline/tail differs":e);
        }else if(!quest_bridge::sha256(bytes,sha,e)||sha!=c.sha)return fail(e,e.empty()?"Quest rating original body differs RVA="+std::to_string(c.rva):e);
    }
    return true;
}
uint32_t original_call(uintptr_t quest,uintptr_t path)noexcept{
    __try{original(quest,path);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t advertise_call(uintptr_t quest)noexcept{
    __try{reinterpret_cast<void(*)(uintptr_t)>(state.image+0x4d6c00)(quest);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
struct Calls {
    a::Context context;a::Binding binding;
    bool bound_active=false;
    std::string owner_key;int32_t sequence=-1;uintptr_t npc=0,task=0;uint8_t finished=0;
    bool active(bool& client){
        if(!live_host_state::economic_fence().quest_binding(context,binding))return false;
        bound_active=true;client=context.role==a::Role::Client;return true;
    }
    bool owner(uintptr_t quest,bool& owned,std::string& e){
        owned=false;
        if(GetCurrentThreadId()!=state.thread)return fail(e,"Quest rating completion requires bound Mainthread");
        if(live_host_state::economic_fence().locked())return fail(e,"Quest rating completion economic fence is locked: "+live_host_state::economic_fence().cause());
        Memory m;uintptr_t ap=0;if(!field(m,quest,0,ap,e))return false;
        if(ap!=state.image+0x174ac10)return true;
        if(!code(m,true,e))return false;
        std::vector<action_runtime::StateOwner> owners;
        if(!action_runtime::state_owners(context,binding,owners,e))return false;
        bool found=false;
        for(const auto& row:owners)for(const auto& slot:row.quests)if(slot.second==quest){
            if(found)return fail(e,"Quest rating duplicate canonical Quest owner");found=true;
            if(!owner_key.empty()&&(row.key!=owner_key||npc!=row.npc||sequence!=slot.first))return fail(e,"Quest rating canonical owner changed across original call");
            owner_key=row.key;npc=row.npc;sequence=slot.first;
        }
        if(!found)return fail(e,"Quest rating receiver lacks current canonical NPC/Quest membership");
        if(!field(m,quest,0x218,task,e)||!task)return fail(e,e.empty()?"Quest rating Tick has no original task":e);
        uintptr_t task_ap=0;
        if(!field(m,task,0,task_ap,e)||task_ap!=state.image+0x1714ac0||!field(m,task,0xe,finished,e)||finished!=1)
            return fail(e,e.empty()?"Quest rating original task class/finished differs":e);
        owned=true;return true;
    }
    bool read(uintptr_t quest,npc_quest_runtime::Value& out,std::string& e){Memory m;return npc_quest_runtime::read(m,state.image,quest,out,e);}
    bool original(uintptr_t quest,uintptr_t path,std::string& e){const auto seh=original_call(quest,path);return !seh||fail(e,"Quest original rating exception="+std::to_string(seh)+"; no replay");}
    bool advertise(uintptr_t quest,std::string& e){const auto seh=advertise_call(quest);return !seh||fail(e,"Quest original advertisement exception="+std::to_string(seh)+"; no replay");}
};
json value(const npc_quest_runtime::Value& v){return {{"rating_bits",v.rating_bits},{"rating_set",v.rating_set},{"xp_reward",v.xp_reward},{"gold_reward",v.gold_reward}};}
void rejected(const std::string& why){
    ++failures;if(failure.empty())failure=why.empty()?"Quest rating completion failed without cause":why;
    live_host_state::economic_fence().journal_failed(failure);
}
void entry(uintptr_t quest,uintptr_t path){
    const auto caller=uintptr_t(_ReturnAddress());
    try{
        if(entered){rejected("Quest rating nested completion entry; no replay");return;}
        entered=true;struct Exit {~Exit(){entered=false;}} exit;
        Calls calls;
        const auto receipt=npc_quest_runtime::complete(calls,quest,path,caller,state.image);
        if(calls.bound_active){
            if(receipt.confirmed){if(receipt.client)++client_suppressed;else ++host_completed;}
            last={{"owner_key",calls.owner_key},{"sequence_id",calls.sequence},{"native_epoch",calls.binding.native_epoch},
                {"native_generation",calls.binding.native_generation},{"snapshot_uuid",calls.context.snapshot_uuid},
                {"caller_rva",caller>=state.image?caller-state.image:0},{"path_present",path!=0},{"task_present",calls.task!=0},
                {"task_finished",calls.finished},{"owned",receipt.owned},{"client",receipt.client},{"attempted",receipt.attempted},{"returned",receipt.returned},
                {"suppressed",receipt.suppressed},{"confirmed",receipt.confirmed},{"cause",receipt.cause},
                {"before",value(receipt.before)},{"after",value(receipt.after)}};
        }
        if(!receipt.confirmed)rejected(receipt.cause);
    }catch(const std::exception& x){rejected(std::string("Quest rating boundary exception: ")+x.what());}
     catch(...){rejected("Quest rating nonstandard boundary exception");}
}
}
bool rating_enable(std::string& e){
    namespace r=rating_authority;Memory m;
    if(r::ready)return r::code(m,true,e);
    if(r::attempted)return fail(e,"Quest rating hook already attempted: "+r::failure);
    r::attempted=true;
    const auto rejected=[&](const std::string& why){r::failure=why;return fail(e,why);};
    if(!r::code(m,false,e))return rejected(e);
    auto status=MH_CreateHook(reinterpret_cast<void*>(state.image+0x4d6a10),reinterpret_cast<void*>(&r::entry),reinterpret_cast<void**>(&r::original));
    const auto hook_error=[&](const char* operation){std::string why=std::string("Quest rating ")+operation+" status="+std::to_string(status);
#ifdef MT2MP_FRAMEWORK_PLUGIN
        why+=" original="+hooks::framework::process_backend().error();
#endif
        return rejected(why);};
    if(status!=MH_OK||!r::original)return hook_error("prepare");
    status=MH_EnableHook(reinterpret_cast<void*>(state.image+0x4d6a10));if(status!=MH_OK)return hook_error("enable");
    if(!m.copy(state.image+0x4d6a10,r::installed_entry.data(),r::installed_entry.size(),e)||
       !m.copy(uintptr_t(r::original),r::trampoline.data(),r::trampoline.size(),e))return rejected(e);
    if(!r::code(m,true,e))return rejected(e);r::ready=true;return true;
}
json rating_status(){return {{"ready",rating_authority::ready},{"host_completed",rating_authority::host_completed},
    {"client_suppressed",rating_authority::client_suppressed},{"failures",rating_authority::failures},
    {"cause",rating_authority::failure},{"last",rating_authority::last}};}
