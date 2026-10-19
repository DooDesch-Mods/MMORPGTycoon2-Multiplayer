#define main group_reader_fixture_main
#include "test_group_reader.cpp"
#undef main
#include "group_inverse.h"
namespace gi=mt2mp::game::group_inverse;
int main()try{
    constexpr uintptr_t graph=0x90000,node=0xa0000;std::string e;
    auto fixture=[&](bool grouping){auto m=f::setup();f::array(m,graph+0x18,0x16e2dc0,{node});m.put(node,f::image+(grouping?0x1723370:0x16ff380));m.put(node+0xb8,f::group_uid);
        const size_t offset=grouping?0xc0:0x160;f::array(m,node+offset,0x1749370,{1,2});m.put(node+offset+0x2000,uint32_t(11));m.put(node+offset+0x2004,uint32_t(22));
        if(!grouping){m.put(node+0xc0,f::root);m.put(node+0xc8,f::region);m.put(node+0xf0,f::pose());}return m;};
    const std::vector<gi::Target> targets{{f::group_uid,f::root,f::region,f::pose(),{{f::scenery,11},{f::free_prop,22}}}};
    auto find=[](uint64_t uid,uintptr_t& p,std::string&){p=uid==11?f::scenery:uid==22?f::free_prop:0;return true;};
    for(bool grouping:{true,false}){auto m=fixture(grouping);f::need(gi::read(m,f::image,graph,grouping,targets,find,e),e);
        const size_t offset=grouping?0xc0:0x160;
        auto bad=[&](auto edit){m=fixture(grouping);edit(m);f::need(!gi::read(m,f::image,graph,grouping,targets,find,e),"invalid original Group inverse accepted");};
        bad([&](auto& v){v.put(node+0xb8,uint64_t(7));});
        bad([&](auto& v){v.put(node+offset+16,int32_t(1));});
        bad([&](auto& v){v.put(node+offset+0x2000,uint32_t(22));});
        bad([&](auto& v){v.put(node,f::image+(grouping?0x16ff380:0x1723370));});
        bad([&](auto& v){v.put(graph+0x28,int32_t(0));});
        if(!grouping){bad([&](auto& v){v.put(node+0xc0,f::root+1);});bad([&](auto& v){v.put(node+0xc8,f::region+1);});bad([&](auto& v){v.put(node+0xf0,uint32_t(1));});}
        m=fixture(grouping);auto high=targets;high[0].members[0].uid=0x10000000b;f::need(!gi::read(m,f::image,graph,grouping,high,find,e),"original truncated inverse accepted as full UID");
        auto wrong=[](uint64_t,uintptr_t& p,std::string&){p=f::group;return true;};f::need(!gi::read(m,f::image,graph,grouping,targets,wrong,e),"inverse UID aliases unrelated pointer");
    }
    std::cout<<f::checks<<" actual Group inverse full UID/ordered members/root/Region/pose cases passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}
