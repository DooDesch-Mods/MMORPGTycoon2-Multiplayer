#include "tutorial_replication/native.h"
#include "tutorial_development.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
namespace action_session=mt2mp::action_session;
namespace tutorial_development=mt2mp::game::tutorial_development;
namespace mt2mp::game::tutorial_replication {
namespace {
using World=building_mixed_identity::opened_file::CurrentWorld;
struct NativeCursor{};
uintptr_t image=0x140000000;
struct {bool armed=false;}load;
tutorial_development::StartingpointSnapshot sample;
bool completed=true,entry=true,changed=false,partial=false;
unsigned snapshots=0,calls=0,applies=0;
State applied;
bool fail(std::string& e,const std::string& why){e=why;return false;}
bool same_world(const World& a,const World& b){return a.game==b.game&&a.mode==b.mode&&a.state==b.state&&a.world==b.world&&a.map==b.map&&a.uuid==b.uuid&&a.sha256==b.sha256&&a.bytes==b.bytes&&a.world_epoch==b.world_epoch&&a.generation==b.generation;}
bool startingpoint_code(std::string& e){return entry||fail(e,"original entry changed");}
bool complete_entry(std::string& e){return startingpoint_code(e);}
bool commit_snapshot(const action_session::Context&,const action_session::Binding&,tutorial_development::StartingpointSnapshot& out,std::string& e){
    ++snapshots;out=sample;if(changed&&snapshots==2)++out.record;
    if(partial&&snapshots==3)return fail(e,"original post-Commit read failed");return true;
}
unsigned long startingpoint_complete(uintptr_t,bool& result){result=completed;return 0;}
startingpoint::Row deployed(){startingpoint::Row r;r.purchased=r.deployed=true;r.inventory_count=r.release_feature_count=r.release_change_count=1;r.release_counter=1;r.commit_bugs=startingpoint::CommitBugs{};return r;}
unsigned long startingpoint_commit_call(uintptr_t){++calls;sample.project=0;sample.row=deployed();return 0;}
bool native_state(const action_session::Context&,const action_session::Binding&,State& s,std::vector<NativeCursor>&,std::string&){s.startingpoint=sample.row;s.initial_complete=false;return true;}
}
bool verify_commit_entry(uintptr_t,std::string& e){return startingpoint_code(e);}
bool apply(const action_session::Context&,const action_session::Binding&,const State& target,bool& attempted,bool& unknown,std::string&){++applies;attempted=true;unknown=false;applied=target;return true;}
#include "commit_intent_body.inc"
void reset(){sample={};sample.project=1;sample.lounge=2;sample.tech=3;sample.record=4;sample.player=5;sample.inventory=6;sample.slots=7;sample.release=8;sample.origin.uuid="actual-local-owner";sample.origin.bytes=75843;sample.admitted=true;
    startingpoint::Row r;r.project_present=r.purchased=true;r.work=182;r.lines=182;r.desired_workers=1;sample.row=r;
    completed=entry=true;changed=partial=load.armed=false;snapshots=calls=applies=0;applied={};}
void need(bool yes,const char* e){if(!yes)throw std::runtime_error(e);}
void run(){action_session::Context host;host.role=action_session::Role::Host;action_session::Context client;client.role=action_session::Role::Client;action_session::Binding b;
    std::string e;bool attempted=false,unknown=false;
    reset();auto before=*sample.row;need(commit_startingpoint(host,b,before,attempted,e)&&attempted&&calls==1,"Host original Commit not once");
    reset();completed=false;need(!commit_startingpoint(host,b,before,attempted,e)&&!attempted&&!calls&&e.find("not natively complete")!=std::string::npos,"incomplete Host project mutated");
    reset();changed=true;need(!commit_startingpoint(host,b,before,attempted,e)&&!attempted&&!calls&&e.find("owners changed")!=std::string::npos,"changed owner mutated");
    reset();partial=true;need(!commit_startingpoint(host,b,before,attempted,e)&&attempted&&calls==1&&e=="original post-Commit read failed","post-effect failure became no-effect");
    reset();sample.row->bugs_total=1;before=*sample.row;need(!commit_startingpoint(host,b,before,attempted,e)&&!attempted&&!calls,"nonzero bug Commit permitted");
    reset();before=*sample.row;sample.items.fill(99);need(!commit_startingpoint(host,b,before,attempted,e)&&!attempted&&!calls&&e.find("inventory full")!=std::string::npos,"full inventory mutated");
    reset();sample.admitted=false;sample.admission={{"cause","event owner mismatch"}};need(!commit_startingpoint(host,b,before,attempted,e)&&!attempted&&!calls&&e.find("event owner mismatch")!=std::string::npos,"event owner admission cause lost");
    reset();need(!commit_startingpoint(client,b,before,attempted,e)&&!attempted&&!calls,"Client original Commit executed");
    reset();need(apply_startingpoint_commit(client,b,before,deployed(),attempted,unknown,e)&&attempted&&!unknown&&applies==1&&calls==0&&applied.startingpoint==deployed(),"Client did not use existing adopter");
    reset();sample.row->work=17;need(!apply_startingpoint_commit(client,b,before,deployed(),attempted,unknown,e)&&!attempted&&!unknown&&!applies,"Client mismatched before row mutated");
    reset();entry=false;need(!commit_startingpoint(host,b,before,attempted,e)&&!attempted&&!calls&&e=="original entry changed","entry guard failed open");
}
}
int main(){try{mt2mp::game::tutorial_replication::run();std::cout<<"PASS 11 actual native Host/Client intent body cases\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
