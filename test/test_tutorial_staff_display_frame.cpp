#include "live_host_state/day_rollover/economy/codec.h"
#include <iostream>
#include <stdexcept>
namespace live=mt2mp::game::live_host_state;
namespace economy=live::day::economy;
namespace tutorial=mt2mp::game::tutorial_replication;
namespace {
void need(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
economy::State frame(){
    economy::State s;auto& f=s.day.frame;
    f.session=41;f.snapshot="11111111-1111-4111-8111-111111111111";
    f.epoch=7;f.source_generation=9;f.revision=3;f.selected=1;f.pause=1;
    f.values.calendar={1,1,2,1,30,7};f.values.record={1,93640,0,0,0};f.values.cash=93640;
    tutorial::State t;t.active.push_back({"initial_tutorial","startingpoint",124,93,false,{"welcome"},{}});
    tutorial::startingpoint::Row project;project.project_present=project.purchased=true;project.work=78;project.lines=40;project.desired_workers=1;
    t.startingpoint=project;t.staff_display=tutorial::staff_display::State{1,1,{{7,6,false}}};
    f.tutorial=t;s.day.history={{1,live::day::record(f.values)}};return s;
}
bool decode(const nlohmann::json& raw,economy::State& out,std::string& e){
    const auto bytes=raw.dump();return economy::decode(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size(),out,e);
}
}
int main(){try{
    const auto state=frame();std::string e;std::vector<uint8_t> bytes;economy::State actual;
    need(economy::encode(state,bytes,e),"actual complete Economy encode: "+e);
    need(economy::decode(bytes.data(),bytes.size(),actual,e),"actual complete Economy/Day/HostState/Tutorial decode: "+e);
    need(actual==state,"complete Channel9 payload lost Staff display or existing fields");
    std::cout<<"PASS complete Economy11/Day/HostState9/Tutorial Staff display roundtrip\n";
    auto raw=nlohmann::json::parse(bytes);
    raw["day"]["frame"]["tutorial"]["staff_display"]["developers"][0][0]=-1;
    need(!decode(raw,actual,e)&&actual==economy::State{}&&e.find("Staff display")!=std::string::npos,
        "malformed Staff ID bypassed the real complete-frame decoder or lost its cause: "+e);
    std::cout<<"PASS malformed Staff payload rejects through complete decoder without partial output\n";
    auto old=tutorial::encode(*state.day.frame.tutorial);old.erase("staff_display");tutorial::State legacy;
    need(tutorial::decode(old,legacy,e)&&!legacy.staff_display&&legacy.startingpoint==state.day.frame.tutorial->startingpoint,
        "legacy tutorial receipt fabricated Staff data or lost Startingpoint: "+e);
    std::cout<<"PASS legacy tutorial state has no invented Staff presentation\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
