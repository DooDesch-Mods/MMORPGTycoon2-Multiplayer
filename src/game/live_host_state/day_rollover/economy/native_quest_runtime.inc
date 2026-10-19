uint32_t quest_runtime_byte(uintptr_t address,uint8_t expected,uint8_t target,bool& exchanged)noexcept{
    __try{exchanged=uint8_t(_InterlockedCompareExchange8(reinterpret_cast<volatile char*>(address),char(target),char(expected)))==expected;return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t quest_runtime_ads(uintptr_t image,uintptr_t quest)noexcept{
    __try{reinterpret_cast<void(*)(uintptr_t)>(image+0x4d6c00)(quest);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
template<class Guard>bool store_quest_runtime(const Pin& p,const QuestOwner& owner,uintptr_t quest,int32_t sequence,
    const npc_quest_runtime::Value& old,const npc_quest_runtime::Value& target,Guard&& fence,std::string& e){
    struct Calls {
        const Pin& p;const QuestOwner& owner;uintptr_t quest;int32_t sequence;
        std::function<bool(std::string&)> fence;
        bool guard(std::string& error){
            if(!fence(error))return false;
            std::vector<QuestOwner> current;
            if(!npc_probe::action_runtime::state_owners(p.context,p.binding,current,error))return false;
            for(const auto& row:current)if(row.key==owner.key){
                if(row.npc!=owner.npc||row.quests!=owner.quests)return fail(error,"Quest runtime current NPC/ordered Quest membership changed");
                for(const auto& slot:row.quests)if(slot.first==sequence&&slot.second==quest)return true;
            }
            return fail(error,"Quest runtime canonical owner or selected sequence absent");
        }
        bool read(npc_quest_runtime::Value& out,std::string& error){QuestMemory m;return npc_quest_runtime::read(m,p.image,quest,out,error);}
        bool compare32(uintptr_t address,uint32_t before,uint32_t after,std::string& error){
            return compare_fixed_i32(address,std::bit_cast<int32_t>(before),std::bit_cast<int32_t>(after),error);
        }
        bool compare8(uintptr_t address,uint8_t before,uint8_t after,std::string& error){
            bool exchanged=false;const auto seh=quest_runtime_byte(address,before,after,exchanged);
            if(seh)return fail(error,"Quest runtime byte CAS exception="+std::to_string(seh));
            return exchanged||fail(error,"Quest runtime prior flag changed before CAS");
        }
        bool advertise(std::string& error){
            std::vector<unsigned char> bytes(192);
            if(!memory.copy(p.image+0x4d6c00,bytes.data(),bytes.size(),error))return false;
            if(mt2mp::sha256(bytes)!="b2b4ef433836335fa6de391315d0398860a1bd3c20ff1e70a62ea55feade575c")return fail(error,"Quest original SetAdvertisements body differs");
            const auto seh=quest_runtime_ads(p.image,quest);
            return !seh||fail(error,"Quest original SetAdvertisements exception="+std::to_string(seh)+"; no replay");
        }
    } calls{p,owner,quest,sequence,std::forward<Guard>(fence)};
    const auto receipt=npc_quest_runtime::apply(calls,quest,old,target);
    if(!receipt.confirmed)return fail(e,receipt.cause+(receipt.unknown?"; Quest runtime Apply Unknown; no replay":""));
    return true;
}
