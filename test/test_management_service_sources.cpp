#include "management_service_sources.h"
#include "service_membership_fixture.h"
#include <iostream>

int main(){
    namespace g=mt2mp::game::management_service_graph;using namespace service_fixture;
    try{
        auto memory=service_fixture::memory();const uintptr_t shard=0x9000,region=0x9100;const uint64_t uid=77;
        memory.put(building+0x18,uid);memory.put(building+0x170,shard);memory.put(building+0x178,region);
        auto text=[&](uintptr_t p,std::string& value,std::string& error){return memory.text(p,value,error);};
        auto destination=[](uintptr_t,std::string&,std::string& error){error="fixture unexpected destination";return false;};
        auto find=[&](uint64_t value,uintptr_t& result,std::string& error){if(value!=uid){error="fixture UID absent";return false;}result=building;return true;};
        std::map<std::string,g::SourceBuilding> states;std::vector<g::ServiceSourcePin> pins;std::string error;
        std::vector<g::BuildingSource> owners{{building,shard,region,uid,"building:1"}};
        need(g::source_services(memory,text,destination,find,image,shard,owners,states,pins,error),error);
        need(states.at("building:1").services.rows.size()==3&&pins.size()==1,"full actual source Service rows absent");
        memory.put(building+0x170,uintptr_t(0x9200));
        need(!g::source_services(memory,text,destination,find,image,shard,owners,states,pins,error)&&error.find("reciprocal")!=std::string::npos,"foreign owner was accepted");
        std::cout<<"actual Service source reader passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
