#include "dungeon_scene_key.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace scene=mt2mp::game::dungeon_scene_key;
constexpr uintptr_t image=0x10000000,root=0x20000000,shard=0x20001000,region=0x20002000,dungeon=0x20004000;
constexpr uintptr_t instance=0x20006000,exterior=0x20008000,array=0x2000a000,template_shard=0x2000c000,template_region=0x2000e000;
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;unsigned typed_calls=0;
    template<class T>void put(uintptr_t object,size_t offset,T value){const auto* p=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[object+offset+i]=p[i];}
    template<class T>bool read(uintptr_t object,size_t offset,T& value,std::string& e){auto* p=reinterpret_cast<unsigned char*>(&value);
        for(size_t i=0;i<sizeof value;++i){const auto it=bytes.find(object+offset+i);if(it==bytes.end()){e="fixture unreadable";return false;}p[i]=it->second;}return true;}
    bool typed(uintptr_t object,uintptr_t source,uintptr_t target,std::string& e){++typed_calls;
        if((object==instance&&source==0x1657270&&target==0x16650e0)||(object==dungeon&&source==0x1673900&&target==0x164e3d0)||
           (object==exterior&&source==0x1673900&&target==0x1655090))return true;e="fixture exact RTTI ABI differs";return false;}
    void init(int32_t type){
        put(root,0,image+0x174ad20);put(root,0x48,int32_t(0));put(shard,0,image+0x174ad20);put(shard,0x48,type);
        put(shard,0x18,dungeon);put(shard,0x20,instance);put(region,0,image+0x1787930);put(region,0x70,shard);
        put(dungeon,0,image+0x16c0560);put(dungeon,0x48,type==1?shard:template_shard);put(dungeon,0x50,type==1?region:template_region);
        put(template_shard,0,image+0x174ad20);put(template_shard,0x48,int32_t(1));put(template_shard,0x18,dungeon);
        put(instance,0,image+0x1719450);put(instance,0x18,int32_t(7));put(instance,0x30,dungeon);put(instance,0x38,shard);put(instance,0x40,region);
        put(dungeon,0x4c0,array);put(dungeon,0x4c8,int32_t(1));put(dungeon,0x4cc,int32_t(1));put(array,0,instance);
        put(dungeon,0x6f8,exterior);put(exterior,0,image+0x16d93c0);put(exterior,0x18,uint64_t(0x100000012));put(exterior,0xa38,dungeon);put(exterior,0x170,root);
    }
};
int main()try{size_t checks=0;const std::string key="action:11111111-1111-4111-8111-111111111111";
    auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    const auto run=[&](Memory& m,uintptr_t current,scene::Scene& out,std::string& e,bool valid=true){
        return scene::read(image,root,current,[&](auto p,auto o,auto& v,auto& why){return m.read(p,o,v,why);},
            [&](auto p,auto s,auto t,auto& why){return m.typed(p,s,t,why);},
            [&](auto p,auto uid,auto& result,auto& why){if(p!=exterior||uid!=0x100000012){why="fixture full UID differs";return false;}result=valid?key:"raw_uid:18";return true;},out,e);};
    for(int32_t type:{1,2}){Memory m;m.init(type);scene::Scene value;std::string e;
        check(run(m,shard,value,e),e.c_str());check(value.key==key+(type==1?"/template":"/instance/7"),"canonical current Dungeon scene differs");
        check(value.exterior_uid==0x100000012&&m.typed_calls==(type==1?2u:3u),"full UID or exact GCC ABI skipped");
        const auto rejected=[&](auto mutate){auto bad=m;mutate(bad);check(!run(bad,shard,value,e)&&!e.empty()&&value.key.empty(),"foreign Dungeon scene admitted or stale result retained");};
        rejected([](auto& x){x.put(shard,0x48,int32_t(3));});rejected([](auto& x){x.put(dungeon,0x48,root);});
        rejected([](auto& x){x.put(dungeon,0x50,uintptr_t(0));});rejected([](auto& x){x.put(region,0x70,root);});
        rejected([](auto& x){x.put(exterior,0x18,uint64_t(18));});rejected([](auto& x){x.put(exterior,0x170,shard);});
        rejected([](auto& x){x.put(exterior,0xa38,uintptr_t(1));});check(!run(m,shard,value,e,false),"raw local UID used as canonical scene key");
        if(type==2){rejected([](auto& x){x.put(instance,0x38,root);});rejected([](auto& x){x.put(instance,0x40,template_region);});
            rejected([](auto& x){x.put(instance,0x18,int32_t(-1));});rejected([](auto& x){x.put(dungeon,0x4c8,int32_t(0));});
            rejected([](auto& x){x.put(dungeon,0x4cc,int32_t(0));});rejected([](auto& x){x.put(array,0,uintptr_t(1));});
            rejected([](auto& x){x.put(dungeon,0x4c8,int32_t(2));x.put(dungeon,0x4cc,int32_t(2));x.put(array,8,instance);});}
        check(run(m,root,value,e)&&value.key=="world"&&value.exterior_key.empty(),"overworld current root mapping differs");
    }
    std::cout<<checks<<" Dungeon current-scene identity/RTTI/ownership checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}
