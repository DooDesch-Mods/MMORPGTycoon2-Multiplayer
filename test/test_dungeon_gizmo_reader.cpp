#include "dungeon_gizmo_reader.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <limits>
namespace gizmo=mt2mp::game::dungeon_gizmo_reader;
namespace gizmo_reader_fixture {
constexpr uintptr_t image=0x10000000,manager=0x20000000,shard=0x20001000,region=0x20002000,roster=0x20003000;
constexpr std::array<uintptr_t,3> objects{0x30000000,0x30001000,0x30002000};
constexpr std::array<uint64_t,3> ids{0x100000012,0x200000012,0x300000012};
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;std::map<uintptr_t,std::string> strings;bool fail_find=false,missing_parent=false,empty_parent=false,mutate=false;
    template<class T>void put(uintptr_t object,size_t offset,T value){const auto* p=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[object+offset+i]=p[i];}
    template<class T>bool read(uintptr_t object,size_t offset,T& value,std::string& e){auto* p=reinterpret_cast<unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i){const auto found=bytes.find(object+offset+i);if(found==bytes.end()){e="original Gizmo field read failure";return false;}p[i]=found->second;}return true;}
    Memory(){put(region,0x9a8,manager);put(manager,0,image+0x1700260);put(manager,0x18,image+0x172e300);put(manager,0x68,shard);put(manager,0x70,region);put(manager,0x20,roster);put(manager,0x28,int32_t(2));put(manager,0x2c,int32_t(4));put(manager,0x78,uint32_t(7));
        for(size_t i=0;i<2;++i){const auto object=objects[i];put(roster,i*8,object);put(object,0,image+0x1749ab0);put(object,0x18,ids[i]);put(object,0x170,shard);put(object,0x178,region);
            put(object,0x258,i?objects[2]:uintptr_t(0));put(object,0x278,uintptr_t(0x40000000+i*0x100));put(object,0x100,std::array<float,4>{0,0,0,1});put(object,0x110,std::array<float,3>{float(i),2,3});put(object,0x11c,std::array<float,3>{1,1,1});
            put(object,0x250,uint8_t(i));put(object,0x254,int32_t(i));for(size_t offset=0x260;offset<=0x270;offset+=4)put(object,offset,int32_t(offset+i));put(object,0x330,int32_t(3));put(object,0x334,int32_t(11));put(object,0x338,int32_t(12));
            strings[object+0x38]=i?"Chest #2":"Door #1";strings[object+0x210]=i?"Chest":"Door";strings[object+0x230]="Stone";}
        put(objects[2],0,image+0x1749ab0);put(objects[2],0x18,ids[2]);
    }
};
}
#include "gizmo_live_cases.inc"
int main()try{using namespace gizmo_reader_fixture;size_t checks=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(std::string(why)+" comparison "+std::to_string(checks));};
    std::vector<gizmo::Record> record;gizmo::Local local;std::string error;
    const auto run=[&](Memory& m){auto read=[&](auto base,auto offset,auto& value,auto& e){return m.read(base,offset,value,e);};
        auto text=[&](uintptr_t base,size_t offset,std::string& value,std::string& e){const auto found=m.strings.find(base+offset);if(found==m.strings.end()){e="original Gizmo string read failure";return false;}value=found->second;if(m.mutate){m.put(manager,0x78,uint32_t(8));m.mutate=false;}return true;};
        auto find=[&](uint64_t uid,uintptr_t& found,std::string& e){if(m.fail_find){e="original Gizmo UID lookup failure";return false;}found=0;for(size_t i=0;i<ids.size();++i)if(uid==ids[i])found=m.missing_parent&&i==2?0:objects[i];return true;};
        auto parent=[&](uintptr_t pointer,uint64_t uid,std::string& key,std::string& e){if(pointer!=objects[2]||uid!=ids[2]){e="foreign prototype pointer/full UID";return false;}key=m.empty_parent?"":"action:11111111-1111-4111-8111-111111111111/template/gizmo/0";return true;};
        return gizmo::read(read,text,find,parent,image,manager,shard,region,record,local,error);};
    Memory valid;check(run(valid),error.c_str());check(record.size()==2&&record[1].parent.ends_with("/template/gizmo/0")&&record[0].parent.empty()&&record[1].triggered&&record[1].boss_keys==0x271&&record[0].edge_a==11&&record[0].edge_b==12,"Complete Gizmo authored/prototype/edge record differs");
    check(local.members[0].uid==ids[0]&&local.members[1].uid==ids[1]&&local.members[1].parent_uid==ids[2]&&local.mutation_count==7,"Full native UID/definition/prototype witnesses differ");
    const auto reject=[&](auto change){Memory m;change(m);check(!run(m)&&!error.empty()&&record.empty()&&local==gizmo::Local{},"Foreign or incomplete Gizmo roster admitted");};
    reject([](auto& m){m.put(manager,0,image+1);});reject([](auto& m){m.put(manager,0x18,image+1);});reject([](auto& m){m.put(manager,0x70,shard);});reject([](auto& m){m.put(manager,0x68,region);});reject([](auto& m){m.put(manager,0x28,int32_t(5));});
    reject([](auto& m){m.put(roster,0,uintptr_t(0));});reject([](auto& m){m.put(roster,8,objects[0]);});reject([](auto& m){m.put(objects[0],0,image+1);});reject([](auto& m){m.put(objects[0],0x18,uint64_t(18));});
    reject([](auto& m){m.put(objects[0],0x170,region);});reject([](auto& m){m.put(objects[0],0x178,shard);});reject([](auto& m){m.put(objects[0],0x250,uint8_t(2));});
    reject([](auto& m){m.put(objects[0],0x110,std::array<float,3>{std::numeric_limits<float>::infinity(),0,0});});
    reject([](auto& m){m.put(objects[1],0x258,objects[1]);});reject([](auto& m){m.put(objects[2],0,image+1);});reject([](auto& m){m.missing_parent=true;});reject([](auto& m){m.empty_parent=true;});reject([](auto& m){m.mutate=true;});
    Memory missing;missing.fail_find=true;check(!run(missing)&&error=="original Gizmo UID lookup failure","Original native registry cause lost");
    Memory unreadable;unreadable.bytes.erase(objects[0]+0x264);check(!run(unreadable)&&error=="original Gizmo field read failure","Original native field cause lost");
    Memory reordered;reordered.put(roster,0,objects[1]);reordered.put(roster,8,objects[0]);check(run(reordered)&&record[0].name=="Chest #2"&&local.members[0].uid==ids[1],"Native volatile roster order was sorted or replaced");
    Memory same_name;same_name.strings[objects[1]+0x38]="Door #1";check(run(same_name)&&record.size()==2&&local.members[0].uid!=local.members[1].uid,"Full UID identity was replaced with an assumed unique name");
    gizmo_live_fixture::run(check);
    std::cout<<checks<<" actual Dungeon Gizmo roster/prototype/authored-state/identity comparisons passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
