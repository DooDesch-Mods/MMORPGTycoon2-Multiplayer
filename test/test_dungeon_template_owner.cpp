#include "dungeon_template_owner.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace owner=mt2mp::game::dungeon_template_owner;
constexpr uintptr_t image=0x10000000,root=0x20000000,dungeon=0x20004000,fixture_shard=0x20001000,fixture_region=0x20002000,fixture_exterior=0x20008000;
constexpr uint64_t dungeon_uid=0x200000012,exterior_uid=0x300000012;
constexpr std::array<uintptr_t,4> managers{0x30000000,0x30002000,0x30004000,0x30006000};
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;bool fail_uid=false,foreign_uid=false,fail_rtti=false;unsigned exterior_reads=0;bool move_manager=false;
    template<class T>void put(uintptr_t object,size_t offset,T value){const auto* p=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[object+offset+i]=p[i];}
    template<class T>bool read(uintptr_t object,size_t offset,T& value,std::string& e){auto* p=reinterpret_cast<unsigned char*>(&value);
        for(size_t i=0;i<sizeof value;++i){const auto it=bytes.find(object+offset+i);if(it==bytes.end()){e="original template read failure";return false;}p[i]=it->second;}
        if(object==dungeon&&offset==0x6f8&&++exterior_reads==2&&move_manager)put(fixture_region,owner::region_offsets[0],managers[1]);return true;}
    Memory(){put(root,0,image+0x174ad20);put(root,0x48,int32_t(0));put(fixture_shard,0,image+0x174ad20);put(fixture_shard,0x48,int32_t(1));put(fixture_shard,0x18,dungeon);
        put(fixture_region,0,image+0x1787930);put(fixture_region,0x70,fixture_shard);put(dungeon,0,image+0x16c0560);put(dungeon,0x18,dungeon_uid);put(dungeon,0x48,fixture_shard);put(dungeon,0x50,fixture_region);put(dungeon,0x6f8,fixture_exterior);
        put(fixture_exterior,0,image+0x16d93c0);put(fixture_exterior,0x18,exterior_uid);put(fixture_exterior,0xa38,dungeon);put(fixture_exterior,0x170,root);
        for(size_t i=0;i<4;++i){put(fixture_region,owner::region_offsets[i],managers[i]);put(managers[i],0,image+owner::addresspoints[i]);put(managers[i],owner::reverse_region[i],fixture_region);}
        put(managers[0],0x908,fixture_shard);put(managers[3],0x68,fixture_shard);
    }
};
int main()try{size_t comparisons=0;const auto check=[&](bool ok,const char* why){++comparisons;if(!ok)throw std::runtime_error(std::string(why)+" comparison "+std::to_string(comparisons));};
    std::string error;owner::Owner actual;
    const auto run=[&](Memory& m){auto read=[&](auto object,auto offset,auto& value,auto& cause){return m.read(object,offset,value,cause);};
        auto typed=[&](uintptr_t object,uintptr_t source,uintptr_t target,std::string& e){if(m.fail_rtti){e="original RTTI failure";return false;}
            return (object==dungeon&&source==0x1673900&&target==0x164e3d0)||(object==fixture_exterior&&source==0x1673900&&target==0x1655090);};
        auto find=[&](uint64_t uid,uintptr_t& found,std::string& e){if(m.fail_uid){e="original UID lookup failure";return false;}found=m.foreign_uid?1:uid==dungeon_uid?dungeon:uid==exterior_uid?fixture_exterior:0;return true;};
        auto identify=[&](uintptr_t object,uint64_t uid,std::string& key,std::string& e){if(object!=fixture_exterior||uid!=exterior_uid){e="foreign full fixture_exterior UID";return false;}key="action:11111111-1111-4111-8111-111111111111";return true;};
        return owner::read(read,typed,find,identify,image,root,dungeon,actual,error);};
    Memory valid;check(run(valid),error.c_str());check(actual.key=="action:11111111-1111-4111-8111-111111111111/template"&&actual.dungeon_uid==dungeon_uid&&actual.exterior_uid==exterior_uid&&actual.managers==managers,"Explicit template manager/full UID mapping differs");
    const auto reject=[&](auto mutate){Memory m;mutate(m);check(!run(m)&&!error.empty()&&actual==owner::Owner{},"Foreign template owner admitted or partial output retained");};
    reject([](auto& m){m.put(dungeon,0x18,uint64_t(18));});reject([](auto& m){m.foreign_uid=true;});reject([](auto& m){m.fail_rtti=true;});
    for(size_t i=0;i<4;++i){reject([&](auto& m){m.put(fixture_region,owner::region_offsets[i],uintptr_t(0));});reject([&](auto& m){m.put(managers[i],0,uintptr_t(1));});reject([&](auto& m){m.put(managers[i],owner::reverse_region[i],root);});}
    reject([](auto& m){m.put(managers[0],0x908,root);});reject([](auto& m){m.put(managers[3],0x68,root);});reject([](auto& m){m.move_manager=true;});
    Memory unavailable;unavailable.fail_uid=true;check(!run(unavailable)&&error=="original UID lookup failure","Original registry failure cause lost");
    Memory unreadable;unreadable.bytes.erase(dungeon+0x48);check(!run(unreadable)&&error=="original template read failure","Original native read cause lost");
    std::cout<<comparisons<<" explicit Dungeon template manager/full UID/ownership comparisons passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
