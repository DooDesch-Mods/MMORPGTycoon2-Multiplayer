#include "mt2mp/net/session_binding.h"
#include <iostream>
#include <stdexcept>
using namespace mt2mp::net::session_binding;
unsigned checks=0;
void require(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
int main()try{
    Slots slots;std::string error;
    require(topology({55},55,slots,error)&&slots[1]==55&&mask(slots)==1,"solo Host topology rejected");
    require(topology({77,22,55},55,slots,error)&&slots==Slots{0,55,22,77,0},"physical slot mapping not canonical");
    const Binding current{100,1000,slots};
    auto raw=encode(current,"beta","game");Binding decoded;
    require(decode(raw.dump(),"beta","game",decoded,error)&&decoded==current,"binding roundtrip changed physical identity");
    require(compare(current,100,slots)==Change::Same,"unchanged lobby restarted");
    auto after=slots;after[3]=0;
    require(compare(current,100,after)==Change::Members,"departure failed to request a world barrier");
    after=slots;after[4]=99;
    require(compare(current,100,after)==Change::Members,"late join failed to request a world barrier");
    after[1]=77;after[3]=55;
    require(compare(current,100,after)==Change::HostLost,"Steam Host migration silently resumed simulation");
    require(compare(current,101,slots)==Change::LobbyLost,"foreign lobby retained authority");
    for(const auto& members:{std::vector<uint64_t>{},std::vector<uint64_t>{0,55},std::vector<uint64_t>{55,55},std::vector<uint64_t>{55,1,2,3,4}})
        require(!topology(members,55,slots,error)&&!error.empty(),"invalid physical topology admitted");
    const auto reject=[&](nlohmann::json value){return !decode(value.dump(),"beta","game",decoded,error)&&!error.empty();};
    for(const auto& bad:{nlohmann::json(-1),nlohmann::json(0),nlohmann::json(1.0),nlohmann::json(true),nlohmann::json("1000")}){
        auto value=raw;value["session"]=bad;require(reject(value),"invalid session epoch admitted");
    }
    auto value=raw;value["mod"]="other";require(reject(value),"mixed mod version admitted");
    value=raw;value["game"]="other";require(reject(value),"mixed executable admitted");
    value=raw;value["revision"]=1;require(reject(value),"old fixed-membership protocol admitted");
    value=raw;value["revision"]=2.0;require(reject(value),"floating-point protocol revision admitted");
    value=raw;value["slots"][0]=1;require(reject(value),"reserved slot admitted");
    value=raw;value["slots"][2]=55;require(reject(value),"duplicate physical sender admitted");
    value=raw;value["host"]=22;require(reject(value),"metadata Host differs from physical Host slot");
    value=raw;value["extra"]=1;require(reject(value),"unknown binding field admitted");
    require(!decode(std::string(2049,'x'),"beta","game",decoded,error),"oversized metadata admitted");
    std::cout<<checks<<" actual membership/epoch codec cases passed\n";return 0;
}catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}
