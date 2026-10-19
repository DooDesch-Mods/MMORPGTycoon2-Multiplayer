#pragma once
#include "mt2mp/hooks/private/native_release.h"
#include <cstring>
#include <functional>
#include <algorithm>
#include <string>
#include <stdexcept>

namespace mt2mp::game::release_boundary {
// This consumer owns metadata only. Framework retains the one physical Mode hook.
class Owner {
public:
    using Capture=std::function<bool(const MT2FW_MT2_ReleaseCall&,uint32_t&,std::string&)>;
private:
    MT2FW_MT2_ReleaseV1 api_{};
    Capture capture_;
    uint64_t registration_=0,thread_=0,last_invocation_=0;
    bool failed_=false;std::string cause_;
    static bool fail(std::string& e,const std::string& cause){e=cause;return false;}
    static void decision(MT2FW_MT2_ReleaseDecision& out,uint32_t kind,const std::string& e){
        out={};out.size=sizeof out;out.disposition=kind;const auto count=std::min(e.size(),sizeof(out.cause)-1);std::memcpy(out.cause,e.data(),count);
    }
    static MT2FW_MT2_Result MT2FW_CALL entry(void* user,const MT2FW_MT2_ReleaseCall* call,MT2FW_MT2_ReleaseDecision* out){
        auto& self=*static_cast<Owner*>(user);if(!out)return MT2FW_MT2_INVALID;
        std::string e;uint32_t disposition=MT2FW_MT2_RELEASE_FAILED;
        try{
            if(!call||call->size!=sizeof(*call)||!call->readable||call->read_error||call->reserved||!call->invocation||
               call->registration!=self.registration_||call->thread!=self.thread_||!call->receiver||!call->native_command||
               !call->native_return||!call->text||call->length!=7||std::memcmp(call->text,"LevelUp",7)||call->invocation<=self.last_invocation_)
                e="Release boundary exact ABI/owner/thread/native command/invocation differs";
            else if(self.failed_)e=self.cause_;
            else if(!self.current(e,call->invocation)){}
            else{self.last_invocation_=call->invocation;if(!self.capture_(*call,disposition,e)&&e.empty())e="Release boundary consumer rejected without cause";}
        }catch(const std::exception& ex){e="Release boundary consumer C++: "+std::string(ex.what());}
        catch(...){e="Release boundary consumer unknown C++ exception";}
        if(!e.empty()||(disposition!=MT2FW_MT2_RELEASE_FORWARD&&disposition!=MT2FW_MT2_RELEASE_SUPPRESS)){
            self.failed_=true;if(self.cause_.empty())self.cause_=e.empty()?"Release boundary consumer disposition invalid":e;
            decision(*out,MT2FW_MT2_RELEASE_FAILED,self.cause_);
        }else decision(*out,disposition,{});
        return MT2FW_MT2_OK;
    }
public:
    bool bind(const MT2FW_MT2_ReleaseV1& api,uint64_t thread,Capture capture,std::string& e){
        if(registration_||!thread||!capture||api.size!=sizeof api||api.version!=MT2FW_MT2_RELEASE_VERSION||!api.context||!api.bind||!api.unbind||!api.read)
            return fail(e,"Release boundary requires unbound exact private V1 API/Mainthread/consumer");
        api_=api;thread_=thread;capture_=std::move(capture);MT2FW_MT2_ReleaseReceipt receipt{};receipt.size=sizeof receipt;
        const auto result=api_.bind(api_.context,&entry,this,&receipt);
        if(result!=MT2FW_MT2_OK||receipt.size!=sizeof receipt||!receipt.bound||receipt.failed||!receipt.registration||std::strcmp(receipt.owner,"doodesch.multiplayer"))
            return fail(e,"Release boundary named bind failed result="+std::to_string(result)+" cause="+std::string(receipt.cause));
        registration_=receipt.registration;last_invocation_=0;failed_=false;cause_.clear();e.clear();return true;
    }
    bool current(std::string& e,uint64_t invocation=0)const{
        if(!registration_||!api_.read)return fail(e,"Release boundary metadata is unbound");
        MT2FW_MT2_ReleaseReceipt receipt{};receipt.size=sizeof receipt;const auto result=api_.read(api_.context,&receipt);
        if(result!=MT2FW_MT2_OK||receipt.size!=sizeof receipt||!receipt.bound||receipt.registration!=registration_||receipt.failed||std::strcmp(receipt.owner,"doodesch.multiplayer")||(invocation&&(receipt.last_invocation!=invocation||receipt.inflight!=1)))
            return fail(e,"Release boundary actual registration/owner differs result="+std::to_string(result)+" cause="+std::string(receipt.cause));
        e.clear();return true;
    }
    bool unbind(std::string& e){
        if(!registration_){e.clear();return true;}MT2FW_MT2_ReleaseReceipt receipt{};receipt.size=sizeof receipt;const auto result=api_.unbind(api_.context,registration_,&receipt);
        if(result!=MT2FW_MT2_OK||receipt.size!=sizeof receipt||receipt.bound||receipt.inflight||receipt.registration)
            return fail(e,"Release boundary metadata retirement failed result="+std::to_string(result)+" cause="+std::string(receipt.cause));
        api_={};capture_={};registration_=thread_=last_invocation_=0;failed_=false;cause_.clear();e.clear();return true;
    }
    uint64_t registration()const{return registration_;}
    bool failed()const{return failed_;}
    const std::string& cause()const{return cause_;}
};
}
