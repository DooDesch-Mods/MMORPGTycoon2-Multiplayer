#define main group_reader_fixture_main
#include "test_group_reader.cpp"
#undef main
#include "scenery_stroke_history.h"
namespace sh=mt2mp::game::scenery_stroke_history;
namespace {
constexpr uintptr_t owner=0xb0000,make_node=0xc0000,destroy_node=0xd0000,cash_node=0xe0000;
void dependencies(f::Memory& m,uintptr_t at,uint64_t uid){
    const uintptr_t table=at*4096+0x10000000,values=table+0x1000,link=table+0x2000;
    m.put(at,table);m.put(at+8,int32_t(32));m.put(at+12,int32_t(27));m.put(at+0x18,values);m.put(at+0x20,int32_t(uid?1:0));m.put(at+0x24,int32_t(4));
    for(size_t index=0;index<32;++index)m.put(table+index*24+0x10,uintptr_t(0));
    if(uid){m.put(values,uid);m.put(link,uid);m.put(link+8,uint32_t(uid));m.put(link+0x10,uintptr_t(0));m.put(table+size_t((uint32_t(uid)*0x9e3779ffu)>>27)*24+0x10,link);}
}
f::Memory setup(sh::Saved& saved){
    f::Memory m;saved={};saved.uid=77;saved.root=f::root;saved.region=f::region;saved.type={3,-1234};
    const auto pose=f::pose();std::memcpy(saved.transform.data(),pose.data(),sizeof pose);
    m.put(make_node,f::image+0x1706590);m.put(f::image+0x1706590+0x10,f::image+0x55a3c0);
    m.put(make_node+0xb8,saved.uid);m.put(make_node+0xc0,saved.type);m.put(make_node+0xc8,saved.root);m.put(make_node+0xd0,saved.transform);
    m.put(make_node+0x13c,saved.height);m.put(make_node+0x140,saved.automatic);
    dependencies(m,make_node+8,saved.uid);dependencies(m,make_node+0x30,0);dependencies(m,make_node+0x58,0);
    m.put(destroy_node,f::image+0x16de860);m.put(f::image+0x16de860+0x10,f::image+0x53e1c0);m.put(destroy_node+0xb8,uint64_t(88));
    dependencies(m,destroy_node+8,0);dependencies(m,destroy_node+0x30,0);dependencies(m,destroy_node+0x58,88);
    m.put(cash_node,f::image+0x1786fb0);m.put(f::image+0x1786fb0+0x10,f::image+0x53ccf0);m.put(cash_node+0xb8,int32_t(-10));m.put(cash_node+0xbc,std::array<uint32_t,3>{});
    dependencies(m,cash_node+8,0);dependencies(m,cash_node+0x30,0);dependencies(m,cash_node+0x58,0);
    f::array(m,owner+0x18,0x16e2dc0,{make_node,destroy_node,cash_node});f::array(m,owner+0x30,0x1748aa0,{cash_node,make_node,destroy_node});return m;
}
}
int main()try{
    sh::Saved saved;auto memory=setup(saved);std::string e;std::map<uint64_t,sh::Saved> stored;mt2mp::game::duplicate_history::Graph graph;
    const std::vector<mt2mp::action_session::DuplicateCash> cash{{-10,{}}};
    auto read=[&](bool original=true){return sh::graph(memory,f::image,owner,{{77,saved}},{88},cash,original,stored,graph,e);};
    f::need(read()&&stored.at(77)==saved&&graph.operations.size()==3,e);
    auto bad=[&](auto change){memory=setup(saved);change(memory);f::need(!read(),"invalid complete Scenery history graph accepted");};
    bad([](auto& m){m.put(make_node+0xd0+48,uint8_t(1));});
    memory=setup(saved);memory.put(make_node+0xd0+48,uint8_t(1));f::need(read(false)&&stored.at(77).transform[48]==1,e);
    bad([](auto& m){m.put(make_node+0xd0+16,uint32_t(0x3f800000));});
    bad([](auto& m){m.put(make_node+0xc0,int32_t(4));});
    bad([](auto& m){m.put(destroy_node+0xb8,uint64_t(99));});
    bad([](auto& m){dependencies(m,destroy_node+0x30,77);});
    bad([](auto& m){m.put(cash_node+0xb8,int32_t(-11));});
    bad([](auto& m){m.put(cash_node+0xbc,uint32_t(0x7f800000));});
    bad([](auto& m){m.put(cash_node,f::image+0x1786fb1);});
    bad([](auto& m){f::array(m,owner+0x30,0x1748aa0,{make_node,make_node,cash_node});});
    bad([](auto& m){f::array(m,owner+0x18,0x16e2dc0,{make_node,destroy_node});f::array(m,owner+0x30,0x1748aa0,{make_node,destroy_node});});
    f::need(sh::opposite(sh::opposite(cash))==cash,"Scenery original signed cash inverse did not round trip");
    std::cout<<f::checks<<" complete Scenery history graph cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
