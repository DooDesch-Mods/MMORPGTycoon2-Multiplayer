#include "monster_zone_read.h"
#include "mt2mp/core/hash.h"
#include "vendor/nlohmann/json.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace fixture {
using nlohmann::json;namespace monster_zone_read=mt2mp::game::monster_zone_read;
struct Memory {
    int process=0;std::map<uintptr_t,unsigned char> bytes;std::map<uintptr_t,std::string> strings;uintptr_t unreadable=0;
    template<class T>void set(uintptr_t address,const T& value){const auto raw=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=raw[i];}
    void zero(uintptr_t address,size_t size){for(size_t i=0;i<size;++i)bytes[address+i]=0;}
    bool copy(uintptr_t address,void* out,size_t size,std::string& error){auto raw=static_cast<unsigned char*>(out);
        for(size_t i=0;i<size;++i){const auto found=bytes.find(address+i);if(address+i==unreadable||found==bytes.end()){error="original memory read failure at "+std::to_string(address+i);return false;}raw[i]=found->second;}return true;}
};
template<class T>bool field(Memory& memory,uintptr_t base,size_t offset,T& out,std::string& error){return memory.copy(base+offset,&out,sizeof out,error);}
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
struct Graph {uintptr_t pointer=0;std::vector<std::pair<uintptr_t,std::string>> nodes;bool operator==(const Graph&)const=default;};
struct State {uintptr_t image=0x140000000;} state;
namespace undo_probe {
struct GraphWitness {bool locked=true;int32_t nodes=0,operations=1,pending_delete=0;};
GraphWitness current;
bool read_graph(int,uintptr_t,uintptr_t,GraphWitness& out,std::string&){out=current;return true;}
}
namespace npc_quest_records {
bool read_string(Memory& memory,uintptr_t object,size_t offset,const char*,std::string& out,std::string& error){
    const auto found=memory.strings.find(object+offset);if(found==memory.strings.end())return fail(error,"original string read failure");out=found->second;return true;}
}
namespace quest_bridge {
bool sha256(const std::string& bytes,std::string& out,std::string&){out=mt2mp::sha256(std::vector<unsigned char>(bytes.begin(),bytes.end()));return true;}
}
#include "monster_history_graph_body.inc"
#include "monster_history_others_body.inc"
void setup(Memory& memory,uintptr_t ap,uintptr_t method){
    memory={};undo_probe::current={};memory.zero(0x1000,0x40);memory.set(0x1020,uintptr_t{0x2000});memory.set(0x2000,uintptr_t{0x3000});
    memory.zero(0x3000,0x178);memory.set(0x3000,state.image+ap);memory.set(state.image+ap+0x10,state.image+method);memory.set(0x30b8,uint64_t{0x100000002});
    memory.strings[0x30c0]="Original name";
}
}
int main()try{using namespace fixture;unsigned checks=0;std::string error;Memory memory;Graph first,second;
    const auto need=[&](bool ok,const std::string& message){++checks;if(!ok)throw std::runtime_error(message);};
    for(const auto [ap,method]:{std::pair<uintptr_t,uintptr_t>{0x1786ff0,0x55ed80},{0x1706550,0x5507c0},{0x16de860,0x53e1c0}}){
        setup(memory,ap,method);need(graph(memory,0x1000,0x100000002,ap==0x1786ff0,first,error),error);
        need(graph(memory,0x1000,0x100000002,ap==0x1786ff0,second,error)&&first==second,"Stable graph fingerprint changed");
        memory.bytes[0x30bc]^=1;need(!graph(memory,0x1000,0x100000002,ap==0x1786ff0,second,error),"Foreign full UID accepted");
        setup(memory,ap,method);memory.unreadable=0x30b8;need(!graph(memory,0x1000,0x100000002,ap==0x1786ff0,second,error)&&error.starts_with("original memory read failure"),"Graph replaced original memory error");
    }
    setup(memory,0x1706550,0x5507c0);need(graph(memory,0x1000,0x100000002,false,first,error),error);
    memory.strings[0x30c0]="Changed heap string";need(graph(memory,0x1000,0x100000002,false,second,error)&&first!=second,"Retained Make name mutation invisible");
    for(unsigned mutation=0;mutation<5;++mutation){setup(memory,0x1706550,0x5507c0);
        if(mutation==0)undo_probe::current.locked=false;if(mutation==1)undo_probe::current.pending_delete=1;
        if(mutation==2)memory.set(0x2000,uintptr_t{0});if(mutation==3)memory.set(0x3000,uintptr_t{state.image+0x1786ff0});
        if(mutation==4)memory.set(0x30b8,uint64_t{0});
        error.clear();need(!graph(memory,0x1000,0x100000002,false,second,error)&&!error.empty(),"Invalid original graph lacked visible failure");
    }
    monster_zone_read::State before,after;before.manager=after.manager=10;before.monster_manager=after.monster_manager=20;
    monster_zone_read::Monster removed,survivor;removed.pointer=30;removed.uid=3;survivor.pointer=40;survivor.uid=4;
    before.monsters={removed,survivor};after.monsters={survivor};
    need(protected_others(before,after,3,false,true,error),"Original Remove survivor order rejected");
    need(protected_others(after,before,3,true,false,error),"Original restored receiver rejected");
    ++after.monsters[0].level;need(!protected_others(before,after,3,false,true,error),"Another Monster changed during history");
    after=before;after.monsters[0].position={1,2,3};need(protected_others(before,after,3,false,false,error),"Move target pose should be checked by actual history transition");
    after.monsters.push_back(removed);need(!protected_others(before,after,3,false,false,error),"Duplicate target member accepted");
    std::cout<<checks<<" actual native history graph/lifetime cases passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
