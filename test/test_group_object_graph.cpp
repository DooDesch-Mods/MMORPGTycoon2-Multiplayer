#include "group_object_graph.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
namespace {
namespace g=mt2mp::game;namespace a=mt2mp::action_session;
unsigned checks=0;void need(bool value,const std::string& e){++checks;if(!value)throw std::runtime_error(e);}
struct Memory {
    std::map<uintptr_t,uint8_t> bytes;uintptr_t next=0x100000;
    template<class T>void put(uintptr_t p,const T& value){const auto* data=reinterpret_cast<const uint8_t*>(&value);for(size_t index=0;index<sizeof value;++index)bytes[p+index]=data[index];}
    bool read(uintptr_t p,void* value,size_t count,std::string& e){auto* data=static_cast<uint8_t*>(value);for(size_t index=0;index<count;++index){auto found=bytes.find(p+index);if(found==bytes.end()){e="native graph memory span absent";return false;}data[index]=found->second;}return true;}
    void set(uintptr_t owner,uint64_t uid){const auto table=next,values=next+0x100,link=next+0x200;next+=0x1000;
        put(owner,table);put(owner+8,int32_t(4));put(owner+0xc,int32_t(30));put(owner+0x18,values);put(owner+0x20,int32_t(uid?1:0));put(owner+0x24,int32_t(1));
        for(size_t index=0;index<4;++index)put(table+index*24+16,uintptr_t(0));
        if(uid){const auto hash=uint32_t(uid);const auto bucket=(hash*0x9e3779ffu)>>30;put(table+size_t(bucket)*24+16,link);put(values,uid);put(link,uid);put(link+8,hash);put(link+16,uintptr_t(0));}}
};
constexpr uintptr_t fixture_image=0x140000000,fixture_graph=0x10000,fixture_parent=0x20000,fixture_child=0x30000,fixture_cash=0x40000,fixture_group_object=0x50000,fixture_child_object=0x60000,fixture_root=0x70000,fixture_region=0x80000;
constexpr uint64_t fixture_group_uid=0x10000000b,fixture_child_uid=12;
const std::string fixture_member="action:01234567-89ab-4cde-8fab-0123456789ab",fixture_key="group:"+fixture_member;
std::array<uint32_t,10> pose(){return {0,0,0,0x3f800000,0,0,0,0x3f800000,0x3f800000,0x3f800000};}
template<class T>void array(Memory& m,uintptr_t owner,uintptr_t ap,const std::vector<T>& values){const auto data=m.next;m.next+=0x1000;m.put(owner,fixture_image+ap);m.put(owner+8,data);m.put(owner+16,int32_t(values.size()));m.put(owner+20,int32_t(values.size()));for(size_t index=0;index<values.size();++index)m.put(data+index*sizeof(T),values[index]);}
std::map<uint64_t,g::duplicate_history::Known> known(){g::duplicate_history::Known group,scenery;group.uid=fixture_group_uid;group.object=fixture_group_object;group.root=fixture_root;group.region=fixture_region;group.name="Original Group";
    group.row={fixture_key,"",std::string(64,'a'),a::DuplicateFamily::Group,67,pose(),{fixture_member}};std::memcpy(group.transform.data(),group.row.pose.data(),40);group.transform[0x28]=1;
    scenery.uid=fixture_child_uid;scenery.object=fixture_child_object;scenery.root=fixture_root;scenery.region=fixture_region;scenery.row={fixture_member,fixture_key,std::string(64,'b'),a::DuplicateFamily::Scenery,67,pose(),{}};
    scenery.transform=group.transform;scenery.scenery={1,2};scenery.height=0x3f800000;return {{fixture_group_uid,group},{fixture_child_uid,scenery}};}
Memory setup(bool destroying){Memory m;const auto source=known();const auto& group=source.at(fixture_group_uid);const auto& scenery=source.at(fixture_child_uid);
    array(m,fixture_graph+0x18,0x16e2dc0,std::vector<uintptr_t>{fixture_parent,fixture_cash});array(m,fixture_graph+0x30,0x1748aa0,std::vector<uintptr_t>{fixture_cash,fixture_parent});
    m.put(fixture_parent,fixture_image+(destroying?0x16de860:0x16fb9d0));m.put(fixture_parent+0xb8,fixture_group_uid);m.put(fixture_image+(destroying?0x16de860:0x16fb9d0)+0x10,fixture_image+(destroying?0x53e1c0:0x54e100));
    m.set(fixture_parent+8,destroying?0:fixture_group_uid);m.set(fixture_parent+0x30,0);m.set(fixture_parent+0x58,destroying?fixture_group_uid:0);
    if(!destroying){m.put(fixture_parent+0xc0,fixture_root);m.put(fixture_parent+0xc8,fixture_region);m.put(fixture_parent+0xf0,group.transform);
        array(m,fixture_parent+0x160,0x16e2dc0,std::vector<uintptr_t>{fixture_child});array(m,fixture_parent+0x178,0x1749370,std::vector<int32_t>{int32_t(fixture_child_uid)});
        m.put(fixture_child,fixture_image+0x1706590);m.put(fixture_child+0xb8,fixture_child_uid);m.put(fixture_image+0x1706590+0x10,fixture_image+0x55a3c0);m.put(fixture_child+0xc0,scenery.scenery);m.put(fixture_child+0xc8,fixture_root);
        m.put(fixture_child+0xd0,scenery.transform);m.put(fixture_child+0x13c,scenery.height);m.put(fixture_child+0x140,scenery.automatic);m.set(fixture_child+8,fixture_child_uid);m.set(fixture_child+0x30,0);m.set(fixture_child+0x58,0);}
    m.put(fixture_cash,fixture_image+0x1786fb0);m.put(fixture_image+0x1786fb0+0x10,fixture_image+0x53ccf0);m.put(fixture_cash+0xb8,int32_t(destroying?5:-5));m.put(fixture_cash+0xbc,std::array<uint32_t,3>{});m.set(fixture_cash+8,0);m.set(fixture_cash+0x30,0);m.set(fixture_cash+0x58,0);return m;
}
}
int main()try{
    auto source=known();auto m=setup(false);std::string e;g::duplicate_history::Graph result;bool destroying=false;
    auto find=[&](uint64_t uid,uintptr_t& object,std::string&){object=destroying?(uid==fixture_group_uid?fixture_group_object:uid==fixture_child_uid?fixture_child_object:0):0;return true;};
    auto text=[](uintptr_t object,std::string& value,std::string& e){if(object!=fixture_parent+0xd0){e="foreign owned Group name";return false;}value="Original Group";return true;};
    auto read=[&](){return g::group_object_graph::destroy_restore(m,fixture_image,fixture_graph,destroying,fixture_key,source,{{destroying?5:-5,{0,0,0}}},find,text,result,e);};
    need(read(),e);need(result.operations.size()==3&&result.top_operations.size()==2&&result.targets==std::vector<std::string>{fixture_key},"complete owned Group/child/cash graph coverage differs");
    auto bad=[&](auto change){m=setup(false);source=known();e.clear();change();need(!read(),"invalid original full Group graph accepted");};
    bad([&]{m.put(fixture_parent+0xb8,uint64_t(11));});bad([&]{m.put(fixture_parent+0xc0,fixture_root+8);});bad([&]{m.put(fixture_parent+0xf0,uint32_t(1));});
    bad([&]{m.put(fixture_child+0x13c,uint32_t(0));});bad([&]{m.put(fixture_child+0xd0+0x6b,uint8_t(1));});bad([&]{m.set(fixture_parent+0x30,fixture_child_uid);});
    bad([&]{m.set(fixture_child+8,fixture_group_uid);});bad([&]{array(m,fixture_graph+0x30,0x1748aa0,std::vector<uintptr_t>{fixture_parent,fixture_child});});
    bad([&]{array(m,fixture_parent+0x178,0x1749370,std::vector<int32_t>{13});});bad([&]{m.put(fixture_cash+0xb8,int32_t(5));});bad([&]{m.put(fixture_cash+0xbc,uint32_t(1));});
    bad([&]{m.put(fixture_child,fixture_image+0x16de860);});bad([&]{source.at(fixture_child_uid).row.parent.clear();});
    destroying=true;m=setup(true);source=known();need(read(),e);m.set(fixture_parent+0x58,fixture_child_uid);need(!read(),"Group Destroy dependency narrowed or replaced full UID");
    m=setup(false);array(m,fixture_graph+0x18,0x16e2dc0,std::vector<uintptr_t>{fixture_parent});array(m,fixture_graph+0x30,0x1748aa0,std::vector<uintptr_t>{fixture_parent});
    m.put(fixture_parent,fixture_image+0x1786ff0);m.put(fixture_image+0x1786ff0+0x10,fixture_image+0x55ed80);m.put(fixture_parent+0xb8,fixture_group_uid);m.put(fixture_parent+0xc0,source.at(fixture_group_uid).transform);
    m.set(fixture_parent+8,0);m.set(fixture_parent+0x30,fixture_group_uid);m.set(fixture_parent+0x58,0);
    need(g::group_object_graph::move(m,fixture_image,fixture_graph,fixture_group_uid,source.at(fixture_group_uid).transform,result,e),e);
    m.put(fixture_parent+0xc0+0x6b,uint8_t(1));need(!g::group_object_graph::move(m,fixture_image,fixture_graph,fixture_group_uid,source.at(fixture_group_uid).transform,result,e),"Group Move inverse dropped native cached transform bytes");
    std::cout<<checks<<" original full Group Make/Destroy/Move graph cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
