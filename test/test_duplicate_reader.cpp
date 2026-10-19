#define main group_reader_fixture_main
#include "test_group_reader.cpp"
#undef main
#include "duplicate_reader.h"
namespace d=mt2mp::game::duplicate_reader;
constexpr uintptr_t map_owner=0xa0000,building_manager=0xb0000,scenery_manager=0xc0000,building=0xd0000,behaviour=0xe0000,fixture_ghost=0xf0000,fixture_blueprint=0x100000,fixture_cursor=0x110000;
constexpr uint64_t building_uid=0x400000007;
f::Memory setup(){auto m=f::setup();
    f::array(m,map_owner+0x350,0,{f::region});m.put(f::region,f::image+0x1787930);m.put(f::region+0x70,f::root);m.put(f::region+0x834,int32_t(67));
    m.put(f::region+0x950,building_manager);m.put(f::region+0x960,scenery_manager);
    m.put(building_manager,f::image+0x17183c0);m.put(building_manager+0x450,f::root);m.put(building_manager+0x458,f::region);f::array(m,building_manager+0x408,0x172d7c0,{building});
    m.put(scenery_manager,f::image+0x17152d0);m.put(scenery_manager+0x900,f::region);m.put(scenery_manager+0x908,f::root);
    for(size_t c=0;c<47;++c)f::array(m,scenery_manager+0x18+c*0x30,0x172d6a0,c==3?std::vector<uintptr_t>{f::scenery,f::free_prop}:std::vector<uintptr_t>{});
    for(auto p:{f::scenery,f::free_prop}){m.put(p+0x178,f::region);m.put(p+0x268,int32_t(3));}
    m.put(building,f::image+0x16d93c0);m.put(building+0x18,building_uid);m.put(building+0x170,f::root);m.put(building+0x178,f::region);m.put(building+0x180,uintptr_t(0));m.put(building+0x100,f::pose());
    m.put(behaviour,f::image+0x172f1f0);m.put(behaviour+0x401,uint8_t(1));m.put(behaviour+0x499,uint8_t(1));
    f::array(m,behaviour+0x458,0x1747300,{fixture_blueprint});m.put(behaviour+0x458+0x2000+8,fixture_ghost);m.put(behaviour+0x458+0x2000+16,building);
    f::array(m,behaviour+0x478,0x1748c50,{building});m.put(fixture_ghost,f::image+0x1705230);m.put(fixture_ghost+0x210,fixture_blueprint);m.put(fixture_ghost+0x100,f::pose());
    return m;
}
int main()try{
    auto m=setup();std::string e;d::Census census;
    auto find=[](uint64_t uid,uintptr_t& object,std::string&){object=uid==building_uid?building:uid==f::member_uid?f::scenery:uid==f::free_uid?f::free_prop:0;return true;};
    auto read=[&]{return d::census(m,f::image,map_owner,f::root,find,census,e);};
    f::need(read(),e);f::need(census.objects.size()==3&&census.regions==std::vector<uintptr_t>{f::region},"complete native census lost member");
    auto bad=[&](auto change){m=setup();change();f::need(!read(),"invalid complete native census accepted");};
    bad([&]{m.put(building+0x18,uint64_t(7));});bad([&]{m.put(building+0x170,f::root+1);});bad([&]{m.put(building+0x178,f::region+1);});
    bad([&]{m.put(building_manager+0x458,f::root);});bad([&]{m.put(scenery_manager+0x908,f::region);});
    bad([&]{m.put(f::scenery+0x268,int32_t(4));});bad([&]{f::array(m,building_manager+0x408,0x172d7c0,{building,building});});
    bad([&]{m.put(f::region+0x834,int32_t(-1));});bad([&]{m.put(scenery_manager+0x18+46*0x30,f::image+1);});
    auto point=[](std::array<uint32_t,3>& out,std::string&){out={0,0x41a00000,0};return true;};d::Capture captured;
    auto capture=[&]{return d::capture(m,f::image,behaviour,point,captured,e);};m=setup();f::need(capture(),e);
    f::need(captured.targets==std::vector<d::Handle>{{building,building_uid}}&&captured.ghosts==std::vector<uintptr_t>{fixture_ghost},"original ordered target/Ghost lost");
    auto reject_capture=[&](auto change){m=setup();change();f::need(!capture(),"invalid Duplicate source accepted");};
    reject_capture([&]{m.put(behaviour+0x499,uint8_t(0));});reject_capture([&]{m.put(behaviour+0x401,uint8_t(0));});
    reject_capture([&]{m.put(fixture_ghost+0x210,fixture_blueprint+1);});reject_capture([&]{m.put(behaviour+0x478+0x2000,f::scenery);});
    reject_capture([&]{m.put(behaviour+0x488,int32_t(0));});reject_capture([&]{m.put(fixture_ghost+0x100,uint32_t(0x7fc00000));});
    m=setup();f::need(capture(),e);m.put(behaviour+0x468,int32_t(0));m.put(behaviour+0x490,uintptr_t(0));
    m.put(f::image+0x13df9d0,fixture_cursor);m.put(fixture_cursor+0x190,uintptr_t(123));m.put(captured.data,uintptr_t(0));m.put(captured.data+8,uintptr_t(0));
    f::need(d::cleaned(m,f::image,captured,e),e);m.put(captured.data+8,fixture_ghost);f::need(!d::cleaned(m,f::image,captured,e),"unclean original Ghost accepted");
    m.put(captured.data+8,uintptr_t(0));m.put(fixture_cursor+0x190,behaviour);f::need(!d::cleaned(m,f::image,captured,e),"still active source accepted");
    std::cout<<f::checks<<" actual Duplicate manager/capture/cleanup/full UID cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
