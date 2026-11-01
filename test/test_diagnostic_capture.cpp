#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <iostream>
#include <string>
#include <vector>
using json=nlohmann::json;
namespace live_host_state::day::economy {
struct Frame{uint64_t revision=0;};
struct State{struct Day{Frame frame;}day;int cash=0;};
bool encode_ok=true;unsigned encodes=0;
bool encode(const State& state,std::vector<uint8_t>& bytes,std::string& error){
    ++encodes;if(!encode_ok){error="original Economy encode failure";return false;}
    const auto text=json{{"cash",state.cash},{"revision",state.day.frame.revision}}.dump();
    bytes.assign(text.begin(),text.end());return true;
}
}
using State=live_host_state::day::economy::State;
using Frame=live_host_state::day::economy::Frame;
struct Economy{unsigned reads=0;bool read_ok=true;State current{{{9}},222};
    bool read_current(const Frame&,State& out,std::string& error){++reads;if(!read_ok){error="original owner changed";return false;}out=current;return true;}};
struct Live{std::optional<Frame> accepted=Frame{7};std::optional<State> committed=State{{{7}},111};
    const auto& last(){return accepted;}const State* committed_economy(){return committed?&*committed:nullptr;}};
struct Runtime{Economy* economy_state;Live* live_state;};
json capture(Runtime* runtime,bool inspect_native_economy){
#include "economy_status_body.inc"
    return {{"state",native_economy},{"error",economy_read_error},{"source",economy_source},{"revision",economy_revision}};
}
unsigned checks=0;
void check(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
int main(){try{
    Economy economy;Live live;Runtime runtime{&economy,&live};
    auto result=capture(&runtime,false);
    check(economy.reads==0&&result["state"]["cash"]==111&&result["source"]=="committed_frame"&&result["revision"]==7,"periodic status captured native state or mislabeled retained revision");
    economy.current.cash=333;
    result=capture(&runtime,true);
    check(economy.reads==1&&result["state"]["cash"]==333&&result["source"]=="current_native"&&result["revision"]==9,"explicit inspection reused stale committed Economy");
    economy.read_ok=false;result=capture(&runtime,true);
    check(economy.reads==2&&result["state"].empty()&&result["error"]=="original owner changed"&&result["source"]=="current_native","failed explicit inspection hid original cause behind committed data");
    result=capture(&runtime,false);
    check(economy.reads==2&&result["state"]["cash"]==111&&result["source"]=="committed_frame","routine status performed native retry after failed inspection");
    live.committed.reset();result=capture(&runtime,false);
    check(economy.reads==2&&result["state"].empty()&&result["source"]=="unavailable"&&result["revision"]==0,"missing committed state fabricated a native sample");
    live.committed=State{{{12}},444};result=capture(&runtime,false);
    check(result["state"]["cash"]==444&&result["revision"]==12,"routine status did not follow newly committed frame");
    live_host_state::day::economy::encode_ok=false;result=capture(&runtime,false);
    check(result["state"].empty()&&result["error"]=="original Economy encode failure"&&result["revision"]==12,"committed encoding failure was hidden");
    live_host_state::day::economy::encode_ok=true;live.accepted.reset();const auto encodes=live_host_state::day::economy::encodes;
    result=capture(&runtime,false);
    check(economy.reads==2&&live_host_state::day::economy::encodes==encodes&&result["source"]=="unavailable","reset world exposed retired committed state");
    runtime.live_state=nullptr;result=capture(&runtime,true);
    check(economy.reads==2&&result["source"]=="unavailable","absent live state was inspected");
    runtime.live_state=&live;runtime.economy_state=nullptr;result=capture(&runtime,false);
    check(economy.reads==2&&result["source"]=="unavailable","unbound Economy was reported as committed");
    std::cout<<checks<<" actual diagnostic capture cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
