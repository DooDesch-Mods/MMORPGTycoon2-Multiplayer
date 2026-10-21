#pragma once
#include "session_binding.h"
#include <limits>

namespace mt2mp::net::local_membership {
using Slots=session_binding::Slots;
class Registry {
public:
    bool initialize(uint64_t local,uint64_t instance,std::string& error){
        if(local<1||local>4||!instance)return fail(error,"local membership requires slot1..4 and a process instance");
        *this=Registry{};local_=local;physical_[local]=instance;error.clear();return true;
    }
    bool observe(uint64_t peer,uint64_t instance,uint64_t sequence,bool& fresh,std::string& error,bool participating=true){
        fresh=false;
        if(!local_||peer<1||peer>4||peer==local_||!instance||!sequence)
            return fail(error,"local control sender/process/sequence invalid");
        if(instance==physical_[local_])return fail(error,"local control process instance equals receiver");
        if(peer==1&&seen_[1]&&seen_[1]!=instance)
            return fail(error,"original local Host process changed; no Host migration");
        if(seen_[peer]!=instance){seen_[peer]=instance;sequences_[peer]=0;}
        if(sequence<=sequences_[peer]){error.clear();return true;}
        const auto member=(peer==1||participating)?instance:0;
        if(generation_&&physical_[peer]!=member)changed_=true;
        sequences_[peer]=sequence;physical_[peer]=member;fresh=true;error.clear();return true;
    }
    void departed(uint64_t peer){if(peer>=1&&peer<=4&&peer!=local_){if(generation_&&physical_[peer])changed_=true;physical_[peer]=0;}}
    bool stage(uint64_t generation,const Slots& slots,std::string& error){
        if(local_==1)return fail(error,"Client cannot publish local Host membership");
        if(!generation){
            if(slots!=Slots{})return fail(error,"unbound local roster has nonzero slots");
            error.clear();return true;
        }
        if(!session_binding::valid(slots)||slots[1]!=physical_[1])
            return fail(error,"local roster differs from the original physical Host");
        if(generation<generation_){error.clear();return true;}
        if(generation==generation_){
            if(slots!=bound_)return fail(error,"local roster changed without a binding generation");
            error.clear();return true;
        }
        if(pending_generation_==generation&&pending_!=slots)
            return fail(error,"conflicting local roster at one pending generation");
        if(generation<pending_generation_){error.clear();return true;}
        pending_=slots;pending_generation_=generation;error.clear();return true;
    }
    bool commit_host(std::string& error){
        if(local_!=1||!session_binding::valid(physical_))return fail(error,"local roster commit requires the original Host");
        if(generation_==(std::numeric_limits<uint64_t>::max)())return fail(error,"local roster generation exhausted");
        bound_=physical_;++generation_;changed_=false;error.clear();return true;
    }
    bool adopt_client(std::string& error){
        if(local_==1)return fail(error,"Host cannot adopt a remote local roster");
        if(pending_generation_<=generation_||pending_[local_]!=physical_[local_]){
            error="info: awaiting Host roster for the current local process";return false;
        }
        bound_=pending_;generation_=pending_generation_;pending_={};pending_generation_=0;
        error.clear();return true;
    }
    bool changed()const{return local_==1&&generation_&&(changed_||physical_!=bound_);}
    uint8_t mask()const{return session_binding::mask(generation_?bound_:physical_);}
    uint8_t retirement_mask()const{
        if(local_!=1)return mask();
        uint8_t result=0;
        for(size_t i=1;i<bound_.size();++i)if(bound_[i]&&bound_[i]==physical_[i])result|=uint8_t(1u<<(i-1));
        return result;
    }
    uint64_t generation()const{return generation_;}
    uint64_t instance()const{return local_?physical_[local_]:0;}
    uint64_t host_instance()const{return physical_[1];}
    const Slots& bound()const{return bound_;}
private:
    static bool fail(std::string& error,const char* cause){error=cause;return false;}
    uint64_t local_=0,generation_=0,pending_generation_=0;
    bool changed_=false;
    Slots physical_{},bound_{},pending_{},seen_{},sequences_{};
};
}
