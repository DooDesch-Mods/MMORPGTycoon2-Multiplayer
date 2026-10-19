#define main group_reader_fixture_main
#include "test_group_reader.cpp"
#undef main
#include "duplicate_history_reader.h"
namespace dh=mt2mp::game::duplicate_history;
int main()try{
    constexpr uintptr_t graph=0x90000,cash=0xa0000,group=0xb0000,first=0xc0000,second=0xd0000;
    std::string e;bool alive=false;std::map<uint64_t,dh::Known> known;
    auto add=[&](uint64_t uid,const std::string& key,mt2mp::action_session::DuplicateFamily family){dh::Known row;
        row.uid=uid;row.object=uid*0x1000;row.root=f::root;row.region=f::region;row.row.key=key;row.row.region=67;row.row.family=family;
        row.name="Group";row.scenery={3,1};row.height=0x3f800000;known.emplace(uid,row);};
    add(33,"group:action:a",mt2mp::action_session::DuplicateFamily::Group);add(11,"action:a",mt2mp::action_session::DuplicateFamily::Scenery);add(22,"action:b",mt2mp::action_session::DuplicateFamily::Scenery);
    known.at(33).row.members={"action:a","action:b"};known.at(11).row.parent=known.at(22).row.parent="group:action:a";
    auto find=[&](uint64_t uid,uintptr_t& out,std::string&){const auto row=known.find(uid);out=alive&&row!=known.end()?row->second.object:0;return true;};
    auto text=[](uintptr_t,std::string& out,std::string&){out="Group";return true;};
    auto fixture=[&](bool undo){f::Memory m;alive=undo;f::array(m,graph+0x18,0x16e2dc0,{cash,group});
        m.put(cash,f::image+0x1786fb0);m.put(cash+0xb8,int32_t(undo?20:-20));m.put(cash+0xbc,std::array<uint32_t,3>{});
        m.put(f::image+0x1786fb0+0x10,f::image+0x53ccf0);m.put(f::image+0x16de860+0x10,f::image+0x53e1c0);
        m.put(f::image+0x1706590+0x10,f::image+0x55a3c0);m.put(f::image+0x16fb9d0+0x10,f::image+0x54e100);
        m.put(group,f::image+(undo?0x16de860:0x16fb9d0));m.put(group+0xb8,uint64_t(33));
        if(!undo){const auto& saved=known.at(33);m.put(group+0xc0,saved.root);m.put(group+0xc8,saved.region);m.put(group+0xf0,saved.transform);
            f::array(m,group+0x160,0x16e2dc0,{first,second});f::array(m,group+0x178,0x1749370,{1,2});
            m.put(group+0x178+0x2000,int32_t(11));m.put(group+0x178+0x2004,int32_t(22));
            for(auto [ptr,uid]:std::vector<std::pair<uintptr_t,uint64_t>>{{first,11},{second,22}}){const auto& child=known.at(uid);
                m.put(ptr,f::image+0x1706590);m.put(ptr+0xb8,uid);m.put(ptr+0xc0,child.scenery);m.put(ptr+0xc8,child.root);
                m.put(ptr+0xd0,child.transform);m.put(ptr+0x13c,child.height);m.put(ptr+0x140,child.automatic);}}
        return m;};
    for(bool undo:{false,true}){auto m=fixture(undo);dh::Graph out;
        auto read=[&](){return dh::read(m,f::image,graph,undo,known,find,text,out,e);};
        f::need(read(),e);f::need(out.targets==std::vector<std::string>{"group:action:a"}&&out.cash.size()==1,"original whole Group graph not read");
        f::array(m,graph+0x30,0x1748aa0,{group,cash});std::vector<uintptr_t> sorted;
        f::need(dh::execution_order(m,f::image,graph,out,sorted,e)&&sorted==std::vector<uintptr_t>{group,cash},"native dependency order replaced by insertion order");
        f::array(m,graph+0x30,0x1748aa0,{group,group});f::need(!dh::execution_order(m,f::image,graph,out,sorted,e),"duplicate sorted native operation admitted");
        f::array(m,graph+0x30,0x1748aa0,{group,first});f::need(!dh::execution_order(m,f::image,graph,out,sorted,e),"nested or foreign sorted native operation admitted");
        auto bad=[&](auto change){m=fixture(undo);change(m);f::need(!read(),"invalid Duplicate history original graph admitted");};
        bad([&](auto& v){v.put(group+0xb8,uint64_t(99));});bad([&](auto& v){v.put(group,f::image+1);});
        bad([&](auto& v){v.put(f::image+0x1786fb0+0x10,f::image+1);});bad([&](auto& v){f::array(v,graph+0x18,0x16e2dc0,{cash,group,group});});
        m=fixture(undo);alive=!undo;f::need(!read(),"native complete target alive/absent inversion admitted");
        if(!undo){bad([&](auto& v){v.put(group+0xc0,f::root+1);});bad([&](auto& v){v.put(group+0xc8,f::region+1);});
            bad([&](auto& v){v.put(first+0xc0,int32_t(4));});bad([&](auto& v){v.put(first+0xc8,f::root+1);});
            bad([&](auto& v){v.put(first+0xd0,uint32_t(1));});bad([&](auto& v){v.put(first+0x13c,uint32_t(0));});
            bad([&](auto& v){v.put(second+0xb8,uint64_t(11));});
            bad([&](auto& v){v.put(group+0x178+0x2004,int32_t(-22));});
            bad([&](auto& v){v.put(group+0x178+0x2000,int32_t(22));});}
    }
    known.clear();add(77,"action:building",mt2mp::action_session::DuplicateFamily::Building);alive=false;
    constexpr uintptr_t building=0xe0000,service=0xf0000,quest=0x100000,npc=0x110000;
    f::Memory related_memory;f::array(related_memory,graph+0x18,0x16e2dc0,{building,service,quest,npc});
    related_memory.put(building,f::image+0x173c5b0);related_memory.put(building+0xb8,uint64_t(77));
    related_memory.put(service,f::image+0x1737830);related_memory.put(quest,f::image+0x1713330);related_memory.put(npc,f::image+0x16de8a0);dh::Graph related_graph;
    size_t building_calls=0,related_calls=0;bool permit_building=true,permit_related=true;
    auto building_value=[&](uintptr_t pointer,const dh::Known& saved,std::string& why){++building_calls;
        if(permit_building&&pointer==building&&saved.uid==77)return true;why="exact Building payload cause";return false;};
    auto related_value=[&](uintptr_t pointer,uintptr_t addresspoint,std::string& why){++related_calls;
        if(permit_related&&((pointer==service&&addresspoint==f::image+0x1737830)||(pointer==quest&&addresspoint==f::image+0x1713330)||
            (pointer==npc&&addresspoint==f::image+0x16de8a0)))return true;
        why="exact related inverse cause";return false;};
    auto related_read=[&](bool undo){e.clear();return dh::read(related_memory,f::image,graph,undo,known,find,text,building_value,related_value,related_graph,e);};
    f::need(related_read(false)&&building_calls==1&&related_calls==3&&related_graph.operations.size()==4&&
        related_graph.targets==std::vector<std::string>{"action:building"},"complete Building/related inverse callback dispatch differs");
    f::need(!dh::read(related_memory,f::image,graph,false,known,find,text,related_graph,e),"unbound default reader admitted Building inverse");
    permit_building=false;f::need(!related_read(false)&&e=="exact Building payload cause","Building payload failure cause hidden");permit_building=true;
    permit_related=false;f::need(!related_read(false)&&e=="exact related inverse cause","Service/Quest inverse failure cause hidden");permit_related=true;
    f::array(related_memory,graph+0x18,0x16e2dc0,{building,service,service});
    f::need(!related_read(false)&&e.find("repeats")!=std::string::npos,"repeated Service inverse entered callback twice");
    f::array(related_memory,graph+0x18,0x16e2dc0,{service});
    f::need(!related_read(true)&&e.find("Destroy graph")!=std::string::npos,"related Make inverse entered original Destroy graph");
    f::need(dh::cash_equal({{1,{}},{2,{}}},{{2,{}},{1,{}}})&&!dh::cash_equal({{1,{}},{1,{}}},{{1,{}},{2,{}}}),"cash node multiplicity lost");
    std::cout<<f::checks<<" actual Duplicate inverse graph/nested MakeGroup/full UID/stored transform cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
