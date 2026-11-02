#include <cstdint>
#include <array>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace fixture {
struct Memory {std::map<uintptr_t,std::vector<unsigned char>> values;} weapon_memory;
constexpr uintptr_t weapon_image=0x10000000,toon_address=0x20000000,subscriber_address=0x21000000,region_address=0x22000000,manager_address=0x23000000,root_address=0x24000000;
namespace toon_weapon_visual {
template<class T> bool visual_read(Memory& memory,uintptr_t base,size_t offset,T& out,std::string& error){
    const auto found=memory.values.find(base+offset);
    if(found==memory.values.end()||found->second.size()!=sizeof out){error="read failed";return false;}
    std::memcpy(&out,found->second.data(),sizeof out);return true;
}
}
template<class T> void put(uintptr_t address,T value){auto& bytes=weapon_memory.values[address];bytes.resize(sizeof value);std::memcpy(bytes.data(),&value,sizeof value);}
void reset(){
    weapon_memory.values.clear();put(toon_address,weapon_image+0x1744e90);put(toon_address+0x170,root_address);put(toon_address+0x940,subscriber_address);
    put(subscriber_address,weapon_image+0x16e75f0);put(subscriber_address+0x80,toon_address);put(subscriber_address+0x20,int32_t(46));
    put(toon_address+0x178,region_address);put(region_address,weapon_image+0x1787930);put(region_address+0x9c0,manager_address);put(manager_address,weapon_image+0x16fde30);
}
#include "toon_camera_owner.inc"
struct ToonMotionSample {
    uint32_t backlog=0,deadline=0,scale=0;
    uint8_t branch=0,distance=0,nearby=0,force=0,flag269=0,flag26a=0;
    std::array<uint32_t,10> pose{};
    std::array<uint32_t,8> attributes{};
};
#include "toon_motion_sample.inc"
}
int main(){
    unsigned checks=0;const auto need=[&](bool condition,const char* cause){++checks;if(!condition)throw std::runtime_error(cause);};
    try{
        bool owned=false;std::string error;const auto caller=fixture::weapon_image+0x2ffea1;
        fixture::reset();need(fixture::toon_camera_owner(fixture::toon_address,caller,fixture::root_address,owned,error)&&owned,"valid owner rejected");
        fixture::reset();need(fixture::toon_camera_owner(fixture::toon_address,0,fixture::root_address,owned,error)&&!owned,"foreign caller admitted");
        fixture::reset();fixture::put(fixture::toon_address,uintptr_t(1));need(fixture::toon_camera_owner(fixture::toon_address,caller,fixture::root_address,owned,error)&&!owned,"foreign class admitted");
        fixture::reset();need(fixture::toon_camera_owner(fixture::toon_address,caller,fixture::root_address+1,owned,error)&&!owned,"foreign world admitted");
        fixture::reset();fixture::put(fixture::subscriber_address+0x80,uintptr_t(1));need(!fixture::toon_camera_owner(fixture::toon_address,caller,fixture::root_address,owned,error)&&!owned&&!error.empty(),"reciprocal mismatch admitted");
        fixture::reset();fixture::put(fixture::manager_address,uintptr_t(1));need(!fixture::toon_camera_owner(fixture::toon_address,caller,fixture::root_address,owned,error)&&!owned&&!error.empty(),"manager_address mismatch admitted");
        fixture::reset();fixture::weapon_memory.values.erase(fixture::toon_address+0x170);need(!fixture::toon_camera_owner(fixture::toon_address,caller,fixture::root_address,owned,error)&&!owned&&error=="read failed","read failure lost");
        fixture::reset();fixture::ToonMotionSample sample;
        for(auto offset:{size_t(0xad4),size_t(0x2b0),size_t(0x270)})fixture::put(fixture::toon_address+offset,uint32_t(offset));
        for(auto offset:{size_t(0x1c0),size_t(0x808),size_t(0x809),size_t(0x269),size_t(0x26a)})fixture::put(fixture::toon_address+offset,uint8_t(offset));
        fixture::put(fixture::weapon_image+0x1a323a0,uint8_t(7));
        for(size_t i=0;i<10;++i)fixture::put(fixture::toon_address+0x100+i*4,uint32_t(i+1));
        constexpr std::array<size_t,8> offsets{0x750,0x778,0x7a0,0x754,0x77c,0x7a4,0x7ec,0x7f0};
        for(size_t i=0;i<offsets.size();++i)fixture::put(fixture::toon_address+offsets[i],uint32_t(i+101));
        need(fixture::toon_motion_sample(fixture::toon_address,sample,error)&&sample.force==7&&sample.pose[9]==10&&sample.attributes[0]==101&&sample.attributes[7]==108,"actual trace sample fields differ");
        fixture::weapon_memory.values.erase(fixture::toon_address+0x7f0);
        need(!fixture::toon_motion_sample(fixture::toon_address,sample,error)&&error=="read failed","missing attribute read did not fail");
        std::cout<<checks<<" actual Toon owner checks passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
