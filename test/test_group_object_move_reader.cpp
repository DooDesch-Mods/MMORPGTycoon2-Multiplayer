#include "group_object_move_reader.h"
#include <iostream>
#include <stdexcept>
namespace {
namespace reader=mt2mp::game::group_object_move_reader;
unsigned checks=0;
void need(bool value,const std::string& e){++checks;if(!value)throw std::runtime_error(e);}
struct Memory {
    std::map<uintptr_t,uint8_t> data;
    template<class T>void put(uintptr_t p,const T& value){const auto* bytes=reinterpret_cast<const uint8_t*>(&value);for(size_t index=0;index<sizeof value;++index)data[p+index]=bytes[index];}
    bool read(uintptr_t p,void* value,size_t length,std::string& e){auto* bytes=static_cast<uint8_t*>(value);for(size_t index=0;index<length;++index){
        const auto found=data.find(p+index);if(found==data.end()){e="original memory span unavailable";return false;}bytes[index]=found->second;}return true;}
};
constexpr uintptr_t fixture_image=0x140000000,fixture_root=0x10000,fixture_behaviour=0x20000,fixture_record=0x30000,fixture_blueprint=0x40000,fixture_ghost=0x50000,fixture_group=0x60000,fixture_cursor=0x70000;
constexpr uint64_t fixture_uid=0x100000007;
const std::string fixture_member="action:01234567-89ab-4cde-8fab-0123456789ab",fixture_key="group:"+fixture_member;
std::array<uint32_t,10> pose(){return {0,0,0,0x3f800000,0,0,0,0x3f800000,0x3f800000,0x3f800000};}
Memory setup(){Memory m;m.put(fixture_behaviour,fixture_image+0x172f1f0);m.put(fixture_behaviour+0x401,uint8_t(1));m.put(fixture_behaviour+0x499,uint8_t(0));
    m.put(fixture_behaviour+0x458,fixture_image+0x1747300);m.put(fixture_behaviour+0x460,fixture_record);m.put(fixture_behaviour+0x468,int32_t(1));m.put(fixture_behaviour+0x46c,int32_t(2));
    m.put(fixture_record,fixture_blueprint);m.put(fixture_record+8,fixture_ghost);m.put(fixture_record+16,fixture_group);m.put(fixture_group,fixture_image+0x1749de0);m.put(fixture_group+0x18,fixture_uid);
    m.put(fixture_ghost,fixture_image+0x1705230);m.put(fixture_ghost+0x210,fixture_blueprint);m.put(fixture_blueprint,fixture_image+0x16dd870);m.put(fixture_blueprint+8,fixture_root);
    m.put(fixture_image+0x1749de0+0xd0,fixture_image+0xe47100);m.put(fixture_image+0x1749de0+0x100,fixture_image+0x4ceaa0);
    auto after=pose();after[4]=0x41200000;m.put(fixture_ghost+0x100,after);return m;
}
bool bind(uintptr_t object,uint64_t actual,mt2mp::action_session::GroupObjectIntent& out,std::string& e){
    if(object!=fixture_group||actual!=fixture_uid){e="complete Group canonical UID differs";return false;}
    out.key=fixture_key;out.before=pose();out.members={fixture_member};return true;
}
}
int main()try{
    Memory m=setup();reader::Capture capture;std::string e;bool supported=false;
    auto read=[&](){return reader::read(m,fixture_image,fixture_root,fixture_behaviour,bind,100,capture,e);};
    need(reader::target(m,fixture_image,fixture_behaviour,supported,e)&&supported,e);need(read(),e);
    need(capture.uid==fixture_uid&&capture.intent.key==fixture_key&&capture.intent.members==std::vector<std::string>{fixture_member}&&capture.intent.after[4]==0x41200000,"complete Group native capture lost fields");
    std::array<unsigned char,0x6c> transform;need(reader::transform(capture.intent.after,transform,e)&&transform[0x28]==1&&
        std::memcmp(transform.data(),capture.intent.after.data(),40)==0,"complete Group transform did not preserve all native components");
    auto bad=[&](auto mutation){m=setup();mutation(m);e.clear();need(!read(),"invalid original Group Move capture accepted");};
    bad([](auto& x){x.put(fixture_behaviour+0x401,uint8_t(0));});bad([](auto& x){x.put(fixture_behaviour+0x499,uint8_t(1));});
    bad([](auto& x){x.put(fixture_behaviour+0x468,int32_t(2));});bad([](auto& x){x.put(fixture_behaviour+0x46c,int32_t(0));});
    bad([](auto& x){x.put(fixture_record+8,fixture_group);});bad([](auto& x){x.put(fixture_group+0x18,uint64_t(7));});
    bad([](auto& x){x.put(fixture_ghost+0x210,fixture_blueprint+8);});bad([](auto& x){x.put(fixture_blueprint+8,fixture_root+8);});
    bad([](auto& x){x.put(fixture_image+0x1749de0+0xd0,fixture_image+0xe47101);});bad([](auto& x){x.put(fixture_image+0x1749de0+0x100,fixture_image+0x4ceaa1);});
    bad([](auto& x){x.put(fixture_ghost+0x100,pose());});bad([](auto& x){x.put(fixture_ghost+0x110,uint32_t(0x7fc00000));});
    m=setup();m.put(fixture_group,fixture_image+0x16c1390);need(reader::target(m,fixture_image,fixture_behaviour,supported,e)&&!supported,"normal single prop was captured as Group");
    m=setup();m.put(fixture_behaviour+0x468,int32_t(2));need(reader::target(m,fixture_image,fixture_behaviour,supported,e)&&!supported,"normal multi-selection was captured as Group");
    m=setup();need(read(),e);m.put(fixture_behaviour+0x468,int32_t(0));m.put(fixture_behaviour+0x490,uintptr_t(0));m.put(fixture_image+0x13df9d0,fixture_cursor);m.put(fixture_cursor+0x190,fixture_behaviour+8);
    need(!reader::cleaned(m,fixture_image,capture,e),"uncleaned original Ghost/Blueprint admitted");m.put(fixture_record,uintptr_t(0));m.put(fixture_record+8,uintptr_t(0));
    need(reader::cleaned(m,fixture_image,capture,e),e);m.put(fixture_cursor+0x190,fixture_behaviour);need(!reader::cleaned(m,fixture_image,capture,e),"active original cursor admitted");
    std::cout<<checks<<" Group original Move capture/cleanup cases passed\n";return 0;
}catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}
