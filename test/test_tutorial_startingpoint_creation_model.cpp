#include "tutorial_replication/startingpoint.h"
#include <iostream>
#include <stdexcept>
#include <vector>
namespace sp=mt2mp::game::tutorial_replication::startingpoint;
namespace {
unsigned count=0;
void need(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
void pass(const char* name){++count;std::cout<<"PASS "<<name<<'\n';}
sp::Row project(){sp::Row row;row.purchased=row.project_present=true;row.work=78.88058471679688f;row.lines=182;row.desired_workers=1;return row;}
struct Calls {
    sp::Row actual;std::vector<std::string> order;std::string failure;
    bool pin(const sp::Row& row,bool deploying,std::string& e){order.push_back("pin");if(actual!=row||deploying){e="original owner pin changed";return false;}return true;}
    bool create(std::string& e){order.push_back("create");actual.purchased=actual.project_present=true;if(failure=="create"){e="original create uncertain";return false;}return true;}
    bool project(const sp::Row& row,std::string& e){order.push_back("project");actual=row;if(failure=="project"){e="original project store failed";return false;}return true;}
    bool readback(const sp::Row& row,std::string& e){order.push_back("readback");if(actual!=row||failure=="readback"){e="original full readback differs";return false;}return true;}
    bool deploy(std::string& e){order.push_back("unexpected_deploy");e="unexpected Deploy";return false;}
    bool retire(std::string& e){order.push_back("unexpected_retire");e="unexpected retirement";return false;}
};
}
int main(){try{
    std::string error;sp::Row decoded;sp::Plan operation;bool attempted=false,unknown=false;
    for(bool purchased:{false,true}){sp::Row baseline;baseline.purchased=purchased;need(sp::valid(baseline,error)&&sp::decode(sp::encode(baseline),decoded,error)&&decoded==baseline,"explicit owned record baseline rejected");}pass("explicit unpurchased or purchased no-project baseline survives actual codec");
    {sp::Row baseline;need(sp::plan(baseline,project(),operation,error)&&operation==sp::Plan::Create,"baseline did not choose Create");Calls calls;need(sp::apply(baseline,project(),calls,attempted,unknown,error)&&attempted&&!unknown&&calls.actual==project()&&calls.order==std::vector<std::string>({"pin","create","project","readback"}),"actual Create orchestration changed order");pass("explicit baseline creates before copying Host work and final readback");}
    {sp::Row baseline;baseline.purchased=true;Calls calls;calls.actual=baseline;need(sp::apply(baseline,project(),calls,attempted,unknown,error)&&calls.actual==project(),"purchased no-project baseline cannot create");pass("purchased no-project record can start the owned catalogue project");}
    {Calls calls;attempted=unknown=false;need(!sp::apply(std::nullopt,project(),calls,attempted,unknown,error)&&!attempted&&!unknown&&calls.order.empty(),"optional absence fabricated a native record");pass("optional null cannot authorize native creation");}
    {Calls calls;calls.actual=project();attempted=unknown=false;need(sp::apply(project(),project(),calls,attempted,unknown,error)&&!attempted&&!unknown&&calls.order.empty(),"duplicate Host project replayed original creation");pass("identical project Row causes no replay");}
    for(const auto stage:{"create","project","readback"}){Calls calls;calls.failure=stage;attempted=unknown=false;need(!sp::apply(sp::Row{},project(),calls,attempted,unknown,error)&&attempted&&unknown&&!error.empty(),"partial creation or store failure was acknowledged");need(calls.order.size()==(calls.failure=="create"?2u:calls.failure=="project"?3u:4u),"failure continued past native boundary");}pass("uncertain Create, project store and final readback preserve original failure and stop");
    for(unsigned effect=0;effect<4;++effect){auto target=project();if(effect==0)target.inventory_count=1;if(effect==1)target.release_change_count=1;if(effect==2){target.release_change_count=target.release_feature_count=1;}if(effect==3)target.release_counter=1;need(!sp::plan(sp::Row{},target,operation,error),"creation changed deployment effects");}pass("creation cannot alter inventory or Release effects");
    {auto baseline=sp::Row{};auto target=project();target.custom=true;need(!sp::plan(baseline,target,operation,error),"custom Host project created");target=project();target.purchased=false;need(!sp::plan(baseline,target,operation,error),"unpurchased Host project created");pass("Create target must be purchased and non-custom");}
    {sp::Row baseline;need(!sp::accepts_complete_tech("initial_tutorial",93,"startingpoint",baseline),"new unowned baseline authorized CompleteTech");baseline.purchased=true;need(!sp::accepts_complete_tech("initial_tutorial",93,"startingpoint",baseline),"purchased absent project authorized CompleteTech");pass("new explicit baseline does not admit Step93 native effects");}
    {sp::Row baseline;baseline.commit_bugs=sp::CommitBugs{};need(!sp::valid(baseline,error),"uncommitted baseline carried Host Commit proof");pass("baseline cannot carry deployed Commit receipt");}
    {sp::Row baseline;baseline.inventory_count=1;need(!sp::valid(baseline,error),"baseline carried existing native item");baseline={};baseline.release_change_count=baseline.release_feature_count=1;need(!sp::valid(baseline,error),"baseline carried existing Startingpoint feature");pass("baseline cannot hide existing native deployment effects");}
    {auto wire=sp::encode(sp::Row{});wire["purchased"]=0;need(!sp::decode(wire,decoded,error),"new baseline accepted scalar-shaped bool");pass("new baseline uses actual strict typed Row decoder");}
    std::cout<<count<<" creation model scenarios passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
