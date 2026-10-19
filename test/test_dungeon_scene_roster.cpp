#include "dungeon_scene_roster.h"
#include <iostream>
#include <map>
#include <stdexcept>
namespace roster=mt2mp::game::dungeon_scene_roster;
constexpr uintptr_t image=0x10000000,map=0x20000000,root=map+0x1c0;
constexpr uintptr_t root_regions=0x21000000,root_region=0x22000000,empty_region=0x22004000,root_manager=0x23000000,empty_manager=0x23004000,fixture_buildings=0x24000000;
constexpr uintptr_t ordinary=0x25000000,fixture_exterior=0x25004000,fixture_dungeon=0x26000000,fixture_template_shard=0x27000000,fixture_template_region=0x28000000,fixture_instances=0x29000000;
constexpr std::array<uintptr_t,2> fixture_instance{0x30000000,0x30004000},fixture_instance_shard{0x31000000,0x31004000},instance_region{0x32000000,0x32004000};
constexpr uint64_t dungeon_uid=0x100000012,exterior_uid=0x200000012;
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;bool bad_find=false,bad_rtti=false,mutate=false;size_t identified=0;
    template<class T>void put(uintptr_t object,size_t offset,const T& value){const auto* data=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[object+offset+i]=data[i];}
    template<class T>bool read(uintptr_t object,size_t offset,T& value,std::string& e){auto* data=reinterpret_cast<unsigned char*>(&value);
        for(size_t i=0;i<sizeof value;++i){const auto found=bytes.find(object+offset+i);if(found==bytes.end()){e="original scene owner read failure";return false;}data[i]=found->second;}return true;}
    void array(uintptr_t object,uintptr_t data,int32_t count){put(object,8,data);put(object,0x10,count);put(object,0x14,count+2);}
    Memory(){put(map,0,image+0x1742860);put(map,0x230,image+0x16f04e0);array(map+0x230,root_regions,2);put(root_regions,0,root_region);put(root_regions,8,empty_region);
        put(root,0,image+0x174ad20);put(root,0x48,int32_t(0));
        for(size_t i=0;i<2;++i){const auto r=i?empty_region:root_region,m=i?empty_manager:root_manager;
            put(r,0,image+0x1787930);put(r,0x70,root);put(r,0x834,int32_t(i?7:67));put(r,0x950,m);
            put(m,0,image+0x17183c0);put(m,0x450,root);put(m,0x458,r);put(m,0x408,image+0x172d7c0);array(m+0x408,i?0:fixture_buildings,i?0:2);}
        put(fixture_buildings,0,ordinary);put(fixture_buildings,8,fixture_exterior);put(ordinary,0,image+0x16d93c0);put(ordinary,0xa38,uintptr_t(0));
        put(fixture_exterior,0,image+0x16d93c0);put(fixture_exterior,0x18,exterior_uid);put(fixture_exterior,0x170,root);put(fixture_exterior,0xa38,fixture_dungeon);
        put(fixture_dungeon,0,image+0x16c0560);put(fixture_dungeon,0x18,dungeon_uid);put(fixture_dungeon,0x48,fixture_template_shard);put(fixture_dungeon,0x50,fixture_template_region);put(fixture_dungeon,0x6f8,fixture_exterior);array(fixture_dungeon+0x4b8,fixture_instances,2);
        put(fixture_template_shard,0,image+0x174ad20);put(fixture_template_shard,0x48,int32_t(1));put(fixture_template_shard,0x18,fixture_dungeon);put(fixture_template_region,0,image+0x1787930);put(fixture_template_region,0x70,fixture_template_shard);
        for(size_t i=0;i<2;++i){put(fixture_instances,i*8,fixture_instance[i]);put(fixture_instance[i],0,image+0x1719450);put(fixture_instance[i],0x18,int32_t(i?2:9));put(fixture_instance[i],0x30,fixture_dungeon);put(fixture_instance[i],0x38,fixture_instance_shard[i]);put(fixture_instance[i],0x40,instance_region[i]);
            put(fixture_instance_shard[i],0,image+0x174ad20);put(fixture_instance_shard[i],0x48,int32_t(2));put(fixture_instance_shard[i],0x20,fixture_instance[i]);put(instance_region[i],0,image+0x1787930);put(instance_region[i],0x70,fixture_instance_shard[i]);}
    }
};
int main()try{
    size_t checks=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" comparison="+std::to_string(checks));};
    std::vector<roster::Scene> actual;std::string error;
    const auto run=[&](Memory& m){auto at=[&](auto p,auto offset,auto& value,auto& e){return m.read(p,offset,value,e);};
        auto typed=[&](uintptr_t object,uintptr_t source,uintptr_t target,std::string& e){if(m.bad_rtti){e="original scene RTTI failure";return false;}
            return (object==fixture_dungeon&&source==0x1673900&&target==0x164e3d0)||(object==fixture_exterior&&source==0x1673900&&target==0x1655090)||
                ((object==fixture_instance[0]||object==fixture_instance[1])&&source==0x1657270&&target==0x16650e0);};
        auto find=[&](uint64_t uid,uintptr_t& found,std::string& e){if(m.bad_find){e="original scene full UID failure";return false;}found=uid==dungeon_uid?fixture_dungeon:uid==exterior_uid?fixture_exterior:0;return true;};
        auto identify=[&](uintptr_t object,uint64_t uid,std::string& key,std::string& e){if(object!=fixture_exterior||uid!=exterior_uid){e="foreign scene exterior";return false;}
            key=std::string(64,'a')+":world:building:17";if(m.mutate&&++m.identified==2)m.put(fixture_buildings,0,fixture_exterior);return true;};
        return roster::read(at,typed,find,identify,image,map,root,actual,error);};
    Memory valid;check(run(valid),error.c_str());check(actual.size()==3,"Template and both actual native Instances were not enumerated");
    check(actual[0].exterior_region==67&&actual[0].exterior_slot==1&&actual[0].owner.type==1&&actual[0].owner.instance_index==-1,"Exterior saved slot was replaced with Dungeon rank");
    check(actual[1].owner.instance_index==9&&actual[2].owner.instance_index==2&&actual[1].owner.key.ends_with("/instance/9")&&actual[2].owner.key.ends_with("/instance/2"),"Native saved Instance indices were replaced with array positions");
    check(actual[0].dungeon_uid==dungeon_uid&&actual[0].owner.exterior_uid==exterior_uid,"Full Dungeon/exterior native UIDs truncated");
    const auto reject=[&](auto mutate){Memory m;mutate(m);check(!run(m)&&!error.empty()&&actual.empty(),"Foreign/incomplete scene membership admitted");};
    reject([](auto& m){m.put(root_region,0x70,fixture_template_shard);});reject([](auto& m){m.put(empty_region,0x834,int32_t(67));});
    reject([](auto& m){m.put(root_manager,0x458,empty_region);});reject([](auto& m){m.put(root_manager,0x408,image+1);});
    reject([](auto& m){m.put(fixture_dungeon,0x18,uint64_t(18));});reject([](auto& m){m.put(fixture_exterior,0x18,uint64_t(18));});
    reject([](auto& m){m.put(fixture_template_region,0x70,root);});reject([](auto& m){m.put(fixture_instance[1],0x18,int32_t(9));});
    reject([](auto& m){m.put(fixture_instance[1],0x30,ordinary);});reject([](auto& m){m.put(instance_region[1],0x70,fixture_instance_shard[0]);});
    reject([](auto& m){m.put(fixture_instances,8,fixture_instance[0]);});reject([](auto& m){m.put(fixture_instance[1],0x38,fixture_template_shard);});
    reject([](auto& m){m.mutate=true;});
    Memory absent;absent.put(fixture_exterior,0xa38,uintptr_t(0));check(run(absent)&&actual.empty(),"An actual saved world without Dungeon exteriors failed classification");
    Memory unavailable;unavailable.bad_find=true;check(!run(unavailable)&&error=="original scene full UID failure","Original native registry cause lost");
    Memory broken;broken.bytes.erase(fixture_instance[0]+0x38);check(!run(broken)&&error=="original scene owner read failure","Original native owner read cause lost");
    Memory rtti;rtti.bad_rtti=true;check(!run(rtti)&&error=="original scene RTTI failure","Original native RTTI cause lost");
    std::cout<<checks<<" actual Dungeon root/template/Instance saved-owner comparisons passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
