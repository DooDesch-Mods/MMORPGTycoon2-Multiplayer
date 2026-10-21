#include <functional>
namespace modular_prefab_fixture {
namespace q=mt2mp::game::npc_quest_targets;
namespace action=mt2mp::action_session;
namespace duplicate_history=mt2mp::game::duplicate_history;
namespace scenery_make_history=mt2mp::game::scenery_make_history;
namespace modular_layout_graph=mt2mp::game::modular_layout_graph;
using Scenery=scenery_make_history::Saved;
using Building=mt2mp::game::duplicate_building_history::Saved;
struct Runtime {f::Memory memory;uintptr_t image=f::image;};
namespace duplicate_edit {
bool history_building_inverse(Runtime&,uintptr_t,const Building&,std::string& e){return q::fail(e,"Fixture has no admitted Building Make");}
}
#include "modular_prefab_graph_actual.inc"
unsigned run(){
    constexpr uintptr_t fixture_graph=0x180000,fixture_destroy=0x190000,fixture_cash=0x1a0000;
    constexpr uint64_t fixture_born=0x100000044;
    Scenery saved;Runtime r;std::string error;
    const std::vector<action::DuplicateCash> cash{{10,{}}};
    auto setup=[&](){r.memory=::setup(saved);
        f::array(r.memory,fixture_graph,0x16e32d0,{});
        f::array(r.memory,fixture_graph+0x18,0x16e2dc0,{inverse_node,fixture_destroy,fixture_cash});
        f::array(r.memory,fixture_graph+0x30,0x1748aa0,{});
        r.memory.put(fixture_destroy,f::image+0x16de860);r.memory.put(f::image+0x16de860+0x10,f::image+0x53e1c0);
        r.memory.put(fixture_destroy+0xb8,fixture_born);
        dependency(r.memory,fixture_destroy+8,0);dependency(r.memory,fixture_destroy+0x30,0);dependency(r.memory,fixture_destroy+0x58,fixture_born);
        r.memory.put(fixture_cash,f::image+0x1786fb0);r.memory.put(f::image+0x1786fb0+0x10,f::image+0x53ccf0);
        r.memory.put(fixture_cash+0xb8,int32_t(10));r.memory.put(fixture_cash+0xbc,std::array<uint32_t,3>{});
        dependency(r.memory,fixture_cash+8,0);dependency(r.memory,fixture_cash+0x30,0);dependency(r.memory,fixture_cash+0x58,0);error.clear();};
    auto read=[&](){return prefab_graph(r,fixture_graph,{{saved.uid,saved}},{},{fixture_born},cash,error);};
    unsigned count=0;setup();f::need(read(),"Fresh complete Prefab inverse rejected: "+error);++count;
    setup();f::array(r.memory,fixture_graph,0x16e32d0,{0x1b0000,0x1c0000,0x1d0000});
    f::array(r.memory,fixture_graph+0x30,0x1748aa0,{fixture_cash,inverse_node,fixture_destroy});
    f::need(read(),"Populated complete Prefab inverse rejected: "+error);++count;
    const auto bad=[&](auto change){setup();change();f::need(!read(),"Invalid Prefab payload/cache admitted");++count;};
    bad([&](){r.memory.put(inverse_node+0xb8,saved.uid+1);});
    bad([&](){r.memory.put(inverse_node+0xc8,saved.root+8);});
    bad([&](){r.memory.put(inverse_node+0xd0+48,uint8_t(1));});
    bad([&](){r.memory.put(fixture_destroy+0xb8,fixture_born+1);});
    bad([&](){dependency(r.memory,fixture_destroy+0x30,saved.uid);});
    bad([&](){r.memory.put(fixture_cash+0xb8,int32_t(11));});
    bad([&](){r.memory.put(fixture_cash,f::image+0x1786fb1);});
    bad([&](){f::array(r.memory,fixture_graph+0x18,0x16e2dc0,{inverse_node,fixture_cash});});
    bad([&](){f::array(r.memory,fixture_graph+0x18,0x16e2dc0,{inverse_node,inverse_node,fixture_cash});});
    bad([&](){f::array(r.memory,fixture_graph,0x16e32d0,{0x1b0000});});
    bad([&](){f::array(r.memory,fixture_graph+0x30,0x1748aa0,{inverse_node,fixture_destroy,fixture_cash});});
    bad([&](){r.memory.put(fixture_graph,f::image+0x16e32d1);});
    bad([&](){r.memory.put(fixture_graph+0x30,f::image+0x1748aa1);});
    bad([&](){r.memory.put(fixture_graph+0x10,int32_t(-1));});
    bad([&](){r.memory.put(fixture_graph+0x40,int32_t(100001));});
    bad([&](){f::array(r.memory,fixture_graph,0x16e32d0,{0x1b0000});f::array(r.memory,fixture_graph+0x30,0x1748aa0,{inverse_node,inverse_node,fixture_cash});});
    bad([&](){f::array(r.memory,fixture_graph,0x16e32d0,{0x1b0000});f::array(r.memory,fixture_graph+0x30,0x1748aa0,{inverse_node,fixture_destroy,fixture_cash+8});});
    return count;
}
}
