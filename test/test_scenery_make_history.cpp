#define main group_reader_fixture_main
#include "test_group_reader.cpp"
#undef main
#include "scenery_make_history.h"
namespace history=mt2mp::game::scenery_make_history;
namespace {
constexpr uintptr_t scenery_object=0xb0000,inverse_node=0xc0000;
constexpr uint64_t scenery_uid=987;
void dependency(f::Memory& memory,uintptr_t owner,uint64_t singleton){
    const auto table=owner*4096+0x10000000,values=table+0x1000,link=table+0x2000;
    memory.put(owner,table);memory.put(owner+8,int32_t(32));memory.put(owner+12,int32_t(27));
    memory.put(owner+0x18,values);memory.put(owner+0x20,int32_t(singleton?1:0));memory.put(owner+0x24,int32_t(4));
    for(size_t bucket=0;bucket<32;++bucket)memory.put(table+bucket*24+0x10,uintptr_t(0));
    if(singleton){memory.put(values,singleton);memory.put(link,singleton);memory.put(link+8,uint32_t(singleton));memory.put(link+0x10,uintptr_t(0));
        const auto bucket=(uint32_t(singleton)*0x9e3779ffu)>>27;memory.put(table+size_t(bucket)*24+0x10,link);}
}
f::Memory setup(history::Saved& saved){
    f::Memory memory;memory.put(scenery_object,f::image+0x16c1390);memory.put(scenery_object+0x18,scenery_uid);
    memory.put(scenery_object+0x170,f::root);memory.put(scenery_object+0x178,f::region);
    memory.put(scenery_object+0x268,std::array<int32_t,2>{3,1234});
    std::array<uint8_t,0x6c> transform{};const auto pose=f::pose();std::memcpy(transform.data(),pose.data(),sizeof pose);
    memory.put(scenery_object+0x100,transform);memory.put(scenery_object+0x270,uint32_t(0));memory.put(scenery_object+0x275,uint8_t(1));
    auto find=[](uint64_t uid,uintptr_t& out,std::string& e){out=uid==scenery_uid?scenery_object:0;e.clear();return true;};std::string e;
    f::need(history::live(memory,f::image,scenery_object,scenery_uid,f::root,f::region,find,saved,e),e);
    memory.put(inverse_node,f::image+0x1706590);memory.put(f::image+0x1706590+0x10,f::image+0x55a3c0);
    memory.put(inverse_node+0xb8,scenery_uid);memory.put(inverse_node+0xc0,saved.type);memory.put(inverse_node+0xc8,saved.root);
    memory.put(inverse_node+0xd0,saved.transform);memory.put(inverse_node+0x13c,saved.height);memory.put(inverse_node+0x140,saved.automatic);
    dependency(memory,inverse_node+8,scenery_uid);dependency(memory,inverse_node+0x30,0);dependency(memory,inverse_node+0x58,0);return memory;
}
}
#include "modular_graph_cases.inc"
#include "building_clearance_graph_cases.inc"
int main()try{
    history::Saved saved;auto memory=setup(saved);std::string e;f::need(history::make(memory,f::image,inverse_node,saved,e),e);
    auto bad=[&](auto change){memory=setup(saved);change(memory);f::need(!history::make(memory,f::image,inverse_node,saved,e),"invalid original MakeScenery accepted");};
    bad([](auto& m){m.put(inverse_node,f::image+0x1706591);});
    bad([](auto& m){m.put(f::image+0x1706590+0x10,f::image+0x55a3c1);});
    bad([](auto& m){m.put(inverse_node+0xb8,uint64_t(988));});
    bad([](auto& m){m.put(inverse_node+0xc0,int32_t(4));});
    bad([](auto& m){m.put(inverse_node+0xc8,f::root+8);});
    bad([](auto& m){m.put(inverse_node+0x130,uint8_t(1));});
    bad([](auto& m){m.put(inverse_node+0x13c,uint32_t(1));});
    bad([](auto& m){m.put(inverse_node+0x140,uint8_t(0));});
    bad([](auto& m){dependency(m,inverse_node+8,988);});
    bad([](auto& m){dependency(m,inverse_node+0x30,scenery_uid);});
    bad([](auto& m){dependency(m,inverse_node+0x58,scenery_uid);});
    bad([](auto& m){m.put(inverse_node+8+0x20,int32_t(2));});
    memory=setup(saved);memory.put(inverse_node,f::image+0x16de860);memory.put(f::image+0x16de860+0x10,f::image+0x53e1c0);
    dependency(memory,inverse_node+8,0);dependency(memory,inverse_node+0x30,0);dependency(memory,inverse_node+0x58,scenery_uid);
    f::need(history::destroy(memory,f::image,inverse_node,scenery_uid,e),e);
    dependency(memory,inverse_node+0x30,scenery_uid);f::need(!history::destroy(memory,f::image,inverse_node,scenery_uid,e),"Destroy foreign required dependency accepted");
    memory=setup(saved);const auto owner=inverse_node+8,table=owner*4096+0x10000000,link=table+0x2000;
    memory.put(link+0x10,link);f::need(!history::make(memory,f::image,inverse_node,saved,e),"cyclic original UID dependency accepted");
    memory=setup(saved);memory.put(scenery_object+0x18,uint64_t(988));auto find=[](uint64_t,uintptr_t& out,std::string&){out=scenery_object;return true;};
    history::Saved live;f::need(!history::live(memory,f::image,scenery_object,scenery_uid,f::root,f::region,find,live,e),"stale full UID live Scenery accepted");
    memory=setup(saved);memory.put(scenery_object+0x275,uint8_t(2));
    f::need(!history::live(memory,f::image,scenery_object,scenery_uid,f::root,f::region,find,live,e),"unknown native automatic flag accepted");
    (void)modular_graph_cases();
    (void)building_clearance_cases::run();
    std::cout<<f::checks<<" complete MakeScenery stored-payload/dependency cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
