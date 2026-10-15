#include "live_host_state/day_rollover/economy/state_difference.h"
#include <iostream>
#include <stdexcept>
namespace live=mt2mp::game::live_host_state;
namespace economy=live::day::economy;
namespace quest=mt2mp::subscriber_quest_state;
using namespace economy;
namespace {
unsigned checks=0;
void need(bool v,const std::string& e){if(!v)throw std::runtime_error(e);++checks;}
State baseline(){
    State s;auto& f=s.day.frame;f.session=41;f.snapshot="11111111-1111-4111-8111-111111111111";f.epoch=7;f.source_generation=9;f.revision=3;f.selected=1;f.pause=1;
    f.values.calendar={1,1,2,1,30,7};f.values.record={1,93640,0,0,0};f.values.cash=93640;s.day.history={{1,live::day::record(f.values)}};
    quest::Npc n;n.key="action:b66a1a82-3d84-4ed0-a2cd-8cd8133d1941";n.level=11;n.pose.region=67;n.pose.orientation={0,0,0,1};n.pose.position={8076.98876953125f,70.64453125f,8574.509765625f};n.pose.scale={1,1,1};n.quests.push_back({0,{},{},{}});s.npcs.push_back(n);
    Subscriber a;a.serialized_id=56;s.subscribers.push_back(a);return s;
}
struct Mock final:Port{
    Owners value;unsigned reads=0,stores=0;bool drift=false;
    bool read(Owners& out,std::string&)override{++reads;out=value;return true;}
    bool store_subscriber(const Owners&,size_t i,size_t f,int32_t old,int32_t after,std::string& e)override{
        if(f>=5||value.state.subscribers[i].pay[f]!=old){e="original test CAS failure";return false;}
        ++stores;value.state.subscribers[i].pay[f]=after;if(drift)value.state.npcs[0].pose.position[0]+=1;return true;
    }
    bool store_loan(const Owners&,size_t,size_t,int32_t,int32_t,std::string& e)override{e="unexpected Loan store";return false;}
};
}
int main(){try{
    const auto base=baseline();
    auto change=[&](auto mutate,const std::string& field){auto actual=base;mutate(actual.npcs);auto why=state_difference(actual,base);need(why.starts_with("NPC pose/counters differ: ")&&why.find(field)!=std::string::npos&&why.find(" actual=")!=std::string::npos&&why.find(" required=")!=std::string::npos,field+": "+why);};
    change([](auto& n){n.clear();},"npc.count");change([](auto& n){n[0].key="foreign";},".key");change([](auto& n){n[0].type=1;},".type");change([](auto& n){n[0].level=12;},".level");
    change([](auto& n){n[0].pose.region=68;},"pose.region");change([](auto& n){n[0].pose.position[0]+=1;},"pose.position");change([](auto& n){n[0].pose.orientation[1]=1;},"pose.orientation");change([](auto& n){n[0].pose.scale[0]=2;},"pose.scale");
    change([](auto& n){n[0].accepted.today=1;},"accepted.today");change([](auto& n){n[0].accepted.total=1;},"accepted.total");change([](auto& n){n[0].accepted.history={1};},"accepted.history.count");
    auto a=base,b=base;a.npcs[0].accepted.history={1};b.npcs[0].accepted.history={2};need(state_difference(a,b).find("accepted.history[0] actual=1 required=2")!=std::string::npos,"history value lost");
    change([](auto& n){n[0].completed.today=2;},"completed.today");change([](auto& n){n[0].highest_cache[0]=2;},"highest_cache");change([](auto& n){n[0].quests.clear();},"quests.count");change([](auto& n){n[0].quests[0].sequence_id=1;},"sequence_id");change([](auto& n){n[0].quests[0].accepted.total=3;},"quest[0].accepted.total");change([](auto& n){n[0].quests[0].completed.today=4;},"quest[0].completed.today");change([](auto& n){n[0].quests[0].highest_cache[1]=4;},"quest[0].highest_cache");
    Mock port;port.value.state=base;port.value.history.pin.context.role=mt2mp::action_session::Role::Client;port.value.subscribers={1};port.value.subscriber_slots={0};port.value.needs_owners.resize(1);
    auto target=base;target.subscribers[0].pay[0]=1;const auto before=port.value;Apply apply;port.drift=true;
    const auto receipt=apply.run(port,before,target);need(receipt.attempted&&receipt.unknown&&!receipt.confirmed&&receipt.stores==1&&receipt.cause.find("pose.position")!=std::string::npos,"actual Apply lost Unknown or detailed NPC cause: "+receipt.cause);
    const auto reads=port.reads,stores=port.stores;const auto again=apply.run(port,before,target);need(again.cause==receipt.cause&&port.reads==reads&&port.stores==stores,"Unknown retried mutation or read");
    Mock stable;stable.value=before;Apply stable_apply;const auto okay=stable_apply.run(stable,before,target);need(okay.confirmed&&!okay.unknown&&stable.stores==1,"unchanged successful apply regressed: "+okay.cause);
    std::cout<<"PASS "<<checks<<" actual NPC difference/Apply cases\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

