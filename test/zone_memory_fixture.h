#include "monster_zone_read.h"
#include "npc_zone_append.h"
#include "monster_zone_stroke.h"
#include "monster_zone_probe_guards.h"
#include <map>
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace z=mt2mp::game::monster_zone_read;
namespace q=mt2mp::game::npc_quest_targets;
struct Memory {
    std::map<uintptr_t,unsigned char> data;
    void space(uintptr_t p,size_t n){for(size_t i=0;i<n;++i)data[p+i]=0;}
    template<class T>void put(uintptr_t p,const T& v){space(p,sizeof v);std::memcpy(&buffer[0],&v,sizeof v);for(size_t i=0;i<sizeof v;++i)data[p+i]=buffer[i];}
    unsigned char buffer[4096]{};
    bool read(uintptr_t p,void* out,size_t n,std::string& e){for(size_t i=0;i<n;++i){auto it=data.find(p+i);if(it==data.end()){e="fixture unreadable byte";return false;}static_cast<unsigned char*>(out)[i]=it->second;}return true;}
    bool copy(uintptr_t p,void* out,size_t n,std::string& e){return read(p,out,n,e);}
};
struct Fixture {
    Memory m;q::World w;uintptr_t bm=0x50000,zm=0x60000,mm=0x70000,zone=0x80000,monster=0x90000;
    Fixture(){
        w.image=0x140000000;w.map=0x10000;w.root=w.map+0x1c0;w.region=0x30000;w.saved_region=67;
        for(auto p:{w.map,w.region,bm,zm,mm,zone,monster})m.space(p,0x1000);
        auto ap=[&](uintptr_t p,uintptr_t r){m.put(p,w.image+r);};
        ap(w.map,0x1742860);ap(w.root,0x174ad20);m.put(w.root+0x48,int32_t(0));
        ap(w.map+0x230,0x16f04e0);m.put(w.map+0x238,uintptr_t(0x20000));m.put(w.map+0x240,int32_t(1));m.put(w.map+0x244,int32_t(1));m.put(0x20000,w.region);
        ap(w.region,0x1787930);m.put(w.region+0x70,w.root);m.put(w.region+0x834,int32_t(67));
        m.put(w.region+0x950,bm);m.put(w.region+0x968,zm);m.put(w.region+0x988,mm);
        ap(bm,0x17183c0);m.put(bm+0x450,w.root);m.put(bm+0x458,w.region);
        ap(zm,0x16fe170);m.put(zm+0xf0,w.region);ap(zm+0x48,0x172e1e0);m.put(zm+0x50,uintptr_t(0x61000));m.put(zm+0x58,int32_t(1));m.put(zm+0x5c,int32_t(1));m.put(0x61000,zone);
        ap(zone,0x1745ce0);m.put(zone+0x18,uint64_t(55));m.put(zone+0x170,w.root);m.put(zone+0x178,w.region);m.put(zone+0x24c,int32_t(1));m.put(zone+0x360,int32_t(1));m.put(zone+0x560,int32_t(2));
        ap(zone+0x2f0,0x16e0140);ap(zone+0x330,0x16eb080);m.put(zone+0x308,uintptr_t(0x81000));m.space(0x81000,24);
        ap(mm,0x1714100);ap(mm+0x38,0x16e8b60);m.put(mm+0x80,w.region);m.put(mm+0x40,uintptr_t(0x71000));m.put(mm+0x48,int32_t(1));m.put(mm+0x4c,int32_t(1));m.put(0x71000,monster);
        ap(monster,0x16c0770);m.put(monster+0x18,uint64_t(66));m.put(monster+0x170,w.root);m.put(monster+0x178,w.region);m.put(monster+0x820,int32_t(9));m.put(monster+0x824,int32_t(3));m.put(monster+0x4a4,int32_t(2));m.put(monster+0x4a8,int32_t(1));m.put(monster+0x6f8,int32_t(1));m.put(monster+0x8d0,zone);m.put(monster+0x8d8,uintptr_t(0x82000));m.put(zone+0x28,uintptr_t(0x82000));m.put(0x82000,zone+0x20);
        m.put(zone+0x388,uintptr_t(0x83000));m.put(zone+0x390,int32_t(1));m.put(zone+0x394,int32_t(1));m.put(0x83000,monster);
        m.put(w.image+0x1a30b70,int32_t(10));m.put(w.image+0x164b250,uintptr_t(0xa0000));m.space(0xa0000,32);m.put(w.region+0x160,uintptr_t(0xa0100));m.space(0xa0100,32);
    }
};
void require(bool ok,const std::string& what){if(!ok)throw std::runtime_error(what);}
