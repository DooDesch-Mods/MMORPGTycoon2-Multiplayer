#pragma once
#include <native_steps_draft.h>
#include <algorithm>
#include <cstring>
#include <functional>
#include <string>

namespace mt2mp::game::ordered_session {
// The enclosing session retains this owner until both Step and role-provider
// retirement acknowledge that no callback can use its storage.
class ProofOwner {
public:
    using Read=std::function<bool(MT2FW_MT2_StepProofReply&,uint32_t&,std::string&)>;
    using ReadRow=std::function<bool(uint32_t,MT2FW_MT2_StepProofReply&,std::string&)>;
    explicit ProofOwner(Read read,ReadRow rows={},Read local={}):read_(std::move(read)),local_(std::move(local)),rows_(std::move(rows)){}
    ProofOwner(const ProofOwner&)=delete;
    ProofOwner& operator=(const ProofOwner&)=delete;
    bool bind(const MT2FW_MT2_PatchesV1& service,std::string& error){
        if(attempted_||!read_||service.size!=sizeof(service)||service.version!=MT2FW_MT2_PATCHES_VERSION||
           !service.context||!service.bind_session_provider||!service.unbind_session_provider){
            error="Ordered proof provider requires an unused owner and complete Patches service";return false;}
        attempted_=true;MT2FW_MT2_StepProofReply actual{};
        if(!read_(actual,role_,error)||!valid(actual,error)||!valid_role(role_,error))return false;
        if(actual.phase!=MT2FW_MT2_STEP_PROOF_PRELOAD){error="Ordered proof must bind before the original load request";return false;}
        if(actual.region_rng_mode==2&&!rows_){error="Ordered preserved Region proof requires its retained row reader";return false;}
        frozen_=actual;service_=service;
        MT2FW_MT2_PatchReceipt receipt{};receipt.size=sizeof(receipt);
        const auto result=service_.bind_session_provider(service_.context,&role_call,this,&token_,&receipt);
        if(result!=MT2FW_MT2_OK){error=api_error("bind",result,receipt);return false;}
        if(!token_){error="Ordered role provider returned no registration token";return false;}
        error.clear();return true;
    }
    bool unbind(std::string& error){
        if(busy_){error="Ordered role provider unbind requires outside its callback";return false;}
        retiring_=true;
        if(!token_){error.clear();return true;}
        MT2FW_MT2_PatchReceipt receipt{};receipt.size=sizeof(receipt);
        const auto result=service_.unbind_session_provider(service_.context,token_,&receipt);
        if(result!=MT2FW_MT2_OK){error=api_error("unbind",result,receipt);return false;}
        if(receipt.size!=sizeof(receipt)||receipt.result!=MT2FW_MT2_OK){error="Ordered role provider unbind receipt differs";return false;}
        retired_ack_=true;token_=0;error.clear();return true;
    }
    bool retired_identity(uint64_t session,const std::string& snapshot,uint64_t generation,std::string& error)const{
        if(!retired_ack_||!retiring_||token_||busy_||frozen_.session!=session||frozen_.generation!=generation||snapshot!=frozen_.snapshot_uuid){
            error="Ordered role provider lacks its actual unbind acknowledgement for the retained snapshot";return false;
        }
        error.clear();return true;
    }
    uint64_t token()const{return token_;}
    const std::string& cause()const{return cause_;}
    static MT2FW_MT2_StepProofFn callback(){return &proof_call;}
    static MT2FW_MT2_StepProofFn recovery_callback(){return &local_proof_call;}
private:
    Read read_,local_;ReadRow rows_;MT2FW_MT2_PatchesV1 service_{};MT2FW_MT2_StepProofReply frozen_{};
    uint64_t token_=0;uint32_t role_=0;bool attempted_=false,retiring_=false,busy_=false,retired_ack_=false;std::string cause_;
    static bool valid_role(uint32_t role,std::string& error){
        if(role==MT2FW_MT2_SESSION_HOST||role==MT2FW_MT2_SESSION_CLIENT)return true;
        error="Ordered source has no actual Host or Client role";return false;
    }
    static bool valid(const MT2FW_MT2_StepProofReply& value,std::string& error){
        if(value.size!=sizeof(value)||!value.session||!value.thread||!value.generation||!value.snapshot_bytes||
           value.seed_present!=1||!value.frozen_peer_mask||value.frozen_peer_mask>15||!(value.frozen_peer_mask&1)||
           !std::memchr(value.snapshot_uuid,0,sizeof(value.snapshot_uuid))||!value.snapshot_uuid[0]||
           value.phase<MT2FW_MT2_STEP_PROOF_PRELOAD||value.phase>MT2FW_MT2_STEP_PROOF_LOADED||
           value.resume_workers_known||value.resume_workers||
           std::any_of(std::begin(value.initial_witness_sha256),std::end(value.initial_witness_sha256),[](auto b){return b!=0;})||
           std::any_of(std::begin(value.reserved),std::end(value.reserved),[](auto b){return b!=0;})){
            error="Ordered native proof is incomplete or claims Framework-owned bootstrap state";return false;}
        if((value.region_rng_mode!=1&&value.region_rng_mode!=2)||
           (value.region_rng_mode==1?value.region_rng_count!=0:value.region_rng_count<1||value.region_rng_count>4096)||
           value.region_rng_index!=UINT32_MAX||value.region_rng_id||value.region_rng_extra||value.region_rng_reserved||
           std::any_of(std::begin(value.region_rng_words),std::end(value.region_rng_words),[](auto word){return word!=0;})||
           std::all_of(std::begin(value.region_rng_sha256),std::end(value.region_rng_sha256),[](auto byte){return byte==0;})){
            error="Ordered Region RNG metadata mode, count, commitment or empty row differs";return false;
        }
        if(value.phase==MT2FW_MT2_STEP_PROOF_LOADED){
            if(!value.world_epoch||!value.game||!value.mode||!value.state||!value.world||!value.map){
                error="Ordered loaded proof lacks sealed native world identity";return false;}
        }else if(value.world_epoch||value.game||value.mode||value.state||value.world||value.map){
            error="Ordered preload/loading proof unexpectedly claims a sealed world";return false;}
        return true;
    }
    bool current(MT2FW_MT2_StepProofReply& actual,std::string& error,bool recovery=false){
        if(retiring_||busy_||!attempted_||!read_){error="Ordered proof owner is retiring, reentrant or unbound";return false;}
        busy_=true;struct Clear{bool& flag;~Clear(){flag=false;}} clear{busy_};
        uint32_t role=0;
        const auto& read=recovery?local_:read_;
        if(!read){error="Recovery local proof reader is not bound";return false;}
        if(!read(actual,role,error)||!valid(actual,error)||!valid_role(role,error))return false;
        if(recovery&&actual.phase!=MT2FW_MT2_STEP_PROOF_LOADED){error="Recovery local proof requires the completed original native load";return false;}
        if(role!=role_){error="Ordered local physical role changed after registration";return false;}
        if(actual.session!=frozen_.session||actual.thread!=frozen_.thread||actual.generation!=frozen_.generation||
           actual.snapshot_bytes!=frozen_.snapshot_bytes||actual.agreed_seed!=frozen_.agreed_seed||
           actual.region_rng_mode!=frozen_.region_rng_mode||actual.region_rng_count!=frozen_.region_rng_count||
           std::memcmp(actual.region_rng_sha256,frozen_.region_rng_sha256,32)||
           actual.seed_present!=frozen_.seed_present||actual.frozen_peer_mask!=frozen_.frozen_peer_mask||
           std::strcmp(actual.snapshot_uuid,frozen_.snapshot_uuid)||
           std::memcmp(actual.snapshot_sha256,frozen_.snapshot_sha256,32)){
            error="Ordered proof session, seed, file or generation changed after registration";return false;}
        return true;
    }
    static std::string api_error(const char* operation,MT2FW_MT2_Result code,const MT2FW_MT2_PatchReceipt& receipt){
        return std::string("Ordered role provider ")+operation+" result="+std::to_string(code)+": "+
            std::string(receipt.cause,std::find(std::begin(receipt.cause),std::end(receipt.cause),'\0'));
    }
    static void set_cause(MT2FW_MT2_StepProofReply& out,const std::string& cause){
        const auto count=std::min(cause.size(),sizeof(out.cause)-1);std::memcpy(out.cause,cause.data(),count);out.cause[count]=0;
    }
    static MT2FW_MT2_Result MT2FW_CALL proof_call(void* user,const MT2FW_MT2_StepProofQuery* query,MT2FW_MT2_StepProofReply* out){return proof<false>(user,query,out);}
    static MT2FW_MT2_Result MT2FW_CALL local_proof_call(void* user,const MT2FW_MT2_StepProofQuery* query,MT2FW_MT2_StepProofReply* out){return proof<true>(user,query,out);}
    template<bool RecoveryLocal>
    static MT2FW_MT2_Result proof(void* user,const MT2FW_MT2_StepProofQuery* query,MT2FW_MT2_StepProofReply* out){
        if(!user||!query||!out||out->size!=sizeof(*out))return MT2FW_MT2_INVALID;
        auto& self=*static_cast<ProofOwner*>(user);*out={};out->size=sizeof(*out);
        try{
            if(query->size!=sizeof(*query)||query->reserved||query->version!=MT2FW_MT2_STEP_DRAFT_VERSION||!self.token_||
               query->session!=self.frozen_.session||query->thread!=self.frozen_.thread||query->session_provider_token!=self.token_){
                self.cause_="Ordered proof query differs from the registered session owner";
            }else{
                MT2FW_MT2_StepProofReply actual{};
                if constexpr(RecoveryLocal){if(query->region_rng_index!=UINT32_MAX){
                    self.cause_="Recovery local proof cannot reapply initial Region RNG rows";
                    set_cause(*out,self.cause_);return MT2FW_MT2_UNAVAILABLE;
                }}
                if(self.current(actual,self.cause_,RecoveryLocal)){
                    if(query->region_rng_index!=UINT32_MAX){
                        if(actual.region_rng_mode!=2||query->region_rng_index>=actual.region_rng_count||!self.rows_){
                            self.cause_="Ordered Region RNG row query is outside preserved snapshot census";
                            set_cause(*out,self.cause_);return MT2FW_MT2_UNAVAILABLE;
                        }
                        auto row=actual;
                        if(!self.rows_(query->region_rng_index,row,self.cause_)){
                            set_cause(*out,self.cause_);return MT2FW_MT2_UNAVAILABLE;
                        }
                        if(row.region_rng_index!=query->region_rng_index||row.region_rng_id<0||row.region_rng_reserved||
                           std::all_of(std::begin(row.region_rng_words),std::end(row.region_rng_words),[](auto word){return word==0;})){
                            self.cause_="Ordered Region RNG row reply differs from exact query";
                            set_cause(*out,self.cause_);return MT2FW_MT2_UNAVAILABLE;
                        }
                        actual.region_rng_index=row.region_rng_index;actual.region_rng_id=row.region_rng_id;
                        actual.region_rng_extra=row.region_rng_extra;
                        std::copy(std::begin(row.region_rng_words),std::end(row.region_rng_words),std::begin(actual.region_rng_words));
                    }
                    actual.session_provider_token=self.token_;*out=actual;self.cause_.clear();return MT2FW_MT2_OK;
                }
            }
        }catch(const std::exception& exception){self.cause_=std::string("Ordered proof exception: ")+exception.what();}
        catch(...){self.cause_="Ordered proof unknown exception";}
        set_cause(*out,self.cause_);return MT2FW_MT2_UNAVAILABLE;
    }
    static MT2FW_MT2_Result MT2FW_CALL role_call(void* user,const MT2FW_MT2_SessionQuery* query,MT2FW_MT2_SessionReply* out){
        if(!user||!query||!out||query->size!=sizeof(*query)||query->reserved||out->size!=sizeof(*out))return MT2FW_MT2_INVALID;
        auto& self=*static_cast<ProofOwner*>(user);*out={};out->size=sizeof(*out);
        try{
            MT2FW_MT2_StepProofReply actual{};
            if(query->thread!=self.frozen_.thread){self.cause_="Ordered role query belongs to another thread";return MT2FW_MT2_UNAVAILABLE;}
            if(!self.current(actual,self.cause_))return MT2FW_MT2_UNAVAILABLE;
            out->role=self.role_;out->epoch=query->epoch;self.cause_.clear();return MT2FW_MT2_OK;
        }catch(const std::exception& exception){self.cause_=std::string("Ordered role exception: ")+exception.what();}
        catch(...){self.cause_="Ordered role unknown exception";}
        return MT2FW_MT2_UNAVAILABLE;
    }
};
}
