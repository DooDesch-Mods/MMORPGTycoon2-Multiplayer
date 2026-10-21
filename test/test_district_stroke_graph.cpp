#include "district_stroke_transition.h"
#include <iostream>
#include <stdexcept>
namespace graph=mt2mp::game::district_stroke_graph;
namespace proof=mt2mp::game::district_stroke_proof;
namespace transition=mt2mp::game::district_stroke_transition;
namespace state=mt2mp::game::district_stroke_state;
namespace action=mt2mp::action_session;
using json=nlohmann::json;
namespace district_graph_fixture {
unsigned checks=0;
void need(bool value,const std::string& cause){++checks;if(!value)throw std::runtime_error(cause);}
struct Memory {
    std::map<uintptr_t,uint8_t> bytes;
    template<class T>void put(uintptr_t at,const T& value){const auto* data=reinterpret_cast<const uint8_t*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[at+i]=data[i];}
    void zero(uintptr_t at,size_t size){for(size_t i=0;i<size;++i)bytes[at+i]=0;}
    bool read(uintptr_t at,void* out,size_t size,std::string& error){auto* data=static_cast<uint8_t*>(out);
        for(size_t i=0;i<size;++i){auto found=bytes.find(at+i);if(found==bytes.end()){error="fixture missing address "+std::to_string(at+i);return false;}data[i]=found->second;}return true;}
};
constexpr uintptr_t image=0x140000000,root=0x100000,region=0x200000,graph_pointer=0x300000,node_base=0x400000;
constexpr uint64_t uid=0x200000007;
const std::string operation="11111111-1111-4111-8111-111111111111",key=std::string(64,'a')+":world:district:17";
void array(Memory& memory,uintptr_t object,uintptr_t ap,uintptr_t data,int32_t count){
    memory.put(object,image+ap);memory.put(object+8,data);memory.put(object+16,count);memory.put(object+20,count);
}
void node(Memory& memory,size_t index,uintptr_t ap,uintptr_t method){
    const auto object=node_base+index*0x1000;memory.zero(object,0x1f8);memory.put(object,image+ap);memory.put(image+ap+0x10,image+method);
    memory.put(object+0xb8,uid);
    for(size_t set=0;set<3;++set){const auto slot=object+8+set*0x28,table=object+0x400+set*0x80;
        memory.put(slot,table);memory.put(slot+8,int32_t(2));memory.put(slot+0xc,int32_t(31));memory.put(slot+0x18,table+0x40);memory.zero(table,48);}
    if(ap==0x17132b0||ap==0x17132f0){memory.put(object+0xc0,region);memory.put(object+0xc8,root);
        memory.put(object+0xf0,object+0x800);memory.zero(object+0x800,24);
        memory.put(object+0x160,object+0x900);memory.put(object+0x180,object+0x900);}
}
Memory fixture(){
    Memory memory;memory.zero(graph_pointer,0x110);
    array(memory,graph_pointer,0x16e32d0,0,0);array(memory,graph_pointer+0x18,0x16e2dc0,graph_pointer+0x200,2);
    array(memory,graph_pointer+0x30,0x1748aa0,graph_pointer+0x300,2);array(memory,graph_pointer+0x48,0x1748c50,0,0);
    memory.put(graph_pointer+0x10c,uint8_t(1));node(memory,0,0x17132f0,0x549fc0);node(memory,1,0x16de860,0x53e1c0);
    for(size_t i=0;i<2;++i){memory.put(graph_pointer+0x200+i*8,node_base+i*0x1000);memory.put(graph_pointer+0x300+i*8,node_base+i*0x1000);}
    return memory;
}
}
int main()try{
    using namespace district_graph_fixture;
    std::string error;auto memory=fixture();graph::Graph original;
    need(graph::read(memory,image,graph_pointer,original,error),error);
    need(original.nodes.size()==2&&original.order==std::vector<size_t>{0,1}&&original.nodes[0].stored.uid==uid,"complete original graph/order/full UID lost");
    std::map<uint64_t,std::string> keys;json canonical;
    auto absent=[](uint64_t,uintptr_t& object,std::string&){object=0;return true;};
    need(proof::graph(original,operation,root,{{region,17}},{},absent,canonical,keys,error),error);
    need(keys.at(uid)=="district-transient:"+operation+":0","transient identity is not scoped to original operation");
    need(transition::restores(canonical,{},{},error),error);
    auto reversed=canonical;std::reverse(reversed.begin(),reversed.end());need(!transition::restores(reversed,{},{},error),"wrong native execution order accepted");
    auto alive=[](uint64_t,uintptr_t& object,std::string&){object=0x777000;return true;};
    need(!proof::graph(original,operation,root,{{region,17}},{},alive,canonical,keys,error),"unknown live transient UID admitted");
    need(!proof::graph(original,operation,root+8,{{region,17}},{},absent,canonical,keys,error),"foreign Root accepted");
    need(!proof::graph(original,operation,root,{{region+8,17}},{},absent,canonical,keys,error),"foreign Region accepted");
    auto malformed=[&](auto change){auto copy=fixture();change(copy);graph::Graph result;error.clear();need(!graph::read(copy,image,graph_pointer,result,error)&&!error.empty(),"invalid complete native graph admitted");};
    malformed([](auto& copy){copy.put(graph_pointer+0x10c,uint8_t(0));});
    malformed([](auto& copy){copy.put(graph_pointer+0x40,int32_t(1));});
    malformed([](auto& copy){copy.put(graph_pointer+0x308,node_base);});
    malformed([](auto& copy){copy.put(graph_pointer+0x208,node_base);});
    malformed([](auto& copy){copy.put(node_base,image+0x1713330);});
    malformed([](auto& copy){copy.put(image+0x17132f0+0x10,image+0x549fc1);});
    malformed([](auto& copy){copy.put(node_base+0xb8,uint64_t(0));});
    malformed([](auto& copy){copy.put(node_base+0x1bc,uint8_t(2));});
    malformed([](auto& copy){copy.put(node_base+0x1c4,uint32_t(0x7fc00000));});
    malformed([](auto& copy){copy.put(node_base+8+0xc,int32_t(32));});
    malformed([](auto& copy){copy.put(node_base+8+0x20,int32_t(1));});
    // A retained canonical owner uses its complete UID, not its low 32 bits.
    memory=fixture();need(graph::read(memory,image,graph_pointer,original,error),error);
    need(proof::graph(original,operation,root,{{region,17}},{{uid,key}},absent,canonical,keys,error)&&keys.at(uid)==key,error);
    need(proof::graph(original,operation,root,{{region,17}},{{uid&0xffffffffULL,key}},absent,canonical,keys,error)&&keys.at(uid)!=key,"truncated UID selected an existing alias");
    const auto stored=proof::stored(original.nodes[0].stored,key,17);json edit=json::array({json::array({0x17132b0,stored,json::array()})});
    auto changed=stored;changed[2]=1;
    need(transition::restores(edit,{{key,changed}},{{key,stored}},error),error);
    need(!transition::restores(edit,{},{{key,stored}},error),"Edit of missing target accepted");
    auto wrong=stored;wrong[5]="changed description";need(!transition::restores(edit,{{key,changed}},{{key,wrong}},error),"inverse payload drift accepted");
    json cash=json::array({json::array({0x1786fb0,json::array({1,std::array<uint32_t,3>{}}),json::array()})});
    need(!transition::restores(cash,{},{},error),"nonzero District refund accepted");
    action::DistrictState first;first.region=17;action::DistrictState other;other.region=18;
    state::Values before{{key,first},{"unrelated",other}},after=before;after[key].geo=2;
    need(state::changed_region(before,after,17,error),error);after["unrelated"].name="foreign";
    need(!state::changed_region(before,after,17,error),"unrelated Region changed");
    std::cout<<checks<<" District original mixed graph and canonical inverse checks passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
