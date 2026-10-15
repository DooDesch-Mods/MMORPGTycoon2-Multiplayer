#include "vendor/nlohmann/json.hpp"
#include "native_open_runtime.h"
#include "tutorial_startingpoint_fixture/controller_admission.h"
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
        Pin baseline;baseline.development=native_baseline;baseline.project=1;
        baseline.player=10;baseline.inventory=20;baseline.slots=30;baseline.items[0]=40;
        std::string error;
        check(before(baseline,error),"retained native zero-bug baseline rejected");
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
        std::cout<<"checks="<<checks<<" status=PASS\n";
        return 0;
    }catch(const std::exception& x){std::cerr<<"checks="<<checks<<" error="<<x.what()<<'\n';return 1;}
}
