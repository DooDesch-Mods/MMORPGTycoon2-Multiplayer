#include "tutorial_replication/state.h"
#include "typed_action/action_session.h"
#include "bug_authority/codec.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace mt2mp::game::tutorial_replication {
namespace {
struct World {std::string uuid="native-origin";bool operator==(const World&)const=default;};
World research_proof_origin;
nlohmann::json research_commit_proof;
size_t checks=0;
void check(bool ok,const char* cause){++checks;if(!ok)throw std::runtime_error(cause);}
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool same_world(const World& a,const World& b){return a==b;}
bool origin(const action_session::Context&,const action_session::Binding&,World& out,std::string&){out={};return true;}
std::string fail_call;
std::vector<std::string> call_events;
struct Receipt {World origin;uintptr_t tech=0,record=0;std::optional<startingpoint::CommitBugs> bugs;std::string cause;} research_commit_receipt;
namespace tutorial_development {struct StartingpointSnapshot {std::optional<startingpoint::Row> row;int witness=0;};}
struct StartingpointCalls {
    StartingpointCalls(const action_session::Context&,const action_session::Binding&){}
    std::string key;bool share_reader_metadata=false,creating=false;
    struct {World origin;uintptr_t tech=11,record=12;std::optional<startingpoint::Row> row;} before;
    bool event(const std::string& step,std::string& e){call_events.push_back(step);return step!=fail_call||fail(e,"original "+step+" failed");}
    bool pin(const startingpoint::Row& row,bool deploy,std::string& e){check(key=="bridges","exact keyed pin");before.row=row;return event(deploy?"pin-deploy":"pin",e);}
    bool create(std::string& e){return event("create",e);}
    bool project(const startingpoint::Row&,std::string& e){return event("project",e);}
    bool deploy(std::string& e){return event("deploy",e);}
    bool retire(std::string& e){return event("retire",e);}
    bool readback(const startingpoint::Row& row,std::string& e,tutorial_development::StartingpointSnapshot* observed=nullptr){if(!event("readback",e))return false;if(observed)*observed={row,71};return true;}
};
#include "research_adoption_body.inc"
#include "research_effect_body.inc"
State base(){State s;startingpoint::Row old;old.purchased=old.deployed=true;old.inventory_count=old.release_feature_count=1;old.release_change_count=5;old.release_counter=7;s.startingpoint=old;s.terminal_startingpoint=old;
    research::Row row;row.key="bridges";row.value.project_present=row.value.purchased=true;row.value.release_change_count=5;row.value.release_counter=7;row.value.bugs_total=5;row.value.bugs_known=3;row.value.work=21600;row.value.desired_workers=1;s.research=row;return s;}
State deployed(State s){auto& r=s.research->value;r.project_present=false;r.deployed=true;r.inventory_count=r.release_feature_count=1;r.work=0;r.lines=r.desired_workers=r.bugs_total=r.bugs_known=0;++r.release_change_count;++r.release_counter;r.commit_bugs=startingpoint::CommitBugs{5,3};s.startingpoint->release_change_count=r.release_change_count;s.startingpoint->release_counter=r.release_counter;return s;}
}
int run(){std::string e;auto before=base(),target=deployed(before);State projection;research::Plan plan=research::Plan::None;
    check(research_projection(before,target,projection,plan,e)&&plan==research::Plan::Deploy,"nonzero Research Deploy accepted");
    check(projection.startingpoint==target.startingpoint&&projection.terminal_startingpoint==before.terminal_startingpoint,"live Release coupling preserves terminal history");
    check(projection.research==target.research,"selected Research projection");
    auto invalid=target;invalid.startingpoint->release_feature_count++;check(!research_projection(before,invalid,projection,plan,e),"unrelated Startingpoint feature rejected");
    invalid=target;invalid.research->value.commit_bugs->known--;check(!research_projection(before,invalid,projection,plan,e),"exact Host Bug totals required");
    State absent=before;absent.research.reset();check(!research_projection(absent,target,projection,plan,e),"no unowned appearance bypass");
    action_session::Context context;action_session::Binding binding;bool attempted=false,unknown=false;
    call_events.clear();check(research_preflight(context,binding,before,target,projection,e)&&call_events==std::vector<std::string>{"pin-deploy"},"preflight has no mutation");
    call_events.clear();check(research_apply(context,binding,before,target,attempted,unknown,e),"native Deploy/retire/readback path");
    check(attempted&&!unknown&&call_events==std::vector<std::string>{"pin-deploy","deploy","retire","readback"},"no map RNG or worker AI call");
    check(research_commit_receipt.bugs==target.research->value.commit_bugs,"receipt exact nonzero bugs");
    for(const auto& stage:{"deploy","retire","readback"}){call_events.clear();fail_call=stage;attempted=unknown=false;
        check(!research_apply(context,binding,before,target,attempted,unknown,e)&&attempted&&unknown&&e==std::string("original ")+stage+" failed","partial failure retains Unknown/cause");}
    fail_call="pin-deploy";attempted=unknown=false;call_events.clear();check(!research_apply(context,binding,before,target,attempted,unknown,e)&&!attempted&&!unknown&&call_events.size()==1,"preflight failure no effects");fail_call.clear();
    target=before;target.research->value.work+=3;call_events.clear();attempted=unknown=false;
    check(research_apply(context,binding,before,target,attempted,unknown,e)&&call_events==std::vector<std::string>{"pin","project","readback"},"ordinary Host work adoption");
    {
        StartingpointCalls prepared{context,binding};tutorial_development::StartingpointSnapshot observed;
        call_events.clear();attempted=unknown=false;
        check(research_preflight(context,binding,before,target,projection,e,&prepared),"local prepared pin");
        check(research_apply(context,binding,before,target,attempted,unknown,e,&prepared,&observed)&&
              call_events==std::vector<std::string>{"pin","project","readback"}&&observed.witness==71&&observed.row==target.research->value,
              "local Progress consumes one pin and returns successful exact readback");
        for(int guard=0;guard<4;++guard){auto bad=prepared;if(guard==0)bad.key="startingpoint";if(guard==1)bad.before.row.reset();if(guard==2)bad.before.row->work+=1;if(guard==3)bad.creating=true;
            call_events.clear();attempted=unknown=false;
            check(!research_apply(context,binding,before,target,attempted,unknown,e,&bad)&&!attempted&&!unknown&&call_events.empty()&&e=="Research prepared Progress owner differs","foreign prepared pin has no effect");}
        call_events.clear();attempted=unknown=false;
        check(!research_apply(context,binding,before,deployed(before),attempted,unknown,e,&prepared)&&!attempted&&call_events.empty(),"prepared Progress cannot bypass Deploy pin");
        fail_call="readback";observed={};attempted=unknown=false;
        check(!research_apply(context,binding,before,target,attempted,unknown,e,&prepared,&observed)&&attempted&&unknown&&observed.witness==0&&e=="original readback failed","failed Progress cannot export success-shaped snapshot");
        fail_call.clear();
    }
    before=base();before.research->value={};before.research->value.release_change_count=5;before.research->value.release_counter=7;
    target=before;target.research->value.project_present=target.research->value.purchased=true;call_events.clear();attempted=unknown=false;
    check(research_apply(context,binding,before,target,attempted,unknown,e)&&call_events==std::vector<std::string>{"pin","create","project","readback"},"original selected creation path");
    call_events.clear();attempted=unknown=false;check(research_apply(context,binding,target,target,attempted,unknown,e)&&call_events.empty()&&!attempted&&!unknown,"equal frame has no effects");
    const std::string action="12345678-1234-4234-8234-123456789abc";
    context.role=action_session::Role::Host;context.snapshot_uuid="aaaaaaaa-1234-4234-8234-123456789abc";
    bug_authority::State bugs_before,bugs_after;
    bug_authority::Row bug;bug.parent_key="action:"+action;bug.birth={bug_authority::BirthKind::Action,action,0,1};bug.value.variant=3;
    bugs_after.rows.push_back(bug);research_commit_proof={{"release",{{"preserved",true}}}};
    check(retain_research_commit_effect(context,binding,action,&bugs_before,&bugs_after,e),"actual Host new Action-born Bug delta");
    check(research_commit_proof.at("confirmed")==true&&research_commit_proof.at("action_uuid")==action&&research_commit_proof.at("map_bugs").at("added").size()==1,"Host proof binds action and actual added rows");
    bugs_before=bugs_after;bugs_after.rows[0].value.type=1;
    check(retain_research_commit_effect(context,binding,action,&bugs_before,&bugs_after,e)&&research_commit_proof.at("map_bugs").at("changed").size()==1,"original worsening is reported as changed same birth");
    bugs_before={};bugs_after={};check(retain_research_commit_effect(context,binding,action,&bugs_before,&bugs_after,e)&&research_commit_proof.at("map_bugs").at("added").empty(),"original no-target creates no fabricated births");
    bugs_after.rows.push_back(bug);bugs_after.rows[0].birth.source="bbbbbbbb-1234-4234-8234-123456789abc";
    check(!retain_research_commit_effect(context,binding,action,&bugs_before,&bugs_after,e),"foreign action birth rejected");
    research_proof_origin.uuid="stale";check(!retain_research_commit_effect(context,binding,action,&bugs_before,&bugs_after,e),"stale native origin rejected");research_proof_origin={};
    research_commit_proof=nullptr;check(!retain_research_commit_effect(context,binding,action,&bugs_before,&bugs_after,e),"missing actual Release proof rejected");
    research_commit_proof={{"release",{{"preserved",true}}}};context.role=action_session::Role::Client;
    check(retain_research_commit_effect(context,binding,action,nullptr,nullptr,e)&&!research_commit_proof.contains("map_bugs"),"Client receipt preserves existing Host frame map authority");
    check(!retain_research_commit_effect(context,binding,action,&bugs_before,&bugs_after,e),"Client local Bug effect capture rejected");
    std::cout<<checks<<" actual Research adoption checks passed\n";return 0;
}
}
int main(){try{return mt2mp::game::tutorial_replication::run();}catch(const std::exception& x){std::cerr<<"Research adoption failure: "<<x.what()<<'\n';return 1;}}
