#pragma once
#include "action_session.h"
#include <thread>

namespace mt2mp::action_session::tick_commands {
struct OriginalGoBoundary {
    uint64_t lease=0,session=0,world_epoch=0,tick=0,invocation=0;
    uint32_t game_thread=0;
    bool operator==(const OriginalGoBoundary&) const = default;
};
struct OriginalGoObservation {
    OriginalGoBoundary boundary;
    Context context;
    Binding binding;
    bool ordered=false,started=false,locked=false,retiring=false,state_copy=false;
    bool original_running=false;
};
// The runtime owner reads the current Framework lease and its real Controller.
// A matching token alone does not authorize held admission or a UI-only frame.
class OriginalGoScope {
public:
    using Read=std::function<bool(OriginalGoObservation&,std::string&)>;
    using Next=std::function<Outcome(std::string&)>;
    OriginalGoScope():thread_(std::this_thread::get_id()){}
    OriginalGoScope(const OriginalGoScope&)=delete;
    OriginalGoScope& operator=(const OriginalGoScope&)=delete;
    bool admission(bool& admitted,std::string& cause) {
        admitted=false;
        if(std::this_thread::get_id()!=thread_)return fail(cause,"OriginalGo admission is not on the bound game thread");
        if(!claim_){cause.clear();return true;}
        if(!fresh(*claim_,true,cause)){if(claim_->failure.empty())claim_->failure=cause;return false;}
        admitted=true;cause.clear();return true;
    }
    Outcome run(Session& session,Engine& engine,const Context& context,const Binding& binding,
                const OriginalGoBoundary& boundary,const Read& read,const Next& next,std::string& cause) {
        if(std::this_thread::get_id()!=thread_||claim_||!read||!next){
            cause="OriginalGo scope requires its owner thread, no nested invocation, and both callbacks";
            return Outcome::RejectedNoEffect;
        }
        if(!boundary.lease||!boundary.session||!boundary.world_epoch||!boundary.tick||
           !boundary.invocation||!boundary.game_thread||boundary.session!=context.session||
           boundary.world_epoch!=binding.native_epoch){cause="OriginalGo boundary identity is incomplete or differs";return Outcome::RejectedNoEffect;}
        Claim claim{session,engine,context,binding,boundary,read,{}};
        if(!fresh(claim,false,cause))return Outcome::RejectedNoEffect;
        claim_=&claim;
        struct Clear {OriginalGoScope& scope;~Clear(){scope.claim_=nullptr;}} clear{*this};
        Outcome outcome=Outcome::Unknown;
        try {outcome=next(cause);}
        catch(const std::exception& e){cause=std::string("OriginalGo continuation exception: ")+e.what();return Outcome::Unknown;}
        catch(...){cause="OriginalGo continuation nonstandard exception";return Outcome::Unknown;}
        if(!claim.failure.empty()){cause=claim.failure;return Outcome::Unknown;}
        if(outcome==Outcome::Confirmed&&!fresh(claim,false,cause))return Outcome::Unknown;
        if(outcome!=Outcome::Confirmed&&cause.empty())cause="OriginalGo continuation failed without original cause";
        return outcome;
    }
private:
    struct Claim {Session& session;Engine& engine;Context context;Binding binding;OriginalGoBoundary boundary;Read read;std::string failure;};
    const std::thread::id thread_;
    Claim* claim_=nullptr;
    static bool fail(std::string& cause,const char* text){cause=text;return false;}
    static bool fresh(Claim& claim,bool running,std::string& cause) {
        OriginalGoObservation value;Context actual;
        if(!claim.read(value,cause)||!claim.session.current(actual,cause)){
            if(cause.empty())cause="OriginalGo owner or Session read failed without original cause";return false;
        }
        if(value.boundary!=claim.boundary||value.context!=claim.context||actual!=claim.context||value.binding!=claim.binding)
            return fail(cause,"OriginalGo current lease, tick, Session, or native Binding changed");
        if(!value.ordered||!value.started||value.locked||value.retiring||value.state_copy)
            return fail(cause,"OriginalGo requires the exclusive active ordered Controller without state-copy ownership");
        if(value.original_running!=running)
            return fail(cause,running?"OriginalGo admission is outside the current native continuation":"OriginalGo boundary is not held before or after continuation");
        if(!claim.engine.current(claim.binding,claim.context.snapshot_uuid,cause)){
            if(cause.empty())cause="OriginalGo native world proof failed without original cause";return false;
        }
        return true;
    }
};
}
