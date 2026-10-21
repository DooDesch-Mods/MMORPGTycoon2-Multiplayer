#include "monster_move_destination.h"
#include <iostream>
#include <map>
#include <stdexcept>
#include <type_traits>
namespace fixture {
struct Port {
    std::map<uintptr_t,uint64_t> values{{0x1170,0x2000},{0x1178,0x3000},{0x1490,0},{0x3070,0x2000},{0x2018,0},{0x4070,0x2000}};
    uintptr_t destination=0x4000,unreadable=0;
    bool locate_ok=true;unsigned original_queries=0;
    template<class T>bool read(uintptr_t base,size_t offset,T& out,std::string& error){
        const auto address=base+offset;const auto found=values.find(address);
        if(address==unreadable||found==values.end()){error="exact original field read cause";return false;}
        out=static_cast<T>(found->second);return true;
    }
    bool query(uintptr_t shard,const std::array<float,3>& point,uintptr_t& out,std::string& error){
        ++original_queries;if(shard!=0x2000||point!=std::array<float,3>{10,20,30})throw std::runtime_error("original lookup received another owner/pose");
        if(!locate_ok){error="exact original Shard lookup cause";return false;}out=destination;return true;
    }
    bool check(std::string& error){
        const auto at=[&](uintptr_t base,size_t offset,auto& out,std::string& cause){return read(base,offset,out,cause);};
        const auto locate=[&](uintptr_t shard,const std::array<float,3>& point,uintptr_t& out,std::string& cause){return query(shard,point,out,cause);};
        return mt2mp::game::monster_move_destination::check(0x1000,0x2000,0x3000,{10,20,30},at,locate,error);
    }
};
}
int main()try{using namespace fixture;unsigned checks=0;std::string error;
    const auto need=[&](bool ok,const char* cause){++checks;if(!ok)throw std::runtime_error(cause);};
    Port crossed;const auto original=crossed.values;
    need(crossed.check(error)&&crossed.original_queries==1,"Actual fixed owner rejected a different geographic Region");
    need(crossed.values==original,"Destination admission wrote original Region/master ownership");
    Port same;same.destination=0x3000;need(same.check(error),"Existing same-Region route changed");
    for(const auto address:{uintptr_t{0x1170},uintptr_t{0x1178},uintptr_t{0x1490},uintptr_t{0x3070},uintptr_t{0x2018},uintptr_t{0x4070}}){
        Port port;port.unreadable=address;need(!port.check(error)&&error=="exact original field read cause","Original read cause overwritten");
    }
    for(unsigned mutation=0;mutation<7;++mutation){Port port;
        if(mutation==0)port.values[0x1170]=0x5000;if(mutation==1)port.values[0x1178]=0x5000;
        if(mutation==2)port.values[0x3070]=0x5000;if(mutation==3)port.values[0x4070]=0x5000;
        if(mutation==4)port.values[0x1490]=1;if(mutation==5)port.values[0x1490]=2;if(mutation==6)port.destination=0;
        need(!port.check(error)&&!error.empty(),"Foreign or unsupported native owner admitted");
        need(port.original_queries==unsigned(mutation==3||mutation==6),"Rejected owner queried original destination");
    }
    Port lookup;lookup.locate_ok=false;need(!lookup.check(error)&&error=="exact original Shard lookup cause","Original locate error lost");
    Port dungeon;dungeon.values[0x2018]=0x6000;dungeon.values[0x64c8]=0;need(dungeon.check(error),"Idle original Dungeon template rejected");
    dungeon.values[0x64c8]=1;need(!dungeon.check(error)&&error.find("active Dungeon template")!=std::string::npos,"Original no-effect Dungeon guard bypassed");
    std::cout<<checks<<" actual fixed-owner destination admission cases passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
