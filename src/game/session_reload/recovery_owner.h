#pragma once
#include "recovery_protocol.h"
#include "ordered_proof_owner.h"
#include <functional>

namespace mt2mp::game::session_reload::recovery {
// Storage remains owned by ordered_startup until actual Step unbind completes.
class Owner {
public:
    using Poll=std::function<bool(net::session_wire::ProcessCohort&,std::string&)>;
    using Service=std::function<bool(Owner&,const MT2FW_MT2_RecoveryReceipt&,std::string&)>;
    using Witness=std::function<bool(const MT2FW_MT2_RecoveryReceipt&,std::array<uint8_t,32>&,std::string&)>;
    using Publication=std::function<bool(const MT2FW_MT2_RecoveryReceipt&,const MT2FW_MT2_RecoveryPublication&,std::string&)>;
    using Capture=std::function<bool(std::string&)>;
    bool bind(const MT2FW_MT2_StepRecoveryDraftV1& api,const MT2FW_MT2_StepReceipt& step,uint8_t mask,
              ordered_session::ProofOwner& proof,Poll poll,Service service,Witness witness,Publication publication,std::string& error){
        if(attempted_||api.size!=sizeof(api)||api.version!=MT2FW_MT2_STEP_RECOVERY_DRAFT_VERSION||!api.context||
           !api.bind||!api.read||!api.operation||step.size!=sizeof(step)||step.phase!=MT2FW_MT2_STEP_ARMED||
           !step.lease||!step.session||!step.thread||!proof.token()||!poll||!service||!witness||!publication)
            return fail(error,"Recovery binding requires an unused owner, exact optional API and armed Step lease");
        attempted_=true;net::session_wire::ProcessCohort transport;
        if(!poll(transport,error)||transport.session!=step.session||!original_.bind(transport,mask,error)){
            if(error.empty())error="Recovery process handshake differs from armed Step session";return false;
        }
        api_=api;proof_=&proof;poll_=std::move(poll);service_=std::move(service);witness_=std::move(witness);publication_=std::move(publication);
        lease_=step.lease;session_=step.session;thread_=step.thread;local_=uint32_t(transport.local);
        MT2FW_MT2_RecoverySpec spec{};spec.size=sizeof(spec);spec.lease=lease_;spec.local_slot=local_;
        spec.session_provider_token=proof.token();spec.original=original_.cohort();spec.user=this;
        spec.observe=&observe_call;spec.local_proof=&local_call;spec.service=&service_call;
        spec.witness=&witness_call;spec.publication=&publication_call;
        receipt_={};receipt_.size=sizeof(receipt_);
        const auto result=api_.bind(api_.context,&spec,&receipt_);
        if(result!=MT2FW_MT2_OK)return api_failure("bind",result,error);
        if(!identity(receipt_)||receipt_.phase!=MT2FW_MT2_RECOVERY_WATCHING)return fail(error,"Recovery bind receipt differs");
        bound_=true;error.clear();return true;
    }
    bool operation(uint32_t operation,const std::string& token,const Cohort& survivors,uint8_t mask,
                   const MT2FW_MT2_RecoveryPublication* publication,std::string& error,Capture capture={}){
        if(!bound_||!servicing_||!cause_.empty()||receipt_.phase==MT2FW_MT2_RECOVERY_WATCHING||
           !receipt_.interruption||!nonce(token)||bool(capture)!=(operation==MT2FW_MT2_RECOVERY_CAPTURE)||
           (publication&&(operation!=MT2FW_MT2_RECOVERY_PUBLISH)))return fail(error,"Recovery operation lacks its exact active service owner");
        if(capture&&capture_consumed_)return fail(error,"Recovery capture is already consumed; no replay");
        MT2FW_MT2_RecoveryRequest request{};request.size=sizeof(request);request.operation=operation;
        request.lease=lease_;request.interruption=receipt_.interruption;request.survivor_mask=mask;request.survivors=survivors;
        std::memcpy(request.recovery_nonce,token.c_str(),token.size()+1);
        if(publication)request.publication=*publication;
        if(capture){capture_consumed_=true;capture_=std::move(capture);}
        MT2FW_MT2_RecoveryReceipt actual{};actual.size=sizeof(actual);
        const auto result=api_.operation(api_.context,&request,operation==MT2FW_MT2_RECOVERY_CAPTURE?&capture_call:nullptr,
                                       operation==MT2FW_MT2_RECOVERY_CAPTURE?this:nullptr,&actual);
        receipt_=actual;
        if(result!=MT2FW_MT2_OK)return api_failure("operation "+std::to_string(operation),result,error);
        if(!identity(actual))return fail(error,"Recovery operation changed its native owner");
        error.clear();return true;
    }
    const MT2FW_MT2_RecoveryReceipt& receipt()const{return receipt_;}
    bool read(MT2FW_MT2_RecoveryReceipt& out,std::string& error){
        out={};out.size=sizeof(out);
        if(!bound_||!cause_.empty())return fail(error,"Recovery read requires its retained healthy owner");
        const auto result=api_.read(api_.context,lease_,&out);
        if(result!=MT2FW_MT2_OK){receipt_=out;return api_failure("read",result,error);}
        if(!identity(out))return fail(error,"Recovery read changed its native owner");
        error.clear();return true;
    }
    const std::string& cause()const{return cause_;}
    bool bound()const{return bound_;}
    bool transport_interrupted(bool& interrupted,std::string& error){
        interrupted=false;
        if(!bound_||!cause_.empty())return fail(error,"Recovery transport read requires its retained owner");
        net::session_wire::ProcessCohort current;Cohort observed{};
        return poll_(current,error)&&original_.observe(current,observed,interrupted,error);
    }
private:
    MT2FW_MT2_StepRecoveryDraftV1 api_{};MT2FW_MT2_RecoveryReceipt receipt_{};
    Original original_;ordered_session::ProofOwner* proof_=nullptr;
    Poll poll_;Service service_;Witness witness_;Publication publication_;Capture capture_;
    uint64_t lease_=0,session_=0,thread_=0;uint32_t local_=0;
    bool attempted_=false,bound_=false,servicing_=false,capture_consumed_=false;
    std::string cause_;
    bool identity(const MT2FW_MT2_RecoveryReceipt& value)const{
        return value.size==sizeof(value)&&value.lease==lease_&&value.session==session_&&value.thread==thread_&&
            value.local_slot==local_&&local_>=1&&local_<=4&&value.local_process==original_.cohort().process_nonces[local_-1]&&
            same(value.original,original_.cohort());
    }
    bool fail(std::string& error,const std::string& cause){if(cause_.empty())cause_=cause;error=cause_;return false;}
    bool api_failure(const std::string& operation,MT2FW_MT2_Result result,std::string& error){
        const std::string detail(receipt_.cause,std::find(std::begin(receipt_.cause),std::end(receipt_.cause),'\0'));
        return fail(error,"Framework recovery "+operation+" result="+std::to_string(result)+": "+detail);
    }
    static void copy_cause(char* out,uint32_t capacity,const std::string& cause){
        if(out&&capacity){const auto n=std::min<size_t>(cause.size(),capacity-1);std::memcpy(out,cause.data(),n);out[n]=0;}
    }
    template<class Fn> MT2FW_MT2_Result invoke(Fn fn,char* out=nullptr,uint32_t capacity=0){
        if(!cause_.empty()){copy_cause(out,capacity,cause_);return MT2FW_MT2_FAILED;}
        std::string error;
        try{if(fn(error))return MT2FW_MT2_OK;if(error.empty())error="Recovery callback failed without a cause";}
        catch(const std::exception& cause){error=cause.what();}
        catch(...){error="Recovery callback raised an unknown exception";}
        fail(error,error);copy_cause(out,capacity,error);return MT2FW_MT2_FAILED;
    }
    static MT2FW_MT2_Result MT2FW_CALL observe_call(void* user,const MT2FW_MT2_RecoveryReceipt* receipt,MT2FW_MT2_RecoveryObservation* out){
        if(!user||!receipt||!out||out->size!=sizeof(*out))return MT2FW_MT2_INVALID;
        auto& self=*static_cast<Owner*>(user);*out={};out->size=sizeof(*out);
        return self.invoke([&](std::string& error){
            if(!self.identity(*receipt)||self.servicing_)return self.fail(error,"Recovery observation changed its bound native owner");
            net::session_wire::ProcessCohort transport;bool interrupted=false;
            if(!self.poll_(transport,error)||!self.original_.observe(transport,out->observed,interrupted,error))return false;
            out->disposition=interrupted?MT2FW_MT2_COHORT_INTERRUPTED:MT2FW_MT2_COHORT_HEALTHY;return true;
        },out->cause,sizeof(out->cause));
    }
    static MT2FW_MT2_Result MT2FW_CALL local_call(void* user,const MT2FW_MT2_StepProofQuery* query,MT2FW_MT2_StepProofReply* out){
        if(!user)return MT2FW_MT2_INVALID;auto& self=*static_cast<Owner*>(user);
        return self.proof_?ordered_session::ProofOwner::recovery_callback()(self.proof_,query,out):MT2FW_MT2_UNAVAILABLE;
    }
    static MT2FW_MT2_Result MT2FW_CALL service_call(void* user,const MT2FW_MT2_RecoveryReceipt* receipt){
        if(!user||!receipt)return MT2FW_MT2_INVALID;auto& self=*static_cast<Owner*>(user);
        return self.invoke([&](std::string& error){
            if(!self.bound_||self.servicing_||!self.identity(*receipt)||receipt->phase==MT2FW_MT2_RECOVERY_WATCHING)
                return self.fail(error,"Recovery service callback changed its fixed owner");
            self.receipt_=*receipt;self.servicing_=true;struct Clear{bool& flag;~Clear(){flag=false;}}clear{self.servicing_};
            return self.service_(self,*receipt,error);
        });
    }
    static MT2FW_MT2_Result MT2FW_CALL witness_call(void* user,const MT2FW_MT2_RecoveryReceipt* receipt,unsigned char* hash,char* cause,uint32_t capacity){
        if(!user||!receipt||!hash)return MT2FW_MT2_INVALID;auto& self=*static_cast<Owner*>(user);
        return self.invoke([&](std::string& error){
            if(!self.servicing_||!self.identity(*receipt))return self.fail(error,"Recovery witness is outside its native service");
            std::array<uint8_t,32> actual{};if(!self.witness_(*receipt,actual,error))return false;
            if(std::all_of(actual.begin(),actual.end(),[](auto byte){return byte==0;}))return self.fail(error,"Recovery covered witness returned an empty digest");
            std::copy(actual.begin(),actual.end(),hash);return true;
        },cause,capacity);
    }
    static MT2FW_MT2_Result MT2FW_CALL publication_call(void* user,const MT2FW_MT2_RecoveryReceipt* receipt,const MT2FW_MT2_RecoveryPublication* value,char* cause,uint32_t capacity){
        if(!user||!receipt||!value)return MT2FW_MT2_INVALID;auto& self=*static_cast<Owner*>(user);
        return self.invoke([&](std::string& error){
            if(!self.servicing_||!self.identity(*receipt))return self.fail(error,"Recovery publication is outside its native service");
            return self.publication_(*receipt,*value,error);
        },cause,capacity);
    }
    static MT2FW_MT2_Result MT2FW_CALL capture_call(void* user,char* cause,uint32_t capacity){
        if(!user)return MT2FW_MT2_INVALID;auto& self=*static_cast<Owner*>(user);
        return self.invoke([&](std::string& error){
            if(!self.servicing_||!self.capture_consumed_||!self.capture_)return self.fail(error,"Recovery capture lacks its consumed callback");
            auto capture=std::move(self.capture_);return capture(error);
        },cause,capacity);
    }
};
}
