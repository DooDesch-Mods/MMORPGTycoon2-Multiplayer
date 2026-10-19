#include "coupling.h"
#include "capture_readback.h"
namespace mt2mp::game::live_host_state::day::economy {
namespace {bool fail(std::string& error,const std::string& cause){error=cause;return false;}}
bool Coupling::capture(const Target& actual_day,State& out,std::string& error){
    out={};if(locked())return fail(error,capture_cause_.empty()?"economy capture locked after Unknown; no replay":capture_cause_);
    Owners actual;if(!port_.bind_actual_receipt(actual_day,error)||!port_.read(actual,error))return false;
    out=actual.state;std::vector<int32_t> ids;for(const auto& row:out.subscribers)ids.push_back(row.serialized_id);
    std::vector<mt2mp::subscriber_lifecycle::additions::Retirement> retired;if(!port_.subscriber_retirements(actual,retired,error))return false;
    bool captured=false;
    if(!additions_.host(ids,actual.next_id,out.day.frame.revision,[this,&captured](int32_t id,nlohmann::json& graph,std::string& e){captured=true;return port_.capture_subscriber(id,graph,e);},out.additions,error,retired)){
        if(additions_.locked())capture_cause_=error.empty()?"Subscriber Host capture locked without original cause; no replay":error;
        return false;
    }
    if(parties_){party_frame::State party;const auto& pin=actual.history.pin;
        if(!parties_->capture(pin.context,pin.binding,out.day.frame,party,error)){locked_=true;capture_cause_=error;return false;}
        out.day.frame.party=std::move(party);captured=true;
    }
    if(captured){
        auto party=std::move(out.day.frame.party);
        Owners refreshed;
        if(!port_.read(refreshed,error)||!capture_readback(actual,refreshed,error,true)){
            locked_=true;capture_cause_="Subscriber post-capture Frame refresh rejected original="+error+"; no replay";
            out={};return fail(error,capture_cause_);
        }
        // Capture all new graphs before sampling the final authoritative poses.
        // Original C1 still consumes each graph; existing Apply adopts this State.
        auto graphs=std::move(out.additions);out=std::move(refreshed.state);out.additions=std::move(graphs);out.day.frame.party=std::move(party);
    }
    return valid(out,error);
}
bool Coupling::read_current(const Frame& lineage,State& out,std::string& error){
    out={};Target actual_day;Owners actual;
    if(!day_.actual_current(lineage,actual_day,error)||!port_.bind_actual_receipt(actual_day,error)||!port_.read(actual,error))return false;
    out=std::move(actual.state);return valid(out,error);
}
bool Coupling::before_day(const State& target,Owners& before,std::string& error){
    before={};if(locked())return fail(error,"economy before-Day admission locked; no replay");
    Target actual_day;Port::ReadScope observed(port_,before);
    if(!day_.actual_current(target.day.frame,actual_day,error)||!port_.bind_actual_receipt(actual_day,error)||!observed.read(error))return false;
    if(parties_&&(!target.day.frame.party||!target.additions.available))return fail(error,"Party Frame requires complete Subscriber birth state before mutation");
    if(target.additions.available){
        if(target.additions.next_id<before.next_id)return fail(error,"Subscriber Host nextID moved backwards before membership effects");
        if(before.state.loans.size()!=target.loans.size()||before.state.offers!=target.offers)return fail(error,"Subscriber append cannot repair foreign Loan/Offer membership");
        for(size_t i=0;i<target.loans.size();++i)if(before.state.loans[i].text!=target.loans[i].text)return fail(error,"Subscriber append cannot repair Loan identity");
        std::vector<int32_t> current,wanted;for(const auto& row:before.state.subscribers)current.push_back(row.serialized_id);for(const auto& row:target.subscribers)wanted.push_back(row.serialized_id);
        bool append_invoked=false;
        mt2mp::subscriber_lifecycle::additions::Cache::AppendBatch batch;
        if(parties_)batch=[this,&observed,&append_invoked,&target,&before](const std::vector<nlohmann::json>& graphs,nlohmann::json& receipt,std::string& e){
            append_invoked=true;observed.close();const auto& pin=before.history.pin;
            return parties_->apply(pin.context,pin.binding,target.day.frame,graphs,receipt,e);
        };
        nlohmann::json addition;const bool appended=additions_.client(current,wanted,target.additions,[this,&observed,&append_invoked](const nlohmann::json& graph,nlohmann::json& receipt,std::string& e){append_invoked=true;observed.close();return port_.append_subscriber(graph,receipt,e);},addition,error,[this,&observed,&append_invoked](int32_t id,int32_t reason,nlohmann::json& receipt,std::string& e){append_invoked=true;observed.close();return port_.retire_subscriber(id,reason,receipt,e);},std::move(batch));
        if(addition.value("attempted",false)||addition.value("unknown",false)||addition_receipt_.is_null()){addition_receipt_=addition;addition_receipt_["frame_revision"]=target.day.frame.revision;addition_receipt_["frame_committed"]=false;}
        if(!appended){locked_=addition.value("unknown",false);return false;}
        if(append_invoked&&!observed.read(error)){locked_=addition_receipt_.value("attempted",false);return false;}
        if(before.next_id!=target.additions.next_id){
            if(!addition.at("created_ids").empty()){
                locked_=true;return fail(error,"Subscriber original nextID readback differs after append actual="+std::to_string(before.next_id)+
                    " required="+std::to_string(target.additions.next_id)+" created_count="+std::to_string(addition.at("created_ids").size()));
            }
            observed.close();Owners actual;const auto counter=advance_next_id(port_,before,target,actual);
            next_id_receipt_={{"attempted",counter.attempted},{"attempted_stores",counter.attempted_stores},{"completed_stores",counter.stores},
                {"confirmed",counter.confirmed},{"unknown",counter.unknown},{"cause",counter.cause},{"before",before.next_id},
                {"required",target.additions.next_id},{"actual",actual.next_id},{"frame_revision",target.day.frame.revision},{"frame_committed",false}};
            if(!counter.confirmed){locked_=counter.unknown;return fail(error,counter.cause);}
            before=std::move(actual);if(!observed.read(error)){locked_=true;return false;}
        }
    }
    if(!observed.preflight(target,error))return false;
    if(!admit_existing(before,target,error)){locked_=addition_receipt_.is_object()&&addition_receipt_.value("attempted",false)&&!addition_receipt_.value("frame_committed",false);return false;}return true;
}
bool Coupling::after_day(const State& target,const Owners& before,std::string& error){
    if(locked())return fail(error,"economy after-Day Apply locked; no replay");
    receipt_={};
    auto unknown=[&](const std::string& cause){locked_=true;receipt_.unknown=true;receipt_.cause=cause+" original="+error+"; actual Day already committed; no replay";return fail(error,receipt_.cause);};
    Owners current;Port::ReadScope observed(port_,current);
    if(!port_.bind_actual_receipt(target.day,error)||!observed.read(error))return unknown("economy post-Day actual receipt read Unknown");
    if(current.next_id!=before.next_id||current.subscriber_manager!=before.subscriber_manager||current.subscriber_data!=before.subscriber_data||current.loan_data!=before.loan_data||current.offer_data!=before.offer_data||current.subscribers!=before.subscribers||current.subscriber_slots!=before.subscriber_slots||current.needs_owners!=before.needs_owners||current.loans!=before.loans||current.offers!=before.offers||current.state.subscribers!=before.state.subscribers||current.state.loans!=before.state.loans||current.state.offers!=before.state.offers){
        error="actual Day C1/Append unexpectedly changed economic owners/prior values";return unknown("economy post-Day prior owner Unknown");
    }
    auto scalars=target;scalars.additions={};receipt_=apply_.run_observed(observed,scalars);
    if(!receipt_.confirmed){error=receipt_.cause;return unknown("economy native scalar/readback Unknown");}
    error.clear();return true;
}
bool Coupling::rebind_snapshot(std::string& error){
    if(locked())return fail(error,"economy snapshot rebind rejects retained Unknown");
    // Existing Bridge rebinds the actual Day provider first. No old-world read
    // occurs here; the first new Frame binds a fresh verified native receipt.
    apply_=Apply{};receipt_={};additions_.clear();capture_cause_.clear();addition_receipt_=next_id_receipt_=nullptr;error.clear();return true;
}
}
