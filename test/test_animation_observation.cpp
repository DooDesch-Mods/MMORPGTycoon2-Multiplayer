#include "toon_weapon_visual/animation_observation.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>

namespace animation=mt2mp::game::toon_weapon_visual::animation_observation;
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;
    template<class T>void put(uintptr_t address,const T& value){const auto* data=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=data[i];}
    template<class T>bool read(uintptr_t base,size_t offset,T& value,std::string& error){
        auto* data=reinterpret_cast<unsigned char*>(&value);
        for(size_t i=0;i<sizeof value;++i){const auto item=bytes.find(base+offset+i);if(item==bytes.end()){error="missing native field";return false;}data[i]=item->second;}
        return true;
    }
};
int main(){
    unsigned checks=0;const auto need=[&](bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);};
    constexpr uintptr_t character=0x10000,actor=0x20000,catalog=0x30000,active=0x40000,first=0x50000,second=0x60000,def1=0x70000,def2=0x80000;
    Memory memory;std::string error;animation::State state;
    auto read=[&](uintptr_t p,size_t n,auto& out,std::string& cause){return memory.read(p,n,out,cause);};
    auto name=[&](uintptr_t p,std::string& out,std::string&){out=p==def1?"walk":"idle";return true;};
    auto setup=[&]{memory.bytes.clear();error.clear();memory.put(actor+0x18,catalog);memory.put(actor+0x20,int32_t(2));memory.put(actor+0x24,int32_t(2));
        memory.put(actor+0x30,active);memory.put(actor+0x38,int32_t(2));memory.put(actor+0x3c,int32_t(2));
        memory.put(catalog,first);memory.put(catalog+8,second);memory.put(active,first);memory.put(active+8,second);
        for(size_t i=0;i<49;++i)memory.put(character+8+i*4,int32_t(-1));
        memory.put(character+8,int32_t(0));memory.put(character+12,int32_t(0));memory.put(character+16,int32_t(1));
        for(auto [instance,definition]:{std::pair{first,def1},std::pair{second,def2}}){memory.put(instance,definition);
            memory.put(instance+8,std::bit_cast<uint32_t>(5.0f));memory.put(instance+0xc,std::bit_cast<uint32_t>(-1.0f));
            memory.put(instance+0x10,uint8_t(1));memory.put(instance+0x11,uint8_t(0));memory.put(instance+0x12,uint8_t(1));
            memory.put(definition+0x68,int32_t(2));memory.put(definition+0x6c,std::bit_cast<uint32_t>(1.0f));}};
    auto sample=[&]{return animation::sample(read,name,character,actor,state,error);};
    try{
        setup();need(sample()&&state.active.size()==2&&state.effective_index==1&&state.active[0].semantic_aliases==std::vector<int32_t>{0,1}&&state.active[0].name=="walk","active order, aliases or winner differ");
        need(std::bit_cast<float>(state.active[0].time_bits)>std::bit_cast<float>(state.active[0].duration_bits),"native overshoot was rejected");
        setup();memory.put(second+0x11,uint8_t(1));need(sample()&&state.effective_index==1&&state.active[1].paused,"paused playing winner lost");
        setup();memory.put(second+0x10,uint8_t(0));need(sample()&&state.effective_index==0,"stopped final entry selected");
        setup();memory.put(character+8,int32_t(2));need(!sample()&&!error.empty(),"out-of-bounds semantic accepted");
        setup();memory.put(active+8,uintptr_t(0x90000));need(!sample()&&!error.empty(),"detached active instance accepted");
        setup();memory.put(active+8,first);need(!sample()&&!error.empty(),"duplicate active instance accepted");
        setup();memory.put(first+0x12,uint8_t(2));need(!sample()&&!error.empty(),"invalid direction accepted");
        setup();memory.put(def1+0x68,int32_t(4));need(!sample()&&!error.empty(),"invalid mode accepted");
        setup();memory.put(first+8,uint32_t(0x7fc00000));need(!sample()&&!error.empty(),"nonfinite time accepted");
        std::cout<<checks<<" actual animation observation checks passed\n";return 0;
    }catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
}
