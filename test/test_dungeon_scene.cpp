#include "dungeon_scene_key.h"
#include "dungeon_saved_building.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace scene=mt2mp::game::dungeon_scene_key;
constexpr uintptr_t fixture_image=0x10000000,fixture_root=0x20000000,fixture_shard=0x20001000,fixture_region=0x20002000,fixture_dungeon=0x20004000;
constexpr uintptr_t fixture_instance=0x20006000,fixture_exterior=0x20008000,fixture_array=0x2000a000,fixture_template_shard=0x2000c000,fixture_template_region=0x2000e000;
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;unsigned typed_calls=0;
    template<class T>void put(uintptr_t object,size_t offset,T value){const auto* p=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[object+offset+i]=p[i];}
    template<class T>bool read(uintptr_t object,size_t offset,T& value,std::string& e){auto* p=reinterpret_cast<unsigned char*>(&value);
        for(size_t i=0;i<sizeof value;++i){const auto it=bytes.find(object+offset+i);if(it==bytes.end()){e="fixture unreadable";return false;}p[i]=it->second;}return true;}
    bool typed(uintptr_t object,uintptr_t source,uintptr_t target,std::string& e){++typed_calls;
        if((object==fixture_instance&&source==0x1657270&&target==0x16650e0)||(object==fixture_dungeon&&source==0x1673900&&target==0x164e3d0)||
           (object==fixture_exterior&&source==0x1673900&&target==0x1655090))return true;e="fixture exact RTTI ABI differs";return false;}
    void init(int32_t type){
        put(fixture_root,0,fixture_image+0x174ad20);put(fixture_root,0x48,int32_t(0));put(fixture_shard,0,fixture_image+0x174ad20);put(fixture_shard,0x48,type);
        put(fixture_shard,0x18,fixture_dungeon);put(fixture_shard,0x20,fixture_instance);put(fixture_region,0,fixture_image+0x1787930);put(fixture_region,0x70,fixture_shard);
        put(fixture_dungeon,0,fixture_image+0x16c0560);put(fixture_dungeon,0x48,type==1?fixture_shard:fixture_template_shard);put(fixture_dungeon,0x50,type==1?fixture_region:fixture_template_region);
        put(fixture_template_shard,0,fixture_image+0x174ad20);put(fixture_template_shard,0x48,int32_t(1));put(fixture_template_shard,0x18,fixture_dungeon);
        put(fixture_instance,0,fixture_image+0x1719450);put(fixture_instance,0x18,int32_t(7));put(fixture_instance,0x30,fixture_dungeon);put(fixture_instance,0x38,fixture_shard);put(fixture_instance,0x40,fixture_region);
        put(fixture_dungeon,0x4c0,fixture_array);put(fixture_dungeon,0x4c8,int32_t(1));put(fixture_dungeon,0x4cc,int32_t(1));put(fixture_array,0,fixture_instance);
        put(fixture_dungeon,0x6f8,fixture_exterior);put(fixture_exterior,0,fixture_image+0x16d93c0);put(fixture_exterior,0x18,uint64_t(0x100000012));put(fixture_exterior,0xa38,fixture_dungeon);put(fixture_exterior,0x170,fixture_root);
    }
};
int main()try{size_t checks=0;const std::string key="action:11111111-1111-4111-8111-111111111111";
    auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    const auto run=[&](Memory& m,uintptr_t current,scene::Scene& out,std::string& e,bool valid=true){
        return scene::read(fixture_image,fixture_root,current,[&](auto p,auto o,auto& v,auto& why){return m.read(p,o,v,why);},
            [&](auto p,auto s,auto t,auto& why){return m.typed(p,s,t,why);},
            [&](auto p,auto uid,auto& result,auto& why){if(p!=fixture_exterior||uid!=0x100000012){why="fixture full UID differs";return false;}result=valid?key:"raw_uid:18";return true;},out,e);};
    for(int32_t type:{1,2}){Memory m;m.init(type);scene::Scene value;std::string e;
        check(run(m,fixture_shard,value,e),e.c_str());check(value.key==key+(type==1?"/template":"/instance/7"),"canonical current Dungeon scene differs");
        check(value.exterior_uid==0x100000012&&m.typed_calls==(type==1?2u:3u),"full UID or exact GCC ABI skipped");
        const auto rejected=[&](auto mutate){auto bad=m;mutate(bad);check(!run(bad,fixture_shard,value,e)&&!e.empty()&&value.key.empty(),"foreign Dungeon scene admitted or stale result retained");};
        rejected([](auto& x){x.put(fixture_shard,0x48,int32_t(3));});rejected([](auto& x){x.put(fixture_dungeon,0x48,fixture_root);});
        rejected([](auto& x){x.put(fixture_dungeon,0x50,uintptr_t(0));});rejected([](auto& x){x.put(fixture_region,0x70,fixture_root);});
        rejected([](auto& x){x.put(fixture_exterior,0x18,uint64_t(18));});rejected([](auto& x){x.put(fixture_exterior,0x170,fixture_shard);});
        rejected([](auto& x){x.put(fixture_exterior,0xa38,uintptr_t(1));});check(!run(m,fixture_shard,value,e,false),"raw local UID used as canonical scene key");
        if(type==2){rejected([](auto& x){x.put(fixture_instance,0x38,fixture_root);});rejected([](auto& x){x.put(fixture_instance,0x40,fixture_template_region);});
            rejected([](auto& x){x.put(fixture_instance,0x18,int32_t(-1));});rejected([](auto& x){x.put(fixture_dungeon,0x4c8,int32_t(0));});
            rejected([](auto& x){x.put(fixture_dungeon,0x4cc,int32_t(0));});rejected([](auto& x){x.put(fixture_array,0,uintptr_t(1));});
            rejected([](auto& x){x.put(fixture_dungeon,0x4c8,int32_t(2));x.put(fixture_dungeon,0x4cc,int32_t(2));x.put(fixture_array,8,fixture_instance);});}
        check(run(m,fixture_root,value,e)&&value.key=="world"&&value.exterior_key.empty(),"overworld current root mapping differs");
    }
#include "dungeon_saved_building_cases.inc"
    std::cout<<checks<<" Dungeon current-scene identity/RTTI/ownership checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}
