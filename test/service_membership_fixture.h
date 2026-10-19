#pragma once
#include "service_membership_reader.h"
#include <cstring>
#include <map>
#include <stdexcept>
namespace service_fixture {
namespace a=mt2mp::action_session;namespace reader=mt2mp::service_membership_reader;
inline constexpr uintptr_t image=0x140000000,building=0x10000,manager=0x20000,character_data=0x30000,character=0x40000,data=0x50000,service=0x60000;
inline void need(bool ok,const std::string& e){if(!ok)throw std::runtime_error(e);}
struct Memory {
    std::map<uintptr_t,std::vector<uint8_t>> fields;std::map<uintptr_t,std::string> strings;
    template<class T>void put(uintptr_t at,const T& value){std::vector<uint8_t> bytes(sizeof value);std::memcpy(bytes.data(),&value,sizeof value);fields[at]=std::move(bytes);}
    bool read(uintptr_t at,void* out,size_t size,std::string& e){const auto found=fields.find(at);if(found==fields.end()||found->second.size()!=size){e="service fixture field absent at="+std::to_string(at)+" size="+std::to_string(size);return false;}std::memcpy(out,found->second.data(),size);return true;}
    template<class T>bool operator()(uintptr_t base,size_t offset,T& value,std::string& e){return read(base+offset,&value,sizeof value,e);}
    bool text(uintptr_t at,std::string& out,std::string& e){auto found=strings.find(at);if(found==strings.end()){e="service fixture string absent";return false;}out=found->second;return true;}
    void array(uintptr_t owner,size_t offset,uintptr_t items,int32_t count,int32_t capacity){put(owner+offset+8,items);put(owner+offset+16,count);put(owner+offset+20,capacity);}
    void row(uintptr_t object,uint64_t uid,int32_t type,uintptr_t spawn,std::string key){
        put(object,image+0x1718480);put(object+0x18,uid);put(object+0x148,building);put(object+0x20,type);
        put(object+0x28,image+0x174ab50);put(object+0x40,std::array<int32_t,2>{});put(object+0x48,std::array<uint8_t,3>{0,0,1});
        put(object+0x130,uintptr_t(0));put(object+0x138,spawn);put(object+0x140,int32_t(-1));put(object+0x150,uintptr_t(0));put(object+0x178,uintptr_t(0));strings[object+0x158]=std::move(key);
        for(size_t i=0;i<2;++i){const auto offset=0x50+i*0x70;put(object+offset,image+0x16e4710);put(object+offset+0x38,image+0x1749370);
            put(object+offset+0x40,object+0x400+i*0x100);put(object+offset+0x48,std::array<int32_t,8>{0,10,0,0,-9876,10,0,0});put(object+offset+0x68,uint8_t(0));}
    }
};
inline Memory memory(){Memory m;m.strings[building+0x768]="SmallInn";m.strings[image+0x1a30080]="Spawn";
    m.put(image+0x13df7a0,manager);m.put(manager,image+0x1732430);m.array(manager,0x80,character_data,1,4);m.put(character_data,character);
    m.put(character,image+0x1705990);m.put(character+0x18,uint64_t(0x100000001));m.put(character+0x2c,int32_t(0));m.put(character+0x30,int32_t(0));m.put(character+0x490,uint8_t(0));m.strings[character+0xb0]="Warrior";
    m.put(image+0x13df840,uintptr_t(0));m.array(building,0x9d0,data,3,4);
    for(size_t i=0;i<3;++i){m.put(data+i*8,service+i*0x1000);m.row(service+i*0x1000,0x200000001+i,i==1?3:2,i==2?character:0,"SmallInn_Spawn");}
    m.put(image+0x164b250,uintptr_t(0xa0000));m.put(0xa0000,std::array<uint64_t,4>{1,2,3,4});m.put(0xb0000+0x168,manager);return m;
}
inline bool read(Memory& m,a::ServiceMembershipState& value,reader::Local& local,std::string& e){
    auto text=[&](uintptr_t at,std::string& out,std::string& error){return m.text(at,out,error);};
    auto destination=[](uintptr_t,std::string&,std::string& error){error="fixture unknown transit key";return false;};
    return reader::sample(m,text,destination,image,building,value,local,e);
}
inline void mutate(Memory& m,const a::ServiceMembershipIntent& intent,const a::ServiceMembershipState& before,const reader::Local& local){
    std::vector<uintptr_t> pointers;
    for(size_t i=0;i<local.rows.size();++i){const auto& row=before.rows[i];
        if(intent.kind==a::ServiceMembershipKind::Remove?i==size_t(intent.ordinal):row.type==2&&row.destination.empty()&&row.character==intent.character)continue;
        pointers.push_back(local.rows[i].object);}
    if(intent.kind==a::ServiceMembershipKind::AddSpawn){const uintptr_t added=0x90000;m.row(added,0x300000001,2,intent.character<0?0:character,before.building_definition+"_"+before.spawn_type);pointers.push_back(added);}
    for(size_t i=0;i<pointers.size();++i)m.put(data+i*8,pointers[i]);m.put(building+0x9e0,int32_t(pointers.size()));
}
}
