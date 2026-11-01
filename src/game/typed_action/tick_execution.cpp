#include "tick_execution.h"
#include "place_trace.h"
#include <cmath>
#include <limits>

namespace mt2mp::action_session::tick_commands {
Executor::Executor(Session& session,Engine& engine,Context context,Binding binding,ClockPort clock,AdmitExecution admit,uint64_t order,PrepareSnapshot prepare)
    :session_(session),engine_(engine),context_(std::move(context)),binding_(binding),clock_(std::move(clock)),
     admit_(std::move(admit)),prepare_snapshot_(std::move(prepare)),thread_(std::this_thread::get_id()),completed_(order) {}
bool Executor::current(std::string& cause) {
    Context actual;
    if(!session_.current(actual,cause))return false;
    if(actual!=context_){cause="tick execution session or loaded snapshot changed";return false;}
    return engine_.current(binding_,context_.snapshot_uuid,cause);
}
Outcome Executor::gameplay(const Request& request,ExecutionReceipt& receipt,std::string& cause) {
    const auto action=request.action==Action::PlaceBuilding?request.action_uuid:std::string{};
    Ledger before;
    if(!engine_.ledger(binding_,before,cause))return Outcome::RejectedNoEffect;
    if(before.record_present<0||before.record_present>1){cause="tick execution ledger record_present requires 0 or 1";return Outcome::RejectedNoEffect;}
    receipt.native_attempted=true;
    const auto outcome=place_trace::run(action,"engine_execute",cause,[&]{return engine_.execute_action(binding_,request,cause);});
    if(outcome!=Outcome::Confirmed)return outcome;
    auto evidence=std::make_shared<Evidence>();
    if(!place_trace::run(action,"observe_complete",cause,[&]{return observe_completed(engine_,binding_,request,before,*evidence,cause);}))return Outcome::Unknown;
    receipt.evidence=std::move(evidence);
    return Outcome::Confirmed;
}
Outcome Executor::time(const TimeRequest& request,ExecutionReceipt& receipt,std::string& cause) {
    using game::time_session::Intent;
    if(!clock_.read||!clock_.set){cause="tick execution native clock callbacks missing";return Outcome::RejectedNoEffect;}
    game::time_session::Clock before;
    if(!clock_.read(before,cause))return Outcome::RejectedNoEffect;
    if(!std::isfinite(before.selected)||before.selected<0||!std::isfinite(before.pause)||before.pause<0){cause="tick execution native clock outside domain";return Outcome::RejectedNoEffect;}
    receipt.clock_before=before;
    float target=0;
    switch(request.intent) {
    case Intent::Pause:target=0;break;
    case Intent::Normal:target=1;break;
    case Intent::Fast:target=3;break;
    case Intent::Fastest:target=9;break;
    case Intent::TogglePauseNormal:target=before.selected*before.pause==0?1.0f:0.0f;break;
    default:cause="tick execution unknown clock intent";return Outcome::RejectedNoEffect;
    }
    if(target!=before.selected){receipt.native_attempted=true;if(!clock_.set(target,cause))return Outcome::Unknown;}
    game::time_session::Clock after;
    if(!clock_.read(after,cause))return receipt.native_attempted?Outcome::Unknown:Outcome::RejectedNoEffect;
    receipt.clock_after=after;
    if(after.selected!=target||after.pause!=before.pause){cause="tick execution native clock readback differs";return receipt.native_attempted?Outcome::Unknown:Outcome::RejectedNoEffect;}
    return Outcome::Confirmed;
}
Outcome Executor::execute(const Assigned& command,ExecutionReceipt& out,std::string& cause) {
    if(std::this_thread::get_id()!=thread_||busy_){cause="tick execution requires non-reentrant owner thread";return Outcome::RejectedNoEffect;}
    if(last_&&last_->command==command){out=*last_;cause=out.cause;return out.outcome;}
    if(locked_){cause=failure_;return Outcome::RejectedNoEffect;}
    if(completed_==std::numeric_limits<uint64_t>::max()||command.order!=completed_+1){cause="tick execution command order differs";return Outcome::RejectedNoEffect;}
    if(command.input.session!=context_.session||command.input.snapshot_uuid!=context_.snapshot_uuid){cause="tick execution command session or snapshot differs";return Outcome::RejectedNoEffect;}
    busy_=true;
    struct Clear {bool& value;~Clear(){value=false;}} clear{busy_};
    last_.emplace();last_->command=command;
    auto& receipt=*last_;
    cause.clear();
    try {
        if(!admit_){cause="tick execution deterministic native admission missing";receipt.outcome=Outcome::RejectedNoEffect;}
        else if(!current(cause)||!admit_(context_,binding_,command,cause)||!current(cause))receipt.outcome=Outcome::RejectedNoEffect;
        else {
            receipt.outcome=std::holds_alternative<Request>(command.input.command)
                ?gameplay(std::get<Request>(command.input.command),receipt,cause)
                :std::holds_alternative<TimeRequest>(command.input.command)?time(std::get<TimeRequest>(command.input.command),receipt,cause)
                :prepare_snapshot_?(receipt.native_attempted=true,prepare_snapshot_(std::get<SnapshotRequest>(command.input.command),cause)?Outcome::Confirmed:Outcome::Unknown)
                :(cause="Snapshot preparation callback missing",Outcome::RejectedNoEffect);
            if(receipt.outcome==Outcome::Confirmed&&!current(cause))receipt.outcome=Outcome::Unknown;
        }
    } catch(const std::exception& e){cause=std::string("tick execution exception: ")+e.what();receipt.outcome=receipt.native_attempted?Outcome::Unknown:Outcome::RejectedNoEffect;}
    catch(...){cause="tick execution nonstandard exception";receipt.outcome=receipt.native_attempted?Outcome::Unknown:Outcome::RejectedNoEffect;}
    if(receipt.outcome!=Outcome::Confirmed){
        if(cause.empty())cause="tick execution failed without original cause";
        locked_=true;failure_=cause;
    } else ++completed_;
    receipt.cause=cause;out=receipt;return receipt.outcome;
}
}
