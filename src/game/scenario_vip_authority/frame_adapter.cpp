#include "frame_adapter.h"
#include "host_witness.h"
#include "../native_open_runtime.h"
#include "mt2mp/core/logger.h"

namespace mt2mp::game::scenario_vip_authority {
namespace {
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
nlohmann::json encode(const client_alert::Call& call){
    return {{"attempted",call.attempted},{"returned",call.returned},{"exception",call.exception}};
}
nlohmann::json encode(const client_alert::Receipt& r){
    const auto& t=r.transition;
    return {{"sequence",t.sequence},{"arrival_sequence",t.arrival},{"subscriber_id",t.subscriber},
        {"name",t.name},{"active",t.active},{"attempted",r.attempted},{"confirmed",r.confirmed},
        {"unknown",r.unknown},{"duplicate",r.duplicate},{"shown",r.shown},
        {"expired_before_presentation",r.expired_before_presentation},{"cause",r.cause},
        {"create",encode(r.create)},{"show",encode(r.show)},{"hide",encode(r.hide)},
        {"destroy",encode(r.destroy)},
        {"message",{{"text",t.message?nlohmann::json(*t.message):nlohmann::json(nullptr)},
            {"verified",r.message.verified},{"handed_off",r.message.handed_off},
            {"create",encode(r.message.create)},{"fire",encode(r.message.fire)},{"discard",encode(r.message.discard)},
            {"loc_construct",encode(r.message.loc_construct)},{"loc_arguments_destroy",encode(r.message.loc_arguments_destroy)},
            {"loc_string_destroy",encode(r.message.loc_string_destroy)}}}};
}
}
bool FrameAdapter::bind(const Context& c,const Binding& b,uint64_t epoch,std::string& error){
    const bool empty_client_rebind=bound_&&suspended_&&context_.role==action_session::Role::Client&&
        !origin_&&!lifecycle_.owned()&&!lifecycle_.unknown();
    if((bound_&&!empty_client_rebind)||!epoch||!authorize_)
        return fail(error,"VIP Frame adapter bind requires fresh owner/epoch/authorization");
    const Origin origin{c.session,epoch,b.native_generation,c.snapshot_uuid};
    if(!live_host_state::vip::valid_origin(origin))return fail(error,"VIP Frame adapter bind Origin invalid");
    if(c.role==action_session::Role::Host&&!host_bind(c,b,epoch,error))return false;
    context_=c;binding_=b;epoch_=epoch;bound_=true;suspended_=false;cleanup_cause_.clear();
    if(c.role==action_session::Role::Host)origin_=origin;
    else origin_.reset();
    error.clear();return true;
}
bool FrameAdapter::host_capture(const Origin& origin,Batch& batch,std::string& error){
    if(!bound_||suspended_||context_.role!=action_session::Role::Host||origin_!=origin)
        return fail(error,"VIP Host capture adapter Origin/role differs");
    return scenario_vip_authority::host_capture(origin,batch,error);
}
bool FrameAdapter::host_sent(const Origin& origin,uint64_t revision,const Batch& batch,std::string& error){
    if(!bound_||suspended_||origin_!=origin)return fail(error,"VIP sent adapter Origin differs");
    return scenario_vip_authority::host_sent(origin,revision,batch,error);
}
bool FrameAdapter::host_acknowledge(const Origin& origin,uint32_t peer,uint64_t revision,std::string& error){
    if(!bound_||suspended_||origin_!=origin)return fail(error,"VIP ACK adapter Origin differs");
    return scenario_vip_authority::host_acknowledge(origin,peer,revision,error);
}
bool FrameAdapter::client_origin(const Origin& origin,std::string& error){
    if(!bound_||suspended_||context_.role!=action_session::Role::Client||
       origin.session!=context_.session||origin.snapshot!=context_.snapshot_uuid||origin.epoch!=epoch_)
        return fail(error,"VIP Client adapter current Origin/role differs");
    if(origin_){if(*origin_!=origin)return fail(error,"VIP Client adapter source Origin changed without suspension");return true;}
    if(!lifecycle_.bind(origin,error))return false;
    native_=std::make_unique<client_alert::NativePort>(origin,context_,binding_,authorize_);
    origin_=origin;error.clear();return true;
}
bool FrameAdapter::client_preflight(const Origin& origin,const std::vector<Transition>& work,std::string& error){
    return client_origin(origin,error)&&lifecycle_.preflight(*native_,origin,work,error);
}
bool FrameAdapter::client_apply(const Origin& origin,const std::vector<Transition>& work,
                               live_host_state::VipApplyReceipt& receipt,std::string& error){
    receipt={};
    if(!client_origin(origin,error)){receipt.cause=error;return false;}
    const auto retain=[&]{
        const auto& result=lifecycle_.result();receipt={};
        receipt.attempted=result.attempted;receipt.unknown=result.unknown||lifecycle_.unknown();
        receipt.confirmed=result.confirmed;receipt.cause=result.cause;
        for(const auto& row:result.rows)if(row.confirmed)receipt.applied.push_back(row.transition);
    };
    try{
        const bool applied=lifecycle_.apply(*native_,origin,work,error);
        retain();
        receipt.confirmed=applied&&receipt.confirmed&&!receipt.unknown&&receipt.applied==work;
        if(!receipt.confirmed){receipt.cause=error.empty()?"VIP native exact planned rows not confirmed":error;return fail(error,receipt.cause);}
        error.clear();return true;
    }catch(...){
        retain();receipt.unknown=receipt.unknown||receipt.attempted;
        receipt.cause=lifecycle_.cause().empty()?"VIP Frame adapter native apply threw":lifecycle_.cause();
        throw;
    }
}
bool FrameAdapter::suspend(const Context& c,const Binding& b,uint64_t epoch,
                          const std::optional<Origin>& origin,std::string& error){
    if(!bound_||suspended_||c!=context_||b!=binding_||epoch!=epoch_||(origin_&&origin!=origin_))
        return fail(error,"VIP suspension adapter current owner differs");
    if(!origin_&&origin){
        if(c.role!=action_session::Role::Client||!live_host_state::vip::valid_origin(*origin)||
           origin->session!=c.session||origin->snapshot!=c.snapshot_uuid||origin->epoch!=epoch)
            return fail(error,"VIP suspension admitted Client Origin differs");
        // The Bridge may have admitted a queued Frame before native UI work.
        origin_=origin;
    }
    // Stop new UI work even if retiring the existing allocation fails.
    suspended_=true;
    if(c.role==action_session::Role::Host)return host_suspend(*origin_,error);
    if(lifecycle_.unknown())return fail(error,lifecycle_.cause());
    if(!lifecycle_.owned()){error.clear();return true;}
    building_mixed_identity::opened_file::CurrentWorld world;
    if(!native_open_runtime::read_current_world(world,error))return false;
    if(world.world_epoch!=b.native_epoch||world.generation!=b.native_generation)
        return fail(error,"VIP suspension native World differs from local Binding");
    if(!native_||!lifecycle_.cleanup(*native_,uintptr_t(world.mode),error)){
        cleanup_cause_=error.empty()?"VIP native cleanup failed without cause":error;
        LOG_ERROR("[VIPPresentation] {}",cleanup_cause_);return fail(error,cleanup_cause_);
    }
    error.clear();return true;
}
bool FrameAdapter::reset(const Origin& origin,std::string& error){
    if(!bound_||!suspended_||origin_!=origin)return fail(error,"VIP adapter reset requires exact suspended Origin");
    if(context_.role==action_session::Role::Host){if(!host_reset(origin,error))return false;}
    else if(lifecycle_.owned()||lifecycle_.unknown())return fail(error,"VIP adapter reset requires confirmed empty native ownership");
    native_.reset();origin_.reset();bound_=false;error.clear();return true;
}
bool FrameAdapter::before_deinit(uintptr_t mode,std::string& error){
    if(!bound_){error.clear();return true;}
    if(context_.role==action_session::Role::Host){
        if(suspended_){error.clear();return true;}
        suspended_=true;return host_suspend(*origin_,error);
    }
    suspended_=true;
    if(lifecycle_.unknown())return fail(error,lifecycle_.cause());
    if(!lifecycle_.owned()){error.clear();return true;}
    if(!native_)return fail(error,"VIP Mode cleanup native Port absent");
    return lifecycle_.cleanup(*native_,mode,error);
}
void FrameAdapter::after_deinit(uintptr_t mode,bool abnormal){
    if(!bound_)return;
    if(context_.role==action_session::Role::Client)lifecycle_.after_deinit(mode);
    if(abnormal||lifecycle_.unknown()){
        cleanup_cause_=abnormal?"VIP native Mode Deinit did not return normally":lifecycle_.cause();
        LOG_ERROR("[VIPPresentation] {}",cleanup_cause_);
    }
}
void FrameAdapter::retire_lobby(){
    if(!bound_)return;
    std::string error;
    if(context_.role==action_session::Role::Host&&origin_&&!host_reset(*origin_,error))
        LOG_ERROR("[VIPPresentation] Host observer retirement failed: {}",error);
    if(lifecycle_.owned()||lifecycle_.unknown())
        LOG_ERROR("[VIPPresentation] World ended with unconfirmed alert cleanup owned={} cause={}; no late destructor",lifecycle_.owned(),lifecycle_.cause());
    native_.reset();bound_=false;
}
nlohmann::json FrameAdapter::status()const{
    auto rows=nlohmann::json::array();
    for(const auto& [sequence,receipt]:lifecycle_.receipts()){(void)sequence;rows.push_back(encode(receipt));}
    return {{"bound",bound_},{"suspended",suspended_},{"owned",lifecycle_.owned()},
        {"unknown",lifecycle_.unknown()},{"cause",lifecycle_.cause()},{"cleanup_cause",cleanup_cause_},
        {"last",encode(lifecycle_.last())},{"receipts",std::move(rows)}};
}
}
