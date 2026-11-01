#include "tick_authority.h"
namespace mt2mp::action_session::tick_commands::authority {
namespace {
struct Claim {Queue& queue;Session& session;Engine& engine;Binding binding;const Assigned& command;Context context;uint64_t tick=0;};
thread_local Claim* claim=nullptr;
bool fresh(Claim& value,std::string& cause) {
    Context admitted,actual;uint64_t tick=0;
    if(!value.queue.current_execution(value.command,admitted,tick,cause)||!value.session.current(actual,cause)){
        if(cause.empty())cause="Ordered native session admission failed without original cause";return false;
    }
    if(admitted!=value.context||actual!=value.context||tick!=value.tick){cause="Ordered native execution session or tick changed";return false;}
    if(!value.engine.current(value.binding,value.context.snapshot_uuid,cause)){
        if(cause.empty())cause="Ordered native world admission failed without original cause";return false;
    }
    return true;
}
}
bool active(){return claim!=nullptr;}
bool current_time(TimeRequest& out,std::string& cause) {
    if(!claim){cause="Ordered clock has no current admitted command";return false;}
    const auto* expected=std::get_if<TimeRequest>(&claim->command.input.command);
    if(!expected){cause="Ordered clock requires the admitted time command";return false;}
    if(!fresh(*claim,cause))return false;
    out=*expected;return true;
}
bool current_snapshot(SnapshotRequest& out,std::string& cause) {
    if(!claim){cause="Ordered snapshot has no current admitted command";return false;}
    const auto* expected=std::get_if<SnapshotRequest>(&claim->command.input.command);
    if(!expected){cause="Ordered snapshot requires the admitted snapshot command";return false;}
    if(!fresh(*claim,cause))return false;
    out=*expected;return true;
}
bool matches(const Context& context,const Binding& binding,const Request& request,std::string& cause) {
    if(!claim){cause="Ordered native execution has no current admitted command";return false;}
    const auto* expected=std::get_if<Request>(&claim->command.input.command);
    if(context!=claim->context||binding!=claim->binding||!expected||*expected!=request){cause="Ordered native execution request, role, or world binding differs";return false;}
    return fresh(*claim,cause);
}
bool matches(const Binding& binding,const Request& request,std::string& cause) {
    if(!claim){cause="Ordered native execution has no current admitted command";return false;}
    return matches(claim->context,binding,request,cause);
}
Outcome run(Queue& queue,Session& session,Engine& engine,const Binding& binding,const Assigned& command,
            const std::function<Outcome(std::string&)>& operation,std::string& cause) {
    if(claim||!operation){cause="Ordered native execution scope is nested or callback missing";return Outcome::RejectedNoEffect;}
    Context context;uint64_t tick=0;
    if(!queue.current_execution(command,context,tick,cause))return Outcome::RejectedNoEffect;
    Claim value{queue,session,engine,binding,command,context,tick};
    if(!fresh(value,cause))return Outcome::RejectedNoEffect;
    claim=&value;struct Clear{~Clear(){claim=nullptr;}}clear;
    Outcome outcome=Outcome::Unknown;
    try{outcome=operation(cause);}
    catch(const std::exception& e){cause=std::string("Ordered native execution exception: ")+e.what();return Outcome::Unknown;}
    catch(...){cause="Ordered native execution nonstandard exception";return Outcome::Unknown;}
    if(outcome==Outcome::Confirmed&&!fresh(value,cause))return Outcome::Unknown;
    if(outcome!=Outcome::Confirmed&&cause.empty())cause="Ordered native execution failed without original cause";
    return outcome;
}
}
