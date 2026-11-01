#pragma once
#include "typed_action/action_session.h"
#include "hud_time_reader.h"
#include "shared_time.h"
#include "time_session.h"
#include <functional>
#include <optional>

namespace mt2mp::game::ordered_time_input {
struct Current {
    bool ordered=false,pending=false,original_go=false;
    action_session::Context context;
    action_session::Binding binding;
    bool operator==(const Current&)const=default;
};
class Capture {
public:
    using Read=std::function<bool(Current&,std::string&)>;
    using Classify=std::function<bool(uintptr_t,uintptr_t,bool,hud_time::ActivationBinding&,std::string&)>;
    using Submit=std::function<bool(const action_session::Context&,const action_session::Binding&,time_session::Intent,std::string&)>;
private:
    struct Pending {action_session::Context context;action_session::Binding binding;time_session::Intent intent;};
    std::optional<Pending> pending_;
    bool busy_=false;
    struct Busy {bool& flag;explicit Busy(bool& value):flag(value){flag=true;}~Busy(){flag=false;}};
public:
    bool pending()const{return pending_.has_value();}
    shared_time::HookResult activate(uintptr_t type,uintptr_t item,uintptr_t caller,const Read& read,const Classify& classify,bool& handled){
        shared_time::HookResult result;handled=false;Current before;std::string error;
        const auto reject=[&](const std::string& cause){result.error=cause.empty()?"Ordered time capture rejected without cause":cause;return result;};
        try{
            const bool current=read(before,error);handled=before.ordered;
            if(handled)result.decision=shared_time::Decision::Suppress;
            if(!current)return reject(error);
            if(!handled)return result;
            if(busy_)return reject("Ordered time activation reentered");Busy busy(busy_);
            if(pending_)return reject("Ordered time input already retained; new input rejected");
            const bool mouse=caller==0x58095c;
            hud_time::ActivationBinding binding;if(!classify(type,item,mouse,binding,error))return reject(error);
            if(!binding.matched)return reject("Ordered time activation is not an owned native item/type pair");
            result.matched=true;result.origin=mouse?shared_time::Origin::Mouse:shared_time::Origin::Keyboard;
            const std::array<float,4> scales{0,1,3,9};size_t index=0;while(index<scales.size()&&binding.scale!=scales[index])++index;
            if(index==scales.size())return reject("Ordered time native scale is unsupported");
            auto intent=static_cast<time_session::Intent>(index);
            if(mouse){if(!binding.visible||!binding.enabled)return reject("Ordered time button is unavailable");}
            else if(caller==0x4a2a77||caller==0x4a2d93){
                if(index!=(caller==0x4a2a77?0u:1u))return reject("Ordered pause-toggle caller/item differs");
                intent=time_session::Intent::TogglePauseNormal;
            }else{
                constexpr std::array<uintptr_t,4> direct{0x4a2abe,0x4a2b05,0x4a2b4c,0x4a2b97};
                if(caller!=direct[index])return reject("Ordered time keyboard caller/item differs");
            }
            result.origin_valid=true;result.intent=static_cast<shared_time::Intent>(intent);
            hud_time::ActivationBinding final;Current after;
            if(!classify(type,item,mouse,final,error)||final!=binding||!read(after,error)||after!=before)
                return reject(error.empty()?"Ordered time native owner or Session changed during capture":error);
            pending_=Pending{after.context,after.binding,intent};result.request_queued=true;return result;
        }catch(const std::exception& problem){return reject(std::string("Ordered time capture exception: ")+problem.what());}
        catch(...){return reject("Ordered time capture unknown exception");}
    }
    bool flush(const Read& read,const Submit& submit,std::string& error){
        if(!pending_){error.clear();return true;}
        if(busy_){error="Ordered time flush reentered";return false;}Busy busy(busy_);
        try{
            Current current;
            if(!read(current,error)||!current.ordered||current.context!=pending_->context||current.binding!=pending_->binding){
                pending_.reset();if(error.empty())error="Ordered time retained Session/native binding changed; input cancelled";return false;
            }
            if(current.original_go||current.pending){error.clear();return true;}
            const auto input=*pending_;pending_.reset();
            if(!submit(input.context,input.binding,input.intent,error)){if(error.empty())error="Ordered time submit failed without cause; no replay";return false;}
            error.clear();return true;
        }catch(const std::exception& problem){pending_.reset();error=std::string("Ordered time flush exception; no replay: ")+problem.what();return false;}
        catch(...){pending_.reset();error="Ordered time flush unknown exception; no replay";return false;}
    }
};
}
