#include "tutorial_replication/native.h"
#include "tutorial_replication/start_ledger.h"
#include "tutorial_replication/staff_action.h"
#include <iostream>
#include <map>
#include <stdexcept>
namespace action=mt2mp::action_session;
namespace tutorial_replication=mt2mp::game::tutorial_replication;
namespace bug_authority=mt2mp::game::bug_authority;
namespace research=tutorial_replication::research;
namespace fixture {
unsigned checks=0;
void need(bool ok,const std::string& cause){++checks;if(!ok)throw std::runtime_error(cause);}
const std::string id="11111111-1111-4111-8111-111111111111",snapshot="22222222-2222-4222-8222-222222222222";
const action::Binding fixture_binding{3,7};
action::Ledger money(){action::Ledger r;r.cash=r.record_cash=88160;r.record_present=1;r.flat_day=r.record_day=2;r.record_expense=120;r.line_counts[3]=4;r.line_income[3]=-120;r.line_counts[19]=9;r.line_income[19]=77;return r;}
research::Row initial(bool current=false){research::Row r{"bridges",{}};r.value.release_change_count=4;r.value.release_counter=7;if(current){r.value.project_present=r.value.purchased=true;r.value.work=21600;r.value.lines=36;r.value.desired_workers=1;r.value.bugs_total=5;r.value.bugs_known=3;}return r;}
research::Row deployed(const research::Row& before){auto r=before;r.value={};r.value.purchased=r.value.deployed=true;r.value.inventory_count=r.value.release_feature_count=1;r.value.release_change_count=before.value.release_change_count+1;r.value.release_counter=before.value.release_counter+1;r.value.commit_bugs=tutorial_replication::startingpoint::CommitBugs{before.value.bugs_total,before.value.bugs_known};return r;}
bug_authority::State bugs(bool after){bug_authority::State s;bug_authority::Row old;old.parent_key="snapshot:retained-building";old.value.created=1;old.value.type=1;s.rows.push_back(old);if(after){auto added=old;added.parent_key="snapshot:research-building";added.value.created=2;added.value.known=true;added.value.discovered=2;s.rows.push_back(added);}return s;}
struct State {
    action::Context context{5,1,1,3,action::Role::Host,snapshot};
    action::Ledger ledger=money();research::Row row=initial();
    bool current_ok=true,session_ok=true,reject=false,partial=false,uncertain=false,retain_ok=true,wrong_ledger=false,wrong_project=false,wrong_stamp=false;
    int fail_ledger=0,fail_row=0,fail_capture=0,ledger_reads=0,row_reads=0,captures=0,mutations=0,retentions=0,host_starts=0,client_starts=0,worker_calls=0,host_commits=0,client_commits=0;
    int32_t price=10000;std::vector<uint64_t> revisions;std::vector<std::string> events;
    std::optional<bug_authority::State> retained_before,retained_after;
} state;
void reset(action::Action action,bool client=false){state={};state.row=initial(action!=action::Action::StartResearch);state.context.role=client?action::Role::Client:action::Role::Host;state.context.local_peer=client?2:1;}
void bound(const action::Context& c,const action::Binding& b){need(c==state.context&&b==fixture_binding,"native API received foreign context/binding");}
bool mutate(action::Action action,int32_t desired,bool& attempted,bool* unknown,std::string& e){
    if(state.reject){e="original Research eligibility rejected";return false;}
    if(state.uncertain){if(unknown)*unknown=true;e="original native operation uncertain";return false;}
    attempted=true;state.events.push_back("native");++state.mutations;
    if(action==action::Action::StartResearch){if(!tutorial_replication::start_intent::expected_payment(state.ledger,state.price,state.ledger,e))return false;state.row.value.purchased=state.row.value.project_present=true;}
    else if(action==action::Action::SetResearchWorkers)state.row.value.desired_workers=desired;
    else state.row=deployed(state.row);
    if(state.wrong_stamp)++state.row.value.release_counter;
    if(state.wrong_ledger)++state.ledger.line_income[19];
    if(state.wrong_project){if(action==action::Action::CommitResearch)state.row.value.commit_bugs->known=2;else ++state.row.value.inventory_count;}
    if(state.partial){e="original native Research partial effect";return false;}
    return true;
}
struct Session:action::Session {bool current(action::Context& out,std::string& e)override{if(!state.session_ok){e="original session read failed";return false;}out=state.context;return true;}} session;
}
namespace mt2mp::action_session {Session& selected_session(){return fixture::session;}}
namespace mt2mp::game::bug_authority {
bool capture(const action::Context& c,const action::Binding& b,uint64_t revision,State& out,std::string& e){
    fixture::bound(c,b);auto& s=fixture::state;++s.captures;s.revisions.push_back(revision);s.events.push_back(s.captures==1?"capture-before":"capture-after");
    if(s.captures==s.fail_capture){e="original Bug capture failed";return false;}out=fixture::bugs(s.captures>1);return true;
}
}
namespace mt2mp::game::tutorial_replication {
bool read_startingpoint_row(const action::Context&,const action::Binding&,startingpoint::Row&,std::string& e){e="unexpected Startingpoint observation";return false;}
bool read_research_row(const action::Context& c,const action::Binding& b,const std::string& key,research::Row& out,std::string& e){
    fixture::bound(c,b);fixture::need(key=="bridges","native row read selected wrong Tech");auto& s=fixture::state;++s.row_reads;s.events.push_back("row");
    if(s.row_reads==s.fail_row){e="original Research row read failed";return false;}out=s.row;return true;
}
bool start_research(const action::Context& c,const action::Binding& b,const action::Ledger& before,const research::Row& row,action::Ledger& after,research::Row& target,bool& attempted,std::string& e){
    fixture::bound(c,b);auto& s=fixture::state;++s.host_starts;fixture::need(before==s.ledger&&row==s.row,"Host Start input differs from actual BEFORE");
    const bool ok=fixture::mutate(action::Action::StartResearch,0,attempted,nullptr,e);after=s.ledger;target=s.row;return ok;
}
bool apply_research_start(const action::Context& c,const action::Binding& b,const std::string& id,const action::Ledger& before,const action::Ledger& after,const research::Row& row,const research::Row& target,bool& attempted,bool& unknown,std::string& e){
    fixture::bound(c,b);auto& s=fixture::state;++s.client_starts;fixture::need(id==fixture::id&&before==s.ledger&&row==s.row,"Client Start input differs from bound action/BEFORE");
    const bool ok=fixture::mutate(action::Action::StartResearch,0,attempted,&unknown,e);
    if(ok&&!s.wrong_ledger&&!s.wrong_project)fixture::need(s.ledger==after&&research::same_state(s.row,target),"Client Start fixture target differs");return ok;
}
bool set_research_workers(const action::Context& c,const action::Binding& b,const research::Row& before,int32_t desired,bool& attempted,std::string& e){
    fixture::bound(c,b);++fixture::state.worker_calls;fixture::need(before==fixture::state.row,"Worker input differs from BEFORE");return fixture::mutate(action::Action::SetResearchWorkers,desired,attempted,nullptr,e);
}
bool commit_research(const action::Context& c,const action::Binding& b,const research::Row& before,bool& attempted,std::string& e){
    fixture::bound(c,b);++fixture::state.host_commits;fixture::need(before==fixture::state.row&&before.value.bugs_total==5&&before.value.bugs_known==3,"Host Commit lost actual project bug counters");return fixture::mutate(action::Action::CommitResearch,0,attempted,nullptr,e);
}
bool apply_research_commit(const action::Context& c,const action::Binding& b,const research::Row& before,const research::Row& after,bool& attempted,bool& unknown,std::string& e){
    fixture::bound(c,b);++fixture::state.client_commits;fixture::need(before==fixture::state.row&&research::same_state(after,fixture::deployed(before)),"Client Commit input differs from Host deployment");return fixture::mutate(action::Action::CommitResearch,0,attempted,&unknown,e);
}
bool retain_research_commit_effect(const action::Context& c,const action::Binding& b,const std::string& id,const bug_authority::State* before,const bug_authority::State* after,std::string& e){
    fixture::bound(c,b);fixture::need(id==fixture::id,"retention action UUID differs");auto& s=fixture::state;++s.retentions;s.events.push_back("retain");
    if(before)s.retained_before=*before;if(after)s.retained_after=*after;if(!s.retain_ok){e="original Commit effect retention failed";return false;}return true;
}
}
namespace fixture {
struct LiveState {uint64_t next_publication_revision(){return 42;}} live;
struct NativeEngine {
    LiveState* live_state_=&live;
    bool current(const action::Binding& b,const std::string& uuid,std::string& e){need(b==fixture_binding&&uuid==snapshot,"runtime current guard received foreign identity");if(!state.current_ok){e="original current binding failed";return false;}return true;}
    bool read_ledger(action::Ledger& out,std::string& e){++state.ledger_reads;state.events.push_back("ledger");if(state.ledger_reads==state.fail_ledger){e="original ledger read failed";return false;}out=state.ledger;return true;}
#include "research_runtime.inc"
#include "research_observation.inc"
};
action::Request request(action::Action action){action::Request r;r.action=action;r.action_uuid=id;r.snapshot_uuid=snapshot;r.canonical_record_key="tech:bridges";r.desired_workers=action==action::Action::SetResearchWorkers?3:0;return r;}
action::Evidence host_effect(action::Action action){action::Evidence e;e.before=e.after=money();e.research_before=initial(action!=action::Action::StartResearch);e.research_after=e.research_before;std::string error;
    if(action==action::Action::StartResearch){need(tutorial_replication::start_intent::expected_payment(e.before,10000,e.after,error),error);e.research_after->value.project_present=e.research_after->value.purchased=true;}
    else if(action==action::Action::SetResearchWorkers)e.research_after->value.desired_workers=3;
    else e.research_after=deployed(*e.research_before);return e;
}
void success(action::Action action,bool client){
    reset(action,client);NativeEngine engine;auto r=request(action);auto host=host_effect(action);std::string e;
    need(engine.perform_research(fixture_binding,r,client?&host:nullptr,e)==action::Outcome::Confirmed,"actual runtime failed: "+e);
    const auto& receipt=engine.commits_.at(id);need(receipt.confirmed&&receipt.request==r&&receipt.binding==fixture_binding&&receipt.evidence.before==host.before&&receipt.evidence.after==host.after&&receipt.evidence.research_before==host.research_before&&receipt.evidence.research_after==host.research_after,"retained runtime receipt differs from exact actual effect");
    need(state.mutations==1&&state.ledger==host.after,"native operation did not run once with full ledger");
    if(action==action::Action::StartResearch)need((client?state.client_starts:state.host_starts)==1&&state.captures==0&&state.retentions==0&&state.ledger.cash==78160&&state.ledger.line_counts[3]==5&&state.ledger.line_income[19]==77,"Start wrong route, payment or unrelated ledger effects");
    if(action==action::Action::SetResearchWorkers)need(state.worker_calls==1&&state.ledger==money()&&state.row.value.desired_workers==3&&state.row.value.bugs_total==5&&!state.captures&&!state.retentions,"Workers changed fields outside desired count");
    if(action==action::Action::CommitResearch){
        need((client?state.client_commits:state.host_commits)==1&&state.retentions==1&&state.row.value.commit_bugs==tutorial_replication::startingpoint::CommitBugs{5,3},"Commit route or actual receipt differs");
        if(client)need(!state.captures&&!state.retained_before&&!state.retained_after,"Client captured local Bugs or supplied invented Host Bug states");
        else {need(state.captures==2&&state.revisions==std::vector<uint64_t>({42,42})&&state.retained_before==bugs(false)&&state.retained_after==bugs(true),"Host did not retain exact before/after Bug captures");need(state.events==std::vector<std::string>({"ledger","row","capture-before","native","ledger","row","capture-after","retain"}),"Host Bug capture/effect/readback ordering differs");}
    }
    const auto calls=state.events;need(engine.perform_research(fixture_binding,r,client?&host:nullptr,e)==action::Outcome::Unknown&&state.events==calls&&state.mutations==1&&e.find("no replay")!=std::string::npos,"retained action replayed payment or native mutation");
}
void process_stamps(){
    for(const auto action:{action::Action::StartResearch,action::Action::SetResearchWorkers,action::Action::CommitResearch}){
        reset(action,true);state.row.value.release_counter=560;NativeEngine engine;auto r=request(action);auto host=host_effect(action);
        host.research_before->value.release_counter=440;host.research_after->value.release_counter=440+uint32_t(action==action::Action::CommitResearch);
        std::string e;
        need(engine.perform_research(fixture_binding,r,&host,e)==action::Outcome::Confirmed,"unequal process stamps rejected native Research action: "+e);
        const auto& actual=engine.commits_.at(id).evidence;
        need(actual.research_before->value.release_counter==560&&actual.research_after->value.release_counter==560+uint32_t(action==action::Action::CommitResearch),
             "runtime replaced actual local stamp receipt with Host values");
        need(research::same_state(actual.research_before,host.research_before)&&research::same_state(actual.research_after,host.research_after),
             "runtime accepted unequal semantic Research evidence");
        const auto retained=actual;state.row.value.release_counter+=120;action::Evidence observed;
        need(engine.observe_commit(fixture_binding,r,observed,e)&&observed==retained,
             "late scan changed immutable execution receipt or rejected unchanged native state");
        ++state.row.value.release_change_count;
        need(!engine.observe_commit(fixture_binding,r,observed,e)&&e=="Research observation differs from retained result",
             "late semantic Release change passed observation");
    }
    for(bool client:{false,true}){reset(action::Action::CommitResearch,client);state.row.value.release_counter=560;state.wrong_stamp=true;
        NativeEngine engine;auto r=request(action::Action::CommitResearch);auto host=host_effect(r.action);std::string e;
        need(engine.perform_research(fixture_binding,r,client?&host:nullptr,e)==action::Outcome::Unknown&&state.mutations==1&&!engine.commits_.at(id).confirmed,
             "wrong native local Commit stamp increment became confirmation");}
}
void no_effect(){
    for(int mode=0;mode<8;++mode){reset(action::Action::CommitResearch);NativeEngine engine;auto r=request(action::Action::CommitResearch);std::string e;
        if(mode==0)state.current_ok=false;if(mode==1)state.session_ok=false;if(mode==2)state.fail_ledger=1;if(mode==3)state.fail_row=1;if(mode==4)engine.live_state_=nullptr;if(mode==5)state.fail_capture=1;if(mode==6)r.canonical_record_key="tech:friendslist";if(mode==7)for(size_t i=0;i<action::history_limit;++i)engine.commits_.emplace(std::to_string(i),NativeEngine::CommitOperation{});
        need(engine.perform_research(fixture_binding,r,nullptr,e)==action::Outcome::RejectedNoEffect&&!state.mutations&&!state.host_commits&&!state.retentions,"preflight failure executed Commit");
        if(mode!=6)need(!e.empty(),"boundary failure lost original cause");
    }
    for(const auto action:{action::Action::StartResearch,action::Action::SetResearchWorkers,action::Action::CommitResearch}){
        reset(action);NativeEngine engine;auto r=request(action);std::string e;state.reject=true;
        need(engine.perform_research(fixture_binding,r,nullptr,e)==action::Outcome::RejectedNoEffect&&!state.mutations&&e=="original Research eligibility rejected","native no-effect rejection lost outcome/cause");
        need(engine.commits_.contains(id)&&!engine.commits_.at(id).confirmed,"attempted action receipt omitted on native rejection");
        const auto calls=state.events;need(engine.perform_research(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&state.events==calls,"rejected retained attempt reentered native API");
    }
    for(int mode=0;mode<4;++mode){reset(action::Action::CommitResearch,true);NativeEngine engine;auto r=request(action::Action::CommitResearch);auto host=host_effect(r.action);std::string e;
        if(mode==0)++host.before.cash;if(mode==1)host.research_before->value.bugs_known=2;if(mode==2)host.research_after.reset();if(mode==3)state.context.role=action::Role::Host;
        need(engine.perform_research(fixture_binding,r,&host,e)==action::Outcome::RejectedNoEffect&&engine.commits_.empty()&&!state.captures&&!state.mutations&&!state.retentions,"Client authority BEFORE mismatch reached native effects");
    }
}
void partial(){
    for(const auto action:{action::Action::StartResearch,action::Action::SetResearchWorkers,action::Action::CommitResearch})for(int mode=0;mode<5;++mode){
        reset(action);NativeEngine engine;auto r=request(action);std::string e;
        if(mode==0)state.partial=true;if(mode==1)state.fail_ledger=2;if(mode==2)state.fail_row=2;if(mode==3)state.wrong_ledger=true;if(mode==4)state.wrong_project=true;
        need(engine.perform_research(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&state.mutations==1&&!engine.commits_.at(id).confirmed&&!e.empty(),"partial native/actual readback failure became confirmation");
        if(action==action::Action::CommitResearch)need(state.captures==1&&!state.retentions,"invalid Commit readback captured/published after state");
        const auto calls=state.events;need(engine.perform_research(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&state.events==calls,"partial effect replayed");
    }
    {reset(action::Action::StartResearch);NativeEngine engine;auto r=request(action::Action::StartResearch);std::string e;state.price=9999;need(engine.perform_research(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&state.mutations==1&&!engine.commits_.at(id).confirmed&&e.find("price/ledger")!=std::string::npos,"valid ledger with wrong actual Bridges price confirmed");}
    for(int mode=0;mode<2;++mode){reset(action::Action::CommitResearch);NativeEngine engine;auto r=request(action::Action::CommitResearch);std::string e;if(mode==0)state.fail_capture=2;else state.retain_ok=false;
        need(engine.perform_research(fixture_binding,r,nullptr,e)==action::Outcome::Unknown&&state.mutations==1&&state.captures==2&&!engine.commits_.at(id).confirmed,"post-Commit Bug evidence failure confirmed");
        need(e==(mode==0?"original Bug capture failed":"original Commit effect retention failed")&&state.retentions==(mode==0?0:1),"post-Commit Bug cause or retention ordering differs");
    }
    {reset(action::Action::CommitResearch,true);NativeEngine engine;auto r=request(action::Action::CommitResearch);auto host=host_effect(r.action);std::string e;state.wrong_ledger=true;need(engine.perform_research(fixture_binding,r,&host,e)==action::Outcome::Unknown&&!state.captures&&!state.retentions&&e=="Research Client actual result differs from Host","Client wrong actual ledger published Commit effects");}
    {reset(action::Action::CommitResearch,true);NativeEngine engine;auto r=request(action::Action::CommitResearch);auto host=host_effect(r.action);std::string e;state.uncertain=true;need(engine.perform_research(fixture_binding,r,&host,e)==action::Outcome::Unknown&&!state.mutations&&!state.captures&&!state.retentions&&e=="original native operation uncertain","native unknown flag without attempted bit became no-effect rejection");}
}
}
int main(){try{for(const auto action:{action::Action::StartResearch,action::Action::SetResearchWorkers,action::Action::CommitResearch}){fixture::success(action,false);fixture::success(action,true);}fixture::process_stamps();fixture::no_effect();fixture::partial();std::cout<<"PASS "<<fixture::checks<<" actual perform_research assertions\n";return 0;}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
