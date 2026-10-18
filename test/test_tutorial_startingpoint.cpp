#include "vendor/nlohmann/json.hpp"
#include "native_open_runtime.h"
#include "tutorial_development.h"
namespace tutorial_development=mt2mp::game::tutorial_development;
namespace {
tutorial_development::StartingpointSnapshot fixture_research;
bool fixture_read_failure=false;
unsigned fixture_reads=0;
}
namespace mt2mp::game::tutorial_development {
bool read_research(const std::string& key,StartingpointSnapshot& out,nlohmann::json& diagnostic,
                   std::string& error,bool creation,bool share_metadata){
    ++fixture_reads;out={};diagnostic=nullptr;
    if(fixture_read_failure){error="original Research read failure";return false;}
    if(key!="bridges"||!creation||share_metadata){error="Research reader contract differs";return false;}
    out=fixture_research;diagnostic=nlohmann::json::object();error.clear();return true;
}
}

#include "tutorial_startingpoint_fixture/controller_admission.h"
#include "tutorial_startingpoint_fixture/begin_guards.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string>
using json=nlohmann::json;
using World=mt2mp::game::building_mixed_identity::opened_file::CurrentWorld;
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
// Exercise the actual admission and inventory checks with the retained native baseline.
#include "startingpoint_guards.inc"
int main(){
    unsigned checks=0;
    const auto check=[&](bool pass,const char* name){++checks;if(!pass)throw std::runtime_error(name);};
    try{
        using mt2mp::game::tutorial_startingpoint_fixture::controller_admission;
        std::string admission_error;
        const json idle={{"initialized",true},{"started",true},{"locked",false},{"pending",false},{"lobby_retiring",false},{"cause",""},
                         {"live_state",{{"active",true},{"locked",false},{"unknown",false},{"cause",""}}}};
        check(controller_admission(json{{"initialized",false}},admission_error),"isolated uninitialized Host rejected");
        check(controller_admission(idle,admission_error),"idle authoritative Host rejected");
        for(const char* field:{"locked","pending","lobby_retiring"}){
            auto changed=idle;changed[field]=true;
            check(!controller_admission(changed,admission_error),"busy Controller admitted");
        }
        for(const char* field:{"locked","unknown"}){
            auto changed=idle;changed["live_state"][field]=true;
            check(!controller_admission(changed,admission_error),"uncertain live authority admitted");
        }
        auto changed=idle;changed["started"]=false;check(!controller_admission(changed,admission_error),"unstarted Controller admitted");
        changed=idle;changed["live_state"]["active"]=false;check(!controller_admission(changed,admission_error),"inactive live authority admitted");
        changed=idle;changed["cause"]="original failure";check(!controller_admission(changed,admission_error)&&admission_error.find("original failure")!=std::string::npos,"Controller failure admitted or hidden");
        changed=idle;changed["live_state"]["cause"]="original live failure";check(!controller_admission(changed,admission_error)&&admission_error.find("original live failure")!=std::string::npos,"live failure admitted or hidden");
        changed=idle;changed["error"]="read failed";check(!controller_admission(changed,admission_error)&&admission_error.find("read failed")!=std::string::npos,"Controller read cause discarded");
        changed=idle;changed.erase("pending");check(!controller_admission(changed,admission_error),"missing Controller field admitted");
        using mt2mp::game::tutorial_startingpoint_fixture::same_expected_development;
        auto prior_scan=native_baseline,current_scan=native_baseline;
        prior_scan["startingpoint_release"]["mutation_counter"]=440;
        current_scan["startingpoint_release"]["mutation_counter"]=560;
        check(same_expected_development(current_scan,prior_scan),"later native scan rejected expected development");
        check(current_scan!=prior_scan&&prior_scan["startingpoint_release"]["mutation_counter"]==440,
              "observation comparison rewrote original diagnostics");
        for(const char* path:{"/project/work","/project/lines","/project/bugs_total","/project/desired_workers",
                             "/startingpoint/inventory_count","/startingpoint_release/change_count","/startingpoint_release/feature_count"}){
            auto semantic=current_scan;semantic[json::json_pointer(path)]=999;
            check(!same_expected_development(semantic,prior_scan),"meaningful development change ignored");
        }
        auto semantic=current_scan;semantic["startingpoint_release"]["feature_present"]=true;
        check(!same_expected_development(semantic,prior_scan),"native Release feature change ignored");
        semantic=current_scan;semantic["startingpoint"]["deployed"]=true;
        check(!same_expected_development(semantic,prior_scan),"native deployment change ignored");
        for(const auto& bad:std::vector<json>{nullptr,-1,4294967296ULL,1.5,"560"}){
            semantic=current_scan;semantic["startingpoint_release"]["mutation_counter"]=bad;
            check(!same_expected_development(semantic,prior_scan),"invalid observed scan stamp accepted");
        }
        semantic=current_scan;semantic["startingpoint_release"].erase("mutation_counter");
        check(!same_expected_development(semantic,prior_scan),"missing observed scan stamp accepted");
        Pin baseline;baseline.development=native_baseline;baseline.project=1;
        baseline.player=10;baseline.inventory=20;baseline.slots=30;baseline.items[0]=40;
        std::string error;
        check(before(baseline,error),"retained native zero-bug baseline rejected");
        Pin bridges;bridges.tech_key="bridges";bridges.project=44;
        fixture_research={};fixture_research.origin=bridges.world;fixture_research.project=bridges.project;
        fixture_research.row=mt2mp::game::tutorial_replication::startingpoint::Row{};
        fixture_research.row->project_present=fixture_research.row->purchased=true;
        fixture_research.row->bugs_total=5;fixture_research.row->bugs_known=3;
        const auto research_baseline=fixture_research;
        check(before(bridges,error)&&fixture_reads==1,"actual Bridges guard rejected owned purchased project or reader contract");
        for(unsigned field=0;field<10;++field){
            fixture_research=research_baseline;
            if(field==0)++fixture_research.origin.generation;
            if(field==1)++fixture_research.project;
            if(field==2)fixture_research.row.reset();
            if(field==3)fixture_research.row->project_present=false;
            if(field==4)fixture_research.row->purchased=false;
            if(field==5)fixture_research.row->deployed=true;
            if(field==6)fixture_research.row->custom=true;
            if(field==7)fixture_research.row->inventory_count=1;
            if(field==8)fixture_research.row->release_feature_count=1;
            if(field==9)fixture_research.origin.uuid="other";
            check(!before(bridges,error)&&error=="Bridges Commit requires exact purchased current unowned project",
                  "actual Bridges guard accepted changed owner/project/record");
        }
        fixture_research=research_baseline;bridges.items.fill(1);
        check(!before(bridges,error)&&error=="Bridges Commit inventory full","actual Bridges guard accepted full inventory");
        bridges.items[199]=0;
        check(before(bridges,error),"actual Bridges guard rejected final free slot");
        fixture_read_failure=true;
        check(!before(bridges,error)&&error=="original Research read failure","actual Bridges guard hid native reader failure");
        fixture_read_failure=false;

        for(const auto* counter:{"bugs_total","bugs_known","bugs_unknown"}){
            auto altered=baseline;altered.development["project"][counter]=1;error.clear();
            check(!before(altered,error)&&error.find("bug counters0")!=std::string::npos,"nonzero Map effect admitted");
        }
        for(const auto* flag:{"deployed","custom","owned"}){
            auto altered=baseline;altered.development["startingpoint"][flag]=true;error.clear();
            check(!before(altered,error)&&!error.empty(),"already-owned record admitted");
        }
        for(const auto* flag:{"record_present","purchased"}){
            auto altered=baseline;altered.development["startingpoint"][flag]=false;error.clear();
            check(!before(altered,error)&&!error.empty(),"absent or unpurchased record admitted");
        }
        auto altered=baseline;altered.development["project"]["tech_key"]="other";
        check(!before(altered,error),"another technology admitted");
        altered=baseline;altered.development["startingpoint"]["inventory_count"]=1;
        check(!before(altered,error),"existing StartingPoints item admitted");
        altered=baseline;altered.items.fill(40);
        check(!before(altered,error)&&error.find("free native inventory slot")!=std::string::npos,"full inventory admitted");
        altered=baseline;altered.items[1]=50;error.clear();
        check(one_inventory_insert(baseline,altered,error),"single insert rejected");
        check(!one_inventory_insert(baseline,baseline,error),"no insert confirmed");
        altered.items[2]=60;check(!one_inventory_insert(baseline,altered,error),"duplicate insert confirmed");
        altered=baseline;altered.items[0]=50;check(!one_inventory_insert(baseline,altered,error),"replacement confirmed");
        altered=baseline;altered.items[0]=0;check(!one_inventory_insert(baseline,altered,error),"removal confirmed");
        altered=baseline;altered.items[1]=50;altered.inventory=21;
        check(!one_inventory_insert(baseline,altered,error),"foreign inventory confirmed");
        using mt2mp::game::tutorial_startingpoint_fixture::before_begin;
        using mt2mp::game::tutorial_startingpoint_fixture::begin_payment;
        auto unstarted=native_baseline;
        unstarted["project"].update({{"present",false},{"work",0.0f},{"lines",0},{"desired_workers",0},{"bugs_total",0},{"bugs_known",0}});
        unstarted["startingpoint"].update({{"record_present",true},{"purchased",false},{"deployed",false},{"custom",false},{"owned",false},{"inventory_count",0}});
        unstarted["startingpoint_release"]["feature_count"]=0;
        error="stale";
        check(before_begin(unstarted,0,error)&&error.empty(),"owned unpurchased no-project baseline rejected");
        check(!before_begin(unstarted,1,error)&&!error.empty(),"native current project pointer admitted for begin");
        auto begin_changed=unstarted;begin_changed["project"]["present"]=true;
        check(!before_begin(begin_changed,0,error),"present project diagnostic admitted for begin");
        for(const auto* field:{"work","lines","desired_workers","bugs_total","bugs_known"}){
            begin_changed=unstarted;begin_changed["project"][field]=1;
            check(!before_begin(begin_changed,0,error),"unreset native project counters admitted for begin");
        }
        for(const auto* field:{"purchased","deployed","custom","owned"}){
            begin_changed=unstarted;begin_changed["startingpoint"][field]=true;
            check(!before_begin(begin_changed,0,error),"already bought or owned record admitted for begin");
        }
        begin_changed=unstarted;begin_changed["startingpoint"]["record_present"]=false;
        check(!before_begin(begin_changed,0,error),"missing owned TechRecord admitted for begin");
        begin_changed=unstarted;begin_changed["startingpoint"]["inventory_count"]=1;
        check(!before_begin(begin_changed,0,error),"existing native item admitted for begin");
        begin_changed=unstarted;begin_changed["startingpoint_release"]["feature_count"]=1;
        check(!before_begin(begin_changed,0,error),"existing native feature admitted for begin");
        mt2mp::game::native::SimulationState ledger;
        ledger.cash=1000;ledger.record_present=true;ledger.record_day=9;ledger.record_cash=1000;
        ledger.record_income=37;ledger.record_expense=20;ledger.logged_in_subscribers=4;
        ledger.clock.flat_day=9;ledger.clock.seconds=123;ledger.clock.time_scale=0;ledger.clock.pause_scale=3;ledger.clock.paused=true;
        for(size_t i=0;i<20;++i){ledger.line_counts[i]=int32_t(i+1);ledger.line_income[i]=int32_t(i+100);}
        const json original_ledger={{"cash",1000},{"record_present",true},{"record_day",9},{"record_cash",1000},
            {"record_income",37},{"record_expense",20},{"line_counts",ledger.line_counts},{"line_income",ledger.line_income},
            {"subscribers",4},{"clock_day",9},{"clock_seconds",123},{"clock_scale",0.0f},{"pause_scale",3.0f},{"paused",true}};
        json expected;
        error="stale";
        check(begin_payment(ledger,0,expected,error)&&error.empty(),"zero-price original payment rejected");
        auto desired=original_ledger;desired["line_counts"][3]=5;
        check(expected==desired,"zero price must increment category3 only, including all20 ledger groups");
        check(begin_payment(ledger,200,expected,error),"affordable original payment rejected");
        desired=original_ledger;desired["cash"]=800;desired["record_cash"]=800;desired["record_expense"]=220;
        desired["line_counts"][3]=5;desired["line_income"][3]=-97;
        check(expected==desired,"positive payment changed wrong cash/current record/category or unrelated native state");
        check(!begin_payment(ledger,-1,expected,error)&&expected.is_null()&&!error.empty(),"negative price accepted or stale expected ledger retained");
        check(!begin_payment(ledger,1001,expected,error)&&expected.is_null(),"unaffordable price accepted");
        for(unsigned field=0;field<5;++field){
            auto invalid=ledger;
            if(field==0)invalid.record_present=false;
            if(field==1)invalid.record_day=8;
            if(field==2)invalid.record_cash=999;
            if(field==3)invalid.clock.time_scale=1;
            if(field==4)invalid.clock.paused=false;
            check(!begin_payment(invalid,1,expected,error)&&expected.is_null()&&error.find("paused matching current ledger")!=std::string::npos,"unpaused or stale current ledger admitted");
        }
        for(unsigned field=0;field<3;++field){
            auto invalid=ledger;
            if(field==0)invalid.line_counts[3]=INT32_MAX;
            if(field==1)invalid.record_expense=INT32_MAX;
            if(field==2)invalid.line_income[3]=INT32_MIN;
            check(!begin_payment(invalid,1,expected,error)&&expected.is_null()&&error=="startingpoint begin category3 ledger would overflow","native payment overflow accepted or cause hidden");
        }
        auto boundary=ledger;boundary.line_counts[3]=INT32_MAX-1;boundary.record_expense=INT32_MAX-1;boundary.line_income[3]=INT32_MIN+1;
        check(begin_payment(boundary,1,expected,error)&&expected["line_counts"][3]==INT32_MAX&&
              expected["record_expense"]==INT32_MAX&&expected["line_income"][3]==INT32_MIN,"exact signed category3 arithmetic boundary rejected");
        std::cout<<"checks="<<checks<<" status=PASS\n";
        return 0;
    }catch(const std::exception& x){std::cerr<<"checks="<<checks<<" error="<<x.what()<<'\n';return 1;}
}
