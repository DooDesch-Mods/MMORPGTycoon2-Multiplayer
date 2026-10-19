#include "boss_phase_reader.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace read=mt2mp::game::boss_phase_reader;
using mt2mp::action_session::BossTypePhases;
void need(bool ok,const std::string& e){if(!ok)throw std::runtime_error(e);}
struct Memory {
    std::map<uintptr_t,std::vector<uint8_t>> fields;std::map<uintptr_t,std::string> strings;
    template<class T>void put(uintptr_t address,const T& value){std::vector<uint8_t> bytes(sizeof value);std::memcpy(bytes.data(),&value,sizeof value);fields[address]=std::move(bytes);}
    template<class T>bool operator()(uintptr_t base,size_t offset,T& value,std::string& e){auto i=fields.find(base+offset);if(i==fields.end()||i->second.size()!=sizeof value){e="fixture missing exact field";return false;}std::memcpy(&value,i->second.data(),sizeof value);return true;}
    void array(uintptr_t object,size_t offset,uintptr_t data,int32_t count){put(object+offset+8,data);put(object+offset+16,count);put(object+offset+20,count+2);}
};
Memory fixture(){Memory m;const uintptr_t image=0x140000000,object=0x10000;
    m.put(object,image+0x1705990);m.put(object+0x18,uint64_t(0x100000042));m.put(object+0x2c,int32_t(3));m.put(object+0x30,int32_t(0));
    m.put(object+0x4a0,uintptr_t(0));m.put(object+0x490,uint8_t(0));m.put(object+0x491,uint8_t(0));m.put(object+0x492,uint8_t(1));m.put(object+0x2e0,int32_t(2));
    m.strings[object+0xb0]="Boss";m.strings[object+0x248]="definition";m.array(object,0x128,0x20000,2);m.array(object,0x158,0x30000,2);
    for(int32_t i=0;i<2;++i){const auto ability=uintptr_t(0x40000+i*0x1000),phase=uintptr_t(0x60000+i*0x1000),data=uintptr_t(0x80000+i*0x1000),element=uintptr_t(0xa0000+i*0x1000);
        m.put(0x20000+size_t(i)*8,ability);m.put(ability,image+0x171c930);m.strings[ability+0x48]="Attack";m.put(0x30000+size_t(i)*8,phase);m.put(phase,image+0x16ddb00);
        m.put(phase+0x5c,i);m.put(phase+0x58,int32_t(20+i));m.put(phase+0x80,std::array<float,4>{1,0,0,1});m.strings[phase+0x38]=i?"Second":"First";m.strings[phase+0x60]="fire";
        m.array(phase,0x90,data,1);m.put(data,element);m.put(element,image+0x173a3a0);m.put(element+0x18,ability);
    }return m;
}
int main(){try{unsigned cases=0;auto m=fixture();std::string e;BossTypePhases result;read::LocalType local;
    auto load=[&](){e.clear();auto text=[&](uintptr_t p,std::string& s,std::string& error){const auto found=m.strings.find(p);if(found==m.strings.end()){error="fixture missing string";return false;}s=found->second;return true;};return read::type(m,text,0x140000000,0x10000,0,result,local,e);};
    need(load(),e);
    need(result.phases.size()==2&&result.phases[0].abilities==std::vector<int32_t>{0}&&result.phases[1].abilities==std::vector<int32_t>{1}&&local.uid==0x100000042,"Complete exact native owner sample");++cases;
    auto before=local;auto after=local;std::swap(after.phases[0],after.phases[1]);need(read::reordered_local(before,after,0,2),"Original insertion boundary after source");++cases;
    after.phases[0].elements[0][1]+=1;need(!read::reordered_local(before,after,0,2),"Reorder changed Ability pointer");++cases;
    auto bad=[&](auto mutate){m=fixture();mutate(m);need(!load(),"Invalid native phase accepted");need(!e.empty(),"Missing native cause");++cases;};
    bad([](auto& f){f.put(0x10018,uint64_t(0));});
    bad([](auto& f){f.put(0x1002c,int32_t(4));});
    bad([](auto& f){f.put(0x10030,int32_t(2));});
    bad([](auto& f){f.put(0x30008,uintptr_t(0x60000));});
    bad([](auto& f){f.put(0x6105c,int32_t(0));});
    bad([](auto& f){f.put(0xa0018,uintptr_t(0xdeadbeef));});
    bad([](auto& f){f.put(0x81000,uintptr_t(0xa0000));});
    bad([](auto& f){f.put(0xa0000,uintptr_t(0));});
    bad([](auto& f){f.put(0x20008,uintptr_t(0x40000));});
    bad([](auto& f){f.put(0x600a0,int32_t(-1));});
    bad([](auto& f){f.put(0x1006c,int32_t(0));f.put(0x1016c,int32_t(1));});
    bad([](auto& f){f.put(0x60080,std::array<float,4>{NAN,0,0,1});});
    m=fixture();need(load(),e);after=local;after.uid+=uint64_t(1)<<32;need(!read::reordered_local(local,after,0,1),"High UID difference ignored");++cases;
    std::cout<<cases<<" actual Boss phase reader/ownership cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
