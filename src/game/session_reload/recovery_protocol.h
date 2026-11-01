#pragma once
#include "mt2mp/net/session_wire.h"
#include <native_step_recovery_draft.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <string>

namespace mt2mp::game::session_reload::recovery {
using Cohort=MT2FW_MT2_RecoveryCohort;
inline bool same(const Cohort& a,const Cohort& b){
    return std::equal(std::begin(a.physical_ids),std::end(a.physical_ids),std::begin(b.physical_ids))&&
        std::equal(std::begin(a.process_nonces),std::end(a.process_nonces),std::begin(b.process_nonces));
}
inline uint8_t bit(uint64_t slot){return slot>=1&&slot<=4?uint8_t(1u<<(slot-1)):0;}
inline bool valid(const Cohort& value,uint8_t& mask){
    mask=0;
    for(size_t i=0;i<4;++i){
        if(bool(value.physical_ids[i])!=bool(value.process_nonces[i]))return false;
        if(!value.physical_ids[i])continue;
        for(size_t j=0;j<i;++j)if(value.physical_ids[i]==value.physical_ids[j]||value.process_nonces[i]==value.process_nonces[j])return false;
        mask|=uint8_t(1u<<i);
    }
    return true;
}
inline bool nonce(const std::string& value){
    if(value.size()!=36)return false;
    for(size_t i=0;i<value.size();++i){
        if(i==8||i==13||i==18||i==23){if(value[i]!='-')return false;}
        else if(!((value[i]>='0'&&value[i]<='9')||(value[i]>='a'&&value[i]<='f')))return false;
    }
    return true;
}
// Freeze this once before activation. Current transport observations never replace it.
class Original {
public:
    bool bind(const net::session_wire::ProcessCohort& value,uint8_t mask,std::string& error){
        if(bound_||!value.session||!value.lobby||value.local<1||value.local>4||value.physical_ids[0]||value.process_nonces[0])
            return fail(error,"Recovery original process owner is invalid or already bound");
        Cohort actual{};
        for(size_t i=0;i<4;++i){actual.physical_ids[i]=value.physical_ids[i+1];actual.process_nonces[i]=value.process_nonces[i+1];}
        uint8_t observed=0;
        if(!valid(actual,observed)||observed!=mask||!(mask&1)||!(mask&bit(value.local)))
            return fail(error,"Recovery original process owner differs from the complete Step cohort");
        transport_=value;cohort_=actual;bound_=true;error.clear();return true;
    }
    bool observe(const net::session_wire::ProcessCohort& value,Cohort& out,bool& interrupted,std::string& error)const{
        out={};interrupted=false;
        if(!bound_||value.session!=transport_.session||value.lobby!=transport_.lobby||value.local!=transport_.local||
            value.physical_ids[0]||value.process_nonces[0])return fail(error,"Recovery observation changed its transport owner");
        for(size_t i=0;i<4;++i){out.physical_ids[i]=value.physical_ids[i+1];out.process_nonces[i]=value.process_nonces[i+1];}
        uint8_t mask=0;
        if(!valid(out,mask))return fail(error,"Recovery observed process cohort is malformed");
        for(size_t i=0;i<4;++i)if(out.physical_ids[i]&&out.physical_ids[i]!=cohort_.physical_ids[i])
            return fail(error,"Recovery observation changed an original physical slot");
        const auto local=transport_.local-1;
        if(out.physical_ids[0]!=cohort_.physical_ids[0]||out.process_nonces[0]!=cohort_.process_nonces[0]||
            out.physical_ids[local]!=cohort_.physical_ids[local]||out.process_nonces[local]!=cohort_.process_nonces[local])
            return fail(error,"Recovery original Host or local process is absent");
        interrupted=!same(out,cohort_);error.clear();return true;
    }
    const Cohort& cohort()const{return cohort_;}
private:
    static bool fail(std::string& error,const char* cause){error=cause;return false;}
    bool bound_=false;net::session_wire::ProcessCohort transport_{};Cohort cohort_{};
};

// The survivor set is a separate recovery obligation. It never rewrites Step's
// original peer mask and cannot shrink after BEGIN.
class Barrier {
public:
    bool begin(const MT2FW_MT2_RecoveryReceipt& local,const std::string& token,const Cohort& survivors,std::string& error){
        if(session_||local.size!=sizeof(local)||local.phase!=MT2FW_MT2_RECOVERY_INTERRUPTED||!local.lease||
           !local.session||!local.interruption||!local.world_epoch||!local.generation||!local.thread||local.workers_held!=1||
           local.local_slot<1||local.local_slot>4||local.local_process!=local.original.process_nonces[local.local_slot-1]||!nonce(token))
            return fail(error,"Recovery BEGIN requires one exact original local interruption receipt");
        uint8_t original_mask=0,survivor_mask=0,observed_mask=0;
        if(!valid(local.original,original_mask)||original_mask!=local.original_mask||!valid(local.observed,observed_mask)||
           !valid(survivors,survivor_mask)||!(survivor_mask&1)||!(survivor_mask&bit(local.local_slot))||
           (survivor_mask&original_mask)!=survivor_mask)return fail(error,"Recovery BEGIN process masks differ");
        for(size_t i=0;i<4;++i){
            const bool survives=local.original.physical_ids[i]&&local.original.physical_ids[i]==local.observed.physical_ids[i]&&
                local.original.process_nonces[i]==local.observed.process_nonces[i];
            if(bool(survivor_mask&(1u<<i))!=survives||
               (survives&&(survivors.physical_ids[i]!=local.original.physical_ids[i]||survivors.process_nonces[i]!=local.original.process_nonces[i])))
                return fail(error,"Recovery BEGIN omits or replaces a surviving original process");
        }
        if(survivor_mask==original_mask)return fail(error,"Recovery BEGIN has no interrupted original process");
        session_=local.session;local_=local.local_slot;interruption_=local.interruption;lease_=local.lease;
        token_=token;original_=local.original;survivors_=survivors;required_=survivor_mask;error.clear();return true;
    }
    bool held(const MT2FW_MT2_RecoveryReceipt& receipt,std::string& error){
        if(!local_receipt(receipt,error)||receipt.phase!=MT2FW_MT2_RECOVERY_INTERRUPTED||receipt.workers_held!=1)
            return fail(error,"Recovery held acknowledgement lacks its actual local BEGIN receipt");
        held_|=bit(local_);error.clear();return true;
    }
    bool acknowledge(uint64_t sender,uint64_t process,uint8_t mask,bool retired,std::string& error){
        if(!session_||sender<1||sender>4||!(required_&bit(sender))||mask!=bit(sender)||process!=survivors_.process_nonces[sender-1])
            return fail(error,"Recovery acknowledgement is not from its frozen original survivor");
        if(retired&&!published_)return fail(error,"Recovery retirement preceded the fresh Host publication");
        if(retired)retired_|=mask;else held_|=mask;
        error.clear();return true;
    }
    bool publish(const MT2FW_MT2_RecoveryReceipt& receipt,std::string& error){
        if(local_!=1||!all_held()||!local_receipt(receipt,error)||receipt.phase!=MT2FW_MT2_RECOVERY_PUBLISHED||
           receipt.prepare_attempted!=1||receipt.prepare_returned!=1||receipt.capture_attempted!=1||receipt.capture_returned!=1||
           !receipt.file_bytes||!receipt.companion_bytes||empty(receipt.witness_sha256)||empty(receipt.file_sha256)||empty(receipt.companion_sha256))
            return fail(error,"Recovery publication lacks the complete fresh Host capture receipt");
        published_=true;error.clear();return true;
    }
    bool all_held()const{return session_&&held_==required_;}
    bool accept_host_publication(uint64_t session,const std::string& token,const Cohort& original,const Cohort& survivors,std::string& error){
        if(local_==1||session!=session_||token!=token_||!same(original,original_)||!same(survivors,survivors_)||!(held_&bit(local_)))
            return fail(error,"Recovery Host publication differs from the locally held survivor attempt");
        published_=true;error.clear();return true;
    }
    bool all_retired()const{return published_&&retired_==required_;}
    uint8_t required()const{return required_;}
    uint8_t held_mask()const{return held_;}
    uint8_t retired_mask()const{return retired_;}
    const Cohort& survivors()const{return survivors_;}
    const std::string& token()const{return token_;}
private:
    static bool fail(std::string& error,const char* cause){error=cause;return false;}
    static bool empty(const unsigned char (&hash)[32]){return std::all_of(std::begin(hash),std::end(hash),[](auto byte){return byte==0;});}
    bool local_receipt(const MT2FW_MT2_RecoveryReceipt& value,std::string& error)const{
        if(value.size!=sizeof(value)||value.session!=session_||value.lease!=lease_||value.interruption!=interruption_||value.local_slot!=local_||
           value.local_process!=survivors_.process_nonces[local_-1]||value.survivor_mask!=required_||!same(value.original,original_)||
           !same(value.survivors,survivors_)||!std::memchr(value.recovery_nonce,0,sizeof(value.recovery_nonce))||token_!=value.recovery_nonce)
            return fail(error,"Recovery receipt differs from its fixed attempt and local process");
        error.clear();return true;
    }
    uint64_t session_=0,local_=0,interruption_=0,lease_=0;
    uint8_t required_=0,held_=0,retired_=0;bool published_=false;
    std::string token_;Cohort original_{},survivors_{};
};
}
