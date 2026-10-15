#include "live_host_state/day_rollover/economy/codec.h"
#include <iostream>
#include <stdexcept>
namespace live=mt2mp::game::live_host_state;
namespace economy=live::day::economy;
namespace tutorial=mt2mp::game::tutorial_replication;
namespace {
void need(bool value,const std::string& cause){if(!value)throw std::runtime_error(cause);}
economy::State frame(){
    economy::State s;auto& f=s.day.frame;f.session=41;f.snapshot="11111111-1111-4111-8111-111111111111";
    f.epoch=7;f.source_generation=9;f.revision=3;f.selected=1;f.pause=1;
    f.values.calendar={1,1,2,1,30,7};f.values.record={1,93640,0,0,0};f.values.cash=93640;
    tutorial::State t;t.initial_complete=true;t.terminal.push_back({"initial_tutorial","finish",107,107,true,{"welcome","finish"},{}});
    tutorial::startingpoint::Row r;r.purchased=r.deployed=true;r.inventory_count=r.release_feature_count=r.release_change_count=1;r.release_counter=1;r.commit_bugs=tutorial::startingpoint::CommitBugs{};
    t.terminal_startingpoint=r;t.terminal_version=std::array<int32_t,3>{0,0,100};t.lifecycle_order={{{0,1},{2,0}}};
    t.active.push_back({"leveltwo_tutorial","welcome",17,1,false,{"welcome"},{}});
    f.tutorial=t;s.day.history={{1,live::day::record(f.values)}};return s;
}
bool decode(const nlohmann::json& raw,economy::State& out,std::string& e){const auto text=raw.dump();return economy::decode(reinterpret_cast<const uint8_t*>(text.data()),text.size(),out,e);}
}
int main(){try{
    auto s=frame();std::string e;std::vector<uint8_t> bytes;economy::State out;
    need(economy::encode(s,bytes,e),"complete Economy terminal encode: "+e);
    need(economy::decode(bytes.data(),bytes.size(),out,e)&&out==s,"complete Economy11/Day/HostState9 terminal roundtrip: "+e);
    std::cout<<"PASS complete frame retains coalesced Initial Commit/end and LevelTwo birth\n";
    auto raw=nlohmann::json::parse(bytes);raw["day"]["frame"]["tutorial"]["terminal_startingpoint"]["commit_bugs"]["known"]=true;
    need(!decode(raw,out,e)&&out==economy::State{},"malformed nested terminal Commit receipt accepted: "+e);
    std::cout<<"PASS malformed terminal Commit type rejects through actual full decoder\n";
    raw=nlohmann::json::parse(bytes);raw["day"]["frame"]["tutorial"]["lifecycle_order"][1][0]=1;
    need(!decode(raw,out,e)&&out==economy::State{},"colliding native lifecycle events accepted: "+e);
    std::cout<<"PASS duplicate event order rejects without partial full frame output\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
