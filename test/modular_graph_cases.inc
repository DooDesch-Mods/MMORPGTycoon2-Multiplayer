#include "modular_layout_graph.h"
#include "modular_identity.h"
namespace {
unsigned modular_graph_cases(){namespace graph=mt2mp::game::modular_layout_graph;namespace building=mt2mp::game::duplicate_building_history;unsigned count=0;constexpr uintptr_t owner=0xf0000,node=0x100000;building::Saved expected;expected.uid=0x100000007;expected.layout_sha=std::string(64,'a');expected.plan_sha=std::string(64,'b');const auto authored=f::pose();std::memcpy(expected.transform.data(),authored.data(),sizeof authored);
    auto setup=[&](){f::Memory m;f::array(m,owner+0x18,0x16e2dc0,{node});f::array(m,owner+0x30,0x1748aa0,{node});m.put(node,f::image+0x173b490);m.put(f::image+0x173b490+0x10,f::image+0x55dd80);m.put(node+0xb8,expected.uid);m.put(node+0xc0,expected.transform);return m;};
    auto memory=setup();unsigned captures=0;bool mutate=false,wrong_plan=false;auto record=[&](uintptr_t object,bool plan,std::string& sha,std::string& e){++captures;if(object!=node+(plan?0x248:0x130)){e="wrong owned Record address";return false;}sha=plan?(wrong_plan?std::string(64,'c'):expected.plan_sha):expected.layout_sha;if(mutate)memory.put(owner+0x18+0x10,int32_t(0));return true;};std::string error;
    f::need(graph::read(memory,f::image,owner,expected.uid,expected,record,error)&&captures==2,error);++count;
    auto bad=[&](auto change){memory=setup();change(memory);f::need(!graph::read(memory,f::image,owner,expected.uid,expected,record,error),"invalid original Modular graph accepted");++count;};
    bad([&](auto& m){m.put(node+0xb8,expected.uid+1);});bad([&](auto& m){m.put(node+0xc0+0x68,uint32_t(1));});bad([&](auto& m){m.put(node,f::image+0x173b491);});bad([&](auto& m){m.put(f::image+0x173b490+0x10,f::image+0x55dd81);});bad([&](auto& m){f::array(m,owner+0x30,0x1748aa0,{node+8});});bad([&](auto& m){f::array(m,owner+0x18,0x16e2dc0,{node,node});});
    memory=setup();wrong_plan=true;f::need(!graph::read(memory,f::image,owner,expected.uid,expected,record,error),"different complete Plan Record accepted");wrong_plan=false;++count;
    memory=setup();mutate=true;f::need(!graph::read(memory,f::image,owner,expected.uid,expected,record,error),"Graph mutation across original Save accepted");mutate=false;++count;
    memory=setup();f::array(memory,owner+0x30,0x1748aa0,{});f::need(graph::read(memory,f::image,owner,expected.uid,expected,record,error,false),"Precommit graph incorrectly requires committed sorted array");++count;
    constexpr uintptr_t modular_object=0x120000,layout=0x130000,plan=0x140000;mt2mp::game::native::BuildingState native_value;native_value.type="Scenery";native_value.variant="base";native_value.prop.uid=expected.uid;
    auto identity=[&](){f::Memory m;m.put(modular_object,f::image+0x16d93c0);m.put(modular_object+0x18,expected.uid);m.put(modular_object+0xcba,uint8_t(0));m.put(modular_object+0x7d0,layout);m.put(modular_object+0x7d8,plan);m.put(layout,f::image+0x1712a60);m.put(plan,f::image+0x16fea40);return m;};memory=identity();
    auto at=[&](uintptr_t base,size_t offset,auto& value,std::string& why){return memory.read(base+offset,&value,sizeof value,why);};
    f::need(mt2mp::game::modular_identity::read(at,f::image,modular_object,native_value,error),error);++count;
    for(auto address:{modular_object,modular_object+0x7d0,modular_object+0x7d8,layout,plan}){memory=identity();memory.put(address,uintptr_t(0));f::need(!mt2mp::game::modular_identity::read(at,f::image,modular_object,native_value,error),"Wrong Modular class/owned pointer accepted");++count;}
    memory=identity();native_value.type="Inn";f::need(!mt2mp::game::modular_identity::read(at,f::image,modular_object,native_value,error),"Separate Modular admission broadened normal Building UI");++count;native_value.type="Scenery";native_value.dungeon_membership=1;f::need(!mt2mp::game::modular_identity::read(at,f::image,modular_object,native_value,error),"Dungeon Modular owner admitted to Root action");++count;return count;
}
}
