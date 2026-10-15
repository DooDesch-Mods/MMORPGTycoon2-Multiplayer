#include "tutorial_replication/startingpoint.h"
#include "tutorial_replication/state.h"
#include "live_host_state/state.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace sp=mt2mp::game::tutorial_replication::startingpoint;
namespace tutorial=mt2mp::game::tutorial_replication;
namespace live=mt2mp::game::live_host_state;
namespace {
unsigned tests=0;
void require(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
void pass(const char* name){++tests;std::cout<<"PASS "<<name<<'\n';}
sp::Row before(){sp::Row row;row.project_present=true;row.purchased=true;row.work=78.88058471679688f;row.lines=182;row.desired_workers=1;return row;}
sp::Row after(){sp::Row row;row.purchased=true;row.deployed=true;row.inventory_count=1;row.release_change_count=1;row.release_feature_count=1;row.release_counter=1;row.commit_bugs=sp::CommitBugs{};return row;}
struct Calls {
    sp::Row actual=before();std::vector<std::string> effects,order;
    std::string fail_at;bool changed_owner=false,wrong_readback=false,throw_deploy=false;
    bool pin(const sp::Row& expected,bool deploying,std::string& error){order.push_back(deploying?"pin_deploy":"pin_project");if(changed_owner||actual!=expected){error="original project owner/readback changed";return false;}return true;}
    bool create(std::string& error){error="unexpected creation in deployment-only regression";return false;}
    bool project(const sp::Row& target,std::string& error){order.push_back("project");effects.push_back("project");actual=target;if(fail_at=="project"){error="original project apply partial";return false;}return true;}
    bool deploy(std::string& error){order.push_back("deploy");effects.push_back("deploy");if(throw_deploy)throw std::runtime_error("original native Deploy exception");actual.deployed=true;actual.inventory_count=1;++actual.release_change_count;++actual.release_feature_count;++actual.release_counter;if(fail_at=="deploy"){error="original native Deploy partial";return false;}return true;}
    bool retire(std::string& error){order.push_back("retire");effects.push_back("retire");actual.project_present=false;actual.work=0;actual.lines=0;actual.desired_workers=0;actual.bugs_total=0;actual.bugs_known=0;actual.commit_bugs=sp::CommitBugs{};if(fail_at=="retire"){error="original project delete partial";return false;}return true;}
    bool readback(const sp::Row& target,std::string& error){order.push_back("readback");if(wrong_readback||actual!=target){error="original full native Row readback differs";return false;}return true;}
};
void codec(){
    std::string error;sp::Row decoded;
    require(sp::decode(sp::encode(before()),decoded,error)&&decoded==before(),"retained Host project Row roundtrip differs");
    require(sp::decode(sp::encode(after()),decoded,error)&&decoded==after(),"retained Host deployed Row roundtrip differs");pass("actual retained before/after project, inventory and Release Row roundtrip");
    for(const char* field:{"project_present","purchased","deployed","custom"}){
        auto json=sp::encode(before());json[field]=1;require(!sp::decode(json,decoded,error)&&!error.empty(),"nonboolean Row flag accepted");
    }pass("Row booleans reject integer-shaped flags");
    for(const char* field:{"lines","desired_workers","bugs_total","bugs_known","inventory_count","release_change_count","release_feature_count","release_counter"}){
        auto json=sp::encode(before());json[field]=false;require(!sp::decode(json,decoded,error),"boolean-shaped Row integer accepted");
    }pass("Row integer fields reject booleans");
    for(const char* field:{"lines","desired_workers","bugs_total","bugs_known","inventory_count","release_change_count","release_feature_count","release_counter"}){
        auto json=sp::encode(before());json[field]=1.5;require(!sp::decode(json,decoded,error),"fractional Row integer accepted");
    }pass("Row integer fields reject fractional values");
    auto overflow=sp::encode(before());overflow["release_counter"]=uint64_t(UINT32_MAX)+1;
    require(!sp::decode(overflow,decoded,error),"overflowed native uint32 Release counter accepted");pass("Release counter cannot exceed the native uint32 domain");
    for(const char* field:{"project_present","work","release_counter"}){auto json=sp::encode(before());json.erase(field);require(!sp::decode(json,decoded,error)&&!error.empty(),"missing fixed Row field accepted");}
    auto extra=sp::encode(before());extra["native_pointer"]=123;require(!sp::decode(extra,decoded,error),"extra native pointer Row field accepted");pass("Row exact fields reject missing data and pointers");
    for(float value:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        auto row=before();row.work=value;sp::Plan plan;require(!sp::plan(before(),row,plan,error),"nonfinite native work admitted");
    }pass("nonfinite project work cannot enter an apply plan");
    auto huge=sp::encode(before());huge["work"]=1e300;require(!sp::decode(huge,decoded,error),"wire work overflow reached native float");pass("wire work overflow is rejected before native effects");
    tutorial::State state;tutorial::Cursor cursor;cursor.scenario="initial_tutorial";cursor.count=107;cursor.next=96;state.active.push_back(cursor);state.startingpoint=after();
    tutorial::State received;require(tutorial::decode(tutorial::encode(state),received,error)&&state==received,"fixed Row lost in actual tutorial State codec");pass("actual tutorial State carries the fixed optional Row");
    live::Frame frame;frame.session=1;frame.epoch=2;frame.source_generation=3;frame.revision=4;frame.snapshot="11111111-1111-4111-8111-111111111111";frame.values.calendar={1,1,1,0,0,7};frame.tutorial=state;
    std::vector<uint8_t> bytes;live::Frame replay;
    require(live::encode(frame,bytes,error)&&live::decode(bytes.data(),bytes.size(),replay,error)&&replay==frame,"new Row not carried by actual schema9 Host frame");pass("actual Host frame payload carries Row without separate transport");
}
void transitions(){
    std::string error;sp::Plan plan;bool attempted=false,unknown=false;
    require(sp::plan(before(),after(),plan,error)&&plan==sp::Plan::Deploy,"actual false-to-true deployment not planned");Calls calls;
    require(sp::apply(before(),after(),calls,attempted,unknown,error)&&attempted&&!unknown&&calls.actual==after()&&calls.effects==std::vector<std::string>({"deploy","retire"}),"actual Deploy/retire/readback did not complete once");pass("false-to-true adoption performs original Deploy then retirement once");
    calls.effects.clear();calls.order.clear();attempted=unknown=false;
    require(sp::apply(after(),after(),calls,attempted,unknown,error)&&!attempted&&!unknown&&calls.effects.empty(),"duplicate Row replayed native adoption");pass("identical deployed Row has no native replay");
    auto progressed=before();progressed.work=100;progressed.lines=220;Calls project;
    require(sp::plan(before(),progressed,plan,error)&&plan==sp::Plan::Project&&sp::apply(before(),progressed,project,attempted,unknown,error)&&attempted&&!unknown&&project.effects==std::vector<std::string>{"project"},"project-only update deployed or retired");pass("Host project progress applies through the fixed project path only");
    auto missing=before();missing.project_present=false;missing.work=0;missing.lines=0;missing.desired_workers=0;Calls absent;absent.actual=missing;attempted=unknown=false;
    require(!sp::apply(missing,after(),absent,attempted,unknown,error)&&!attempted&&!unknown&&absent.effects.empty()&&!error.empty(),"missing actual project authorized native Deploy");pass("missing local project cannot authorize first deployment");
    Calls owner;owner.changed_owner=true;attempted=unknown=false;
    require(!sp::apply(before(),after(),owner,attempted,unknown,error)&&!attempted&&!unknown&&owner.effects.empty()&&error=="original project owner/readback changed","owner failure crossed effect boundary");pass("native owner failure is preserved before any effect");
    for(const char* stage:{"deploy","retire"}){Calls partial;partial.fail_at=stage;attempted=unknown=false;
        require(!sp::apply(before(),after(),partial,attempted,unknown,error)&&attempted&&unknown&&!error.empty(),"partial native effect was success-shaped");
        require(partial.effects.front()=="deploy"&&partial.effects.size()==(partial.fail_at=="deploy"?1u:2u),"partial failure repeated or skipped native operation");
    }pass("native Deploy or deletion failure retains partial Unknown and original order");
    Calls wrong;wrong.wrong_readback=true;attempted=unknown=false;
    require(!sp::apply(before(),after(),wrong,attempted,unknown,error)&&attempted&&unknown&&error=="original full native Row readback differs","partial full readback incorrectly acknowledged");pass("post-effect full Row mismatch is Unknown with its original cause");
    require(sp::accepts_complete_tech("initial_tutorial",93,"startingpoint",before())&&sp::accepts_complete_tech("initial_tutorial",93,"startingpoint",after()),"coalesced Step93 not allowed for authoritative fixed Row");
    for(const auto index:{92,94})require(!sp::accepts_complete_tech("initial_tutorial",index,"startingpoint",after()),"other tutorial step inherited CompleteTech suppression");
    require(!sp::accepts_complete_tech("leveltwo_tutorial",93,"startingpoint",after())&&!sp::accepts_complete_tech("initial_tutorial",93,"inn",after())&&!sp::accepts_complete_tech("initial_tutorial",93,"startingpoint",std::nullopt),"foreign/missing authority inherited suppression");pass("coalesced Step93 suppression is exact scenario/index/key/Row only");
}
void guards(){
    std::string error;bool attempted=false,unknown=false;Calls calls;
    tutorial::State state;state.startingpoint=before();auto legacy=tutorial::encode(state);legacy.erase("startingpoint");tutorial::State decoded;
    require(!tutorial::decode(legacy,decoded,error)&&!error.empty(),"mixed Staff schema without Startingpoint field was accepted");
    legacy.erase("staff_display");
    require(tutorial::decode(legacy,decoded,error)&&!decoded.startingpoint,"legacy State absence was fabricated into native project");
    require(!sp::apply(before(),decoded.startingpoint,calls,attempted,unknown,error)&&!attempted&&!unknown&&calls.effects.empty(),"legacy null erased an existing actual project");pass("legacy missing Row stays null and cannot retire an existing project");
    require(!sp::apply(std::nullopt,after(),calls,attempted,unknown,error)&&!attempted&&!unknown&&calls.effects.empty(),"deployed first frame fabricated an unobserved local project");pass("deployed first frame without local baseline cannot cause native adoption");
    for(const char* field:{"inventory_count","release_change_count","release_feature_count","release_counter"}){
        auto malformed=sp::encode(after());malformed[field]=2;sp::Row target;
        if(sp::decode(malformed,target,error)){Calls isolated;attempted=unknown=false;require(!sp::apply(before(),target,isolated,attempted,unknown,error)&&!attempted&&!unknown&&isolated.effects.empty(),"non-exact deployment delta crossed native boundary");}
    }pass("extra inventory or Release effects cannot masquerade as one native deployment");
    for(unsigned altered=0;altered<4;++altered){auto target=before();if(altered==0)target.inventory_count=1;if(altered==1)target.release_change_count=1;if(altered==2)target.release_feature_count=1;if(altered==3)target.release_counter=1;
        Calls isolated;attempted=unknown=false;require(!sp::apply(before(),target,isolated,attempted,unknown,error)&&!attempted&&!unknown&&isolated.effects.empty(),"project-only frame mutated deployment effects");
    }pass("project-only frames cannot alter item or Release effects");
    for(unsigned total=1;total<=2;++total){auto local=before();local.bugs_total=int32_t(total);local.bugs_known=int32_t(total-1);Calls isolated;isolated.actual=local;attempted=unknown=false;
        require(!sp::apply(local,after(),isolated,attempted,unknown,error)&&!attempted&&!unknown&&isolated.effects.empty(),"unreplicated native Map bug effects were admitted");
        require(sp::accepts_complete_tech("initial_tutorial",93,"startingpoint",local),"valid project Row could not cross Step93 before Commit");
    }pass("nonzero actual bugs reject deployment while valid project Step93 can advance");
    for(unsigned flag=0;flag<2;++flag){auto target=after();if(!flag)target.custom=true;else target.purchased=false;Calls isolated;attempted=unknown=false;
        require(!sp::apply(before(),target,isolated,attempted,unknown,error)&&!attempted&&!unknown&&isolated.effects.empty(),"foreign custom or unpurchased record deployed");
    }pass("custom and unpurchased technology cannot enter the fixed native route");
    for(unsigned counter=0;counter<2;++counter){auto local=before(),target=after();if(!counter){local.release_counter=UINT32_MAX;target.release_counter=0;}else{local.release_change_count=4096;target.release_change_count=4096;}Calls isolated;isolated.actual=local;attempted=unknown=false;
        require(!sp::apply(local,target,isolated,attempted,unknown,error)&&!attempted&&!unknown&&isolated.effects.empty(),"native Release increment overflow was admitted");
    }pass("native Release limits cannot wrap into apparently fresh deployment");
    Calls backward;backward.actual=after();attempted=unknown=false;
    require(!sp::apply(after(),before(),backward,attempted,unknown,error)&&!attempted&&!unknown&&backward.effects.empty(),"deployed technology was recreated as a project");pass("backward deployed-to-project transition has no native effects");
    for(const auto& bad:{nlohmann::json(1),nlohmann::json(false),nlohmann::json("1.0")}){auto json=sp::encode(before());json["work"]=bad;sp::Row row;require(!sp::decode(json,row,error),"non-float native work scalar accepted");}pass("native work requires an exact finite wire float type");
    live::Frame frame;frame.session=1;frame.epoch=2;frame.source_generation=3;frame.revision=4;frame.snapshot="11111111-1111-4111-8111-111111111111";frame.values.calendar={1,1,1,0,0,7};frame.tutorial=state;
    auto conflict=frame;conflict.tutorial->startingpoint->work=79;
    mt2mp::action_session::Context context{1,2,1,3,mt2mp::action_session::Role::Client,frame.snapshot};
    require(live::admit(1,context,2,3,&frame,conflict,0,0,false,error)==live::Admission::Rejected,"same Host revision changed only technology Row");pass("same authenticated Host revision cannot change the project payload");
    Calls throwing;throwing.throw_deploy=true;
    attempted=unknown=false;bool caught=false;try{sp::apply(before(),after(),throwing,attempted,unknown,error);}catch(const std::exception& cause){caught=std::string(cause.what())=="original native Deploy exception";}
    require(caught&&attempted&&unknown&&throwing.effects==std::vector<std::string>{"deploy"},"native exception lost attempted/Unknown or continued into retirement");pass("native exception exits with its original cause and partial-effect markers");
}
void receipts(){
    std::string error;sp::Row decoded;sp::Plan plan;bool attempted=false,unknown=false;
    auto wire=sp::encode(after());require(wire.size()==14&&wire["commit_bugs"]==nlohmann::json{{"total",0},{"known",0}}&&sp::decode(wire,decoded,error)&&decoded==after(),"original Commit receipt lost in fixed Row");pass("wire Row carries exact original Host Commit bugs receipt");
    auto missing=after();missing.commit_bugs.reset();Calls calls;
    require(!sp::apply(before(),missing,calls,attempted,unknown,error)&&!attempted&&!unknown&&calls.effects.empty()&&error=="startingpoint Deploy requires original Host Commit receipt with zero bugs","coalesced deployment inferred bugs from already reset project fields");pass("coalesced deployment requires original Commit receipt instead of reset counters");
    auto nonzero=after();nonzero.commit_bugs=sp::CommitBugs{2,1};
    require(sp::valid(nonzero,error)&&!sp::apply(before(),nonzero,calls,attempted,unknown,error)&&!attempted&&calls.effects.empty(),"nonzero Host Commit Map effects were silently discarded");pass("nonzero Host pre-Commit bugs cannot disappear through coalescing");
    for(const auto& bad:{nlohmann::json(false),nlohmann::json::array(),nlohmann::json{{"total",0}},nlohmann::json{{"total",0},{"known",0},{"extra",0}},nlohmann::json{{"total",true},{"known",0}},nlohmann::json{{"total",0.0},{"known",0}},nlohmann::json{{"total",-1},{"known",0}},nlohmann::json{{"total",0},{"known",1}}}){
        auto json=wire;json["commit_bugs"]=bad;require(!sp::decode(json,decoded,error)&&!error.empty(),"malformed Commit receipt accepted");
    }pass("Commit receipt exact fields and signed counters reject malformed authority");
    auto project=before();project.commit_bugs=sp::CommitBugs{};require(!sp::valid(project,error),"Host Commit receipt attached to a still live project");pass("Commit receipt cannot be attached to an uncommitted project Row");
    auto local=before(),target=before();local.bugs_total=2;local.bugs_known=1;target.bugs_total=3;target.bugs_known=2;target.work=90;Calls project_calls;project_calls.actual=local;
    require(sp::plan(local,target,plan,error)&&plan==sp::Plan::Project&&sp::apply(local,target,project_calls,attempted,unknown,error)&&attempted&&!unknown&&project_calls.actual==target&&project_calls.effects==std::vector<std::string>{"project"},"Host project bug counters were dropped before Commit");pass("nonzero Host project counters copy without Deploy or Map effects");
}
void first_release(){
    for(unsigned changed=0;changed<2;++changed){auto local=before(),target=after();
        if(!changed){local.release_change_count=1;target.release_change_count=2;}
        else{local.release_counter=7;target.release_counter=8;}
        Calls calls;calls.actual=local;bool attempted=false,unknown=false;std::string error;
        require(!sp::apply(local,target,calls,attempted,unknown,error)&&!attempted&&!unknown&&calls.effects.empty()&&!error.empty(),"pre-existing Release content entered first original Deploy path");
        pass(!changed?"existing Release changes reject before original Deploy":"existing Release mutation counter rejects before original Deploy");
    }
    Calls first;bool attempted=false,unknown=false;std::string error;
    require(sp::apply(before(),after(),first,attempted,unknown,error)&&attempted&&!unknown&&first.actual==after()&&first.effects==std::vector<std::string>({"deploy","retire"}),"empty initial Release lost its exact0-to-1 deployment");pass("empty actual initial Release still admits one Deploy and retirement");
}
}
int main(int argc,char** argv){try{if(argc==2&&std::string(argv[1])=="guards")guards();else if(argc==2&&std::string(argv[1])=="receipts")receipts();else if(argc==2&&std::string(argv[1])=="first_release")first_release();else{codec();transitions();}std::cout<<tests<<" Startingpoint Client scenarios passed\n";return 0;}catch(const std::exception& cause){std::cerr<<"error: "<<cause.what()<<'\n';return 1;}}
