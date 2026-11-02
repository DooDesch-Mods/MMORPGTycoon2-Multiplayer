#include "snapshot_save_subscriber_motion.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>

namespace passive=mt2mp::game::subscriber_passive;
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;
    template<class T>void put(uintptr_t address,const T& value){const auto* data=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=data[i];}
    bool copy(uintptr_t address,void* output,size_t size,std::string& error){
        auto* data=static_cast<unsigned char*>(output);
        for(size_t i=0;i<size;++i){const auto it=bytes.find(address+i);if(it==bytes.end()){error="missing native field";return false;}data[i]=it->second;}
        return true;
    }
};
int main(){
    unsigned checks=0;const auto need=[&](bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);};
    constexpr uintptr_t image=0x10000000,subscriber=0x20000000,toon=0x21000000,region=0x22000000,manager=0x23000000,root=0x24000000,render=0x25000000,data=0x26000000;
    Memory memory;std::string error;std::optional<passive::Motion> pose;passive::MotionOwner owner;
    auto read=[&](uintptr_t base,size_t offset,auto& value,std::string& cause){return memory.copy(base+offset,&value,sizeof value,cause);};
    auto setup=[&]{memory.bytes.clear();error.clear();memory.put(subscriber+0x80,toon);memory.put(toon,image+0x1744e90);
        memory.put(toon+0x940,subscriber);memory.put(toon+0x825,uint8_t(1));memory.put(toon+0x170,root);
        memory.put(toon+0x178,region);memory.put(toon+0x9a8,region);memory.put(region+0x834,int32_t(67));
        memory.put(toon+0x30,render);memory.put(render+0x38,image+0x3e82a0);
        memory.put(toon+0x100,std::array<float,4>{0,0,0,1});memory.put(toon+0x110,std::array<float,3>{1,2,3});
        memory.put(toon+0x11c,std::array<float,3>{1,1,1});memory.put(region+0x9c0,manager);
        memory.put(manager,image+0x16fde30);memory.put(manager+0xc8,uint8_t(0));memory.put(manager+0x28,data);
        memory.put(manager+0x30,int32_t(1));memory.put(manager+0x34,int32_t(1));memory.put(data,toon);};
    auto world=[&](passive::MotionOwner& value,std::string&){value.map=root-0x1c0;value.root=root;return true;};
    auto select=[&](const passive::MotionOwner&,int32_t id,uintptr_t expected,uintptr_t& out,std::string& cause){
        if(id!=67||expected!=region){cause="region mismatch";return false;}out=region;return true;};
    auto member=[&](passive::MotionOwner& value,std::string& cause){std::vector<uintptr_t> list;
        if(!passive::motion_manager(memory,read,image,value.region,value.manager,list,cause))return false;
        if(std::count(list.begin(),list.end(),value.toon)!=1){cause="membership mismatch";return false;}return true;};
    auto sample=[&]{return passive::motion(memory,read,image,subscriber,world,select,member,pose,error,&owner);};
    try{
        setup();need(sample()&&pose&&pose->region==67&&pose->position==std::array<float,3>{1,2,3}&&owner.toon==toon,"valid registered pose rejected");
        setup();memory.put(toon+0x940,uintptr_t(1));need(!sample()&&!error.empty(),"nonreciprocal Toon accepted");
        setup();memory.put(toon+0x170,uintptr_t(1));need(!sample()&&!error.empty(),"foreign shard accepted");
        setup();memory.put(render+0x38,uintptr_t(1));need(!sample()&&!error.empty(),"foreign transform callback accepted");
        setup();memory.put(manager+0xc8,uint8_t(1));need(!sample()&&!error.empty(),"updating manager accepted");
        setup();memory.put(data,uintptr_t(1));need(!sample()&&!error.empty(),"unregistered Toon accepted");
        setup();memory.put(subscriber+0x80,uintptr_t(0));need(sample()&&!pose,"absent Toon given a pose");
        setup();memory.put(toon+0x825,uint8_t(0));need(sample()&&!pose,"offline Toon given a pose");
        std::cout<<checks<<" actual passive Subscriber motion checks passed\n";return 0;
    }catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
}
