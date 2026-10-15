#include "tutorial_replication/staff_display.h"
#include <iostream>
#include <limits>
#include <stdexcept>
namespace display=mt2mp::game::tutorial_replication::staff_display;
using json=nlohmann::json;
namespace {
unsigned checks=0;
void need(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
void pass(const char* name){++checks;std::cout<<"PASS "<<name<<'\n';}
display::State mixed(){return {1,2,{{2,0,false},{9,6,false},{41,1,true}}};}
void codecs(){
    std::string e;display::State result;
    auto s=mixed();const auto raw=display::encode(s);
    need(display::decode(json::parse(raw.dump()),result,e)&&result==s,"fixed bounded display did not roundtrip");pass("actual codec preserves Host counts, stable IDs, jobs and on-map values");
    need(raw.at("developers")[1]==json::array({9,6,false}),"wire row changed fixed ID/job/boolean shape");pass("wire Developer row has the three fixed typed values");
    auto bad=raw;bad["developers"][1][0]=2;need(!display::decode(bad,result,e)&&e.find("IDs/order/job")!=std::string::npos,"duplicate stable ID accepted");pass("duplicate ID cannot enter presentation state");
    bad=raw;std::swap(bad["developers"][0],bad["developers"][2]);need(!display::decode(bad,result,e),"unstable Host order accepted");pass("wire IDs must be canonical increasing order");
    for(const auto& value:{json(-1),json(50),json(uint64_t(UINT64_MAX)),json(1.0),json(true)}){
        bad=raw;bad["developers"][0][0]=value;need(!display::decode(bad,result,e),"invalid Developer ID type/domain accepted");
    }pass("negative, out-of-range, oversized, float and boolean IDs reject");
    for(const auto& value:{json(-1),json(8),json(6.0),json(false)}){
        bad=raw;bad["developers"][1][1]=value;need(!display::decode(bad,result,e),"invalid job type/domain accepted");
    }pass("job encoding remains an exact bounded native integer");
    for(const auto& field:{"active_tech","active_developers"}){
        bad=raw;bad[field]=50;need(!display::decode(bad,result,e),"disagreeing native count accepted");
        bad=raw;bad[field]=true;need(!display::decode(bad,result,e),"boolean native count accepted");
    }pass("native summary counts must match actual per-Developer states");
    bad=raw;bad["developers"][0][2]=0;need(!display::decode(bad,result,e),"numeric on-map flag accepted");pass("on-map is an exact boolean");
    for(const auto& row:{json::array({2,0}),json::array({2,0,false,0}),json::object()}){
        bad=raw;bad["developers"][0]=row;need(!display::decode(bad,result,e),"malformed fixed row accepted");
    }pass("short, extended and object Developer rows reject");
    bad=raw;bad["extra"]=false;need(!display::decode(bad,result,e),"extra state key accepted");bad=raw;bad.erase("active_tech");need(!display::decode(bad,result,e),"missing native count accepted");pass("state schema rejects extra and missing keys");
    s={};for(int id=0;id<50;++id)s.developers.push_back({id,0,false});need(display::valid(s,e)&&display::decode(display::encode(s),result,e),"valid 50-Developer boundary rejected");s.developers.push_back({50,0,false});need(!display::valid(s,e),"unbounded display list admitted");pass("exact native population limit is enforced");
    s={};need(display::decode(display::encode(s),result,e)&&result==s,"empty display roundtrip failed");pass("empty native staff state remains valid");
}
struct Calls {
    std::vector<std::string> order;std::string failure;bool mismatch=false;
    std::vector<display::Developer> native_jobs{{2,0,false},{9,0,false},{41,0,false}};
    std::vector<display::Developer> immutable=native_jobs;
    bool step(const std::string& name,std::string& e){order.push_back(name);if(failure==name){e="original "+name;return false;}return true;}
    bool pin(const display::State&,std::string& e){return step("pin",e);}
    bool prepare(const display::State&,std::string& e){return step("prepare",e);}
    bool summary(int32_t count,std::string& e){need(count==1,"wrong active Developer count");return step("summary",e);}
    bool project(int32_t count,std::string& e){need(count==1,"wrong active Tech count");return step("project",e);}
    bool developer(const display::Developer& d,std::string& e){return step("developer:"+std::to_string(d.id),e);}
    bool readback(const display::State&,std::string& e){if(!step("readback",e))return false;if(mismatch){e="original native cell readback differs";return false;}return true;}
};
void rendering(){
    const display::State s{1,1,{{2,0,false},{9,6,false},{41,0,false}}};std::string e;bool attempted=false;
    Calls c;need(display::render(s,c,attempted,e)&&attempted,"actual render orchestration failed");
    need(c.order==std::vector<std::string>{"pin","prepare","summary","project","developer:2","developer:9","developer:41","readback"},"native UI setter/readback order changed");
    need(c.native_jobs==c.immutable,"display changed actual Client simulation jobs");pass("presentation invokes setters once in order and leaves native Client jobs unchanged");
    for(const auto& point:{"pin","prepare","summary","project","developer:2","developer:9","developer:41","readback"}){
        c={};e.clear();c.failure=point;attempted=true;need(!display::render(s,c,attempted,e)&&e==std::string("original ")+point,"original setter cause lost");
        const bool preflight=c.failure=="pin"||c.failure=="prepare";need(attempted!=preflight,"attempt receipt differs from mutation boundary");
        need(c.order.back()==point&&c.native_jobs==c.immutable,"failed setter continued or changed simulation");
        pass(point);
    }
    c={};c.mismatch=true;need(!display::render(s,c,attempted,e)&&attempted&&e=="original native cell readback differs","partial display falsely reported rendered");pass("native readback mismatch cannot produce rendered success");
    c={};auto bad=s;bad.developers[0].job=4;need(!display::render(bad,c,attempted,e)&&!attempted&&c.order==std::vector<std::string>{"pin"}&&e.find("formatter")!=std::string::npos,"unknown formatter reached setter");pass("unsupported activity format fails before any UI setters");
    c={};bad=s;bad.active_tech=0;need(!display::render(bad,c,attempted,e)&&!attempted&&c.order.empty(),"invalid display reached native owner read");pass("invalid target fails before native admission");
    need(std::string(display::project_key(true,0))=="-- {dev_tech_state_ready} --"&&std::string(display::project_key(true,1))=="-- {dev_tech_state_ready} --","completion precedence lost");
    need(std::string(display::project_key(false,0))=="-- {dev_tech_state_awaitingstaff} --"&&std::string(display::project_key(false,1))=="-- {dev_tech_state_implementing} --","working/waiting project labels differ");pass("native completion takes precedence over displayed staffing");
    need(std::string(display::activity_key({0,0,true}))=="{dev_state_idleonmap}"&&std::string(display::activity_key({0,0,false}))=="{dev_state_idle}","idle map state lost");pass("on-map and indoor idle keep distinct native labels");
}
}
int main(){try{codecs();rendering();std::cout<<checks<<" Staff display model/render checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
