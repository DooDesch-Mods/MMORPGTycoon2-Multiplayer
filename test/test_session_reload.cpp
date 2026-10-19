#include "session_reload/protocol.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::game::session_reload;
unsigned checks=0;
void need(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
int main()try{
    const Packet begin{Kind::Begin,42,7,"11111111-1111-4111-8111-111111111111",{}};
    std::string error;Packet decoded;
    const auto data=encode(begin).dump();
    need(decode(reinterpret_cast<const uint8_t*>(data.data()),uint32_t(data.size()),decoded,error)&&decoded==begin,"reload packet changed identity");
    Barrier barrier;
    need(barrier.begin(begin,1,error)&&!barrier.complete()&&barrier.pending()==7,"barrier fabricated native Title readiness");
    const auto ready=barrier.ready_packet();
    need(barrier.acknowledge(1,ready,error)&&barrier.pending()==6,"Host native Title acknowledgement lost");
    need(barrier.acknowledge(2,ready,error)&&barrier.pending()==4,"Client Title acknowledgement lost");
    need(barrier.acknowledge(2,ready,error)&&barrier.pending()==4,"duplicate Ready changed quorum");
    auto foreign=ready;foreign.session=43;
    need(!barrier.acknowledge(3,foreign,error)&&!barrier.complete(),"foreign session made reload ready");
    foreign=ready;foreign.token="22222222-2222-4222-8222-222222222222";
    need(!barrier.acknowledge(3,foreign,error),"retired barrier token admitted");
    need(!barrier.acknowledge(4,ready,error),"unadmitted physical peer made reload ready");
    need(barrier.begin(begin,1,error)&&barrier.pending()==4,"duplicate Begin reset ready acknowledgements");
    foreign=begin;foreign.token="22222222-2222-4222-8222-222222222222";
    need(!barrier.begin(foreign,1,error)&&barrier.pending()==4,"second Begin replaced an active world operation");
    need(barrier.retain_members(3,error)&&barrier.complete(),"departed remote peer retained a phantom acknowledgement requirement");
    need(!barrier.retain_members(2,error),"Host migration silently continued reload");
    barrier.clear();need(!barrier.complete()&&!barrier.active(),"cleared barrier retained authority");
    auto solo=begin;solo.required=1;
    need(barrier.begin(solo,1,error)&&!barrier.complete(),"solo Host barrier completed before native Title");
    need(barrier.acknowledge(1,barrier.ready_packet(),error)&&barrier.complete(),"solo Host native retirement did not complete barrier");
    for(const auto& bad:{nlohmann::json(-1),nlohmann::json(1.0),nlohmann::json(true),nlohmann::json("1")}){
        auto value=encode(begin);value["kind"]=bad;const auto raw=value.dump();
        need(!decode(reinterpret_cast<const uint8_t*>(raw.data()),uint32_t(raw.size()),decoded,error),"untyped command admitted");
    }
    auto value=encode(begin);value["required"]=2;auto raw=value.dump();
    need(!decode(reinterpret_cast<const uint8_t*>(raw.data()),uint32_t(raw.size()),decoded,error),"Host absent quorum admitted");
    value=encode(begin);value["sender"]=2;raw=value.dump();
    need(!decode(reinterpret_cast<const uint8_t*>(raw.data()),uint32_t(raw.size()),decoded,error),"payload sender override admitted");
    std::cout<<checks<<" shared reload codec/native-retirement barrier cases passed\n";return 0;
}catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}
