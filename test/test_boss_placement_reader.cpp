#include "boss_placement_reader.h"
#include <iostream>
#include <map>
#include <vector>
namespace {
namespace read=mt2mp::game::boss_placement_reader;
int checks=0,failures=0;
void need(bool value,const char* why){++checks;if(!value){++failures;std::cerr<<why<<'\n';}}
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;uintptr_t change=0;uint32_t changes=0;
    template<class T>void set(uintptr_t address,const T& value){const auto* data=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[address+i]=data[i];}
    bool read(uintptr_t address,void* out,size_t size,std::string& error){
        auto* data=static_cast<unsigned char*>(out);for(size_t i=0;i<size;++i){const auto found=bytes.find(address+i);if(found==bytes.end()){error="unmapped fixture byte";return false;}data[i]=found->second;}
        if(address==change&&++changes==2)data[0]^=1;return true;
    }
};
constexpr uintptr_t fixture_image=0x140000000,fixture_cursor=0x200000000,fixture_receiver=0x300000000,fixture_point=0x400000000;
Memory memory(){Memory out;out.set(fixture_image+0x13df9d0,fixture_cursor);out.set(fixture_cursor,fixture_image+read::cursor_addresspoint);out.set(fixture_cursor+0x190,fixture_receiver);
    out.set(fixture_receiver,fixture_image+read::addresspoint);out.set(fixture_receiver+0x3b1,uint8_t(1));out.set(fixture_receiver+0x148,int32_t(7));out.set(fixture_receiver+0x330,uintptr_t(0x500000000));
    std::array<unsigned char,0x6c> pose{};const std::array<float,10> values{0,0,0,1,2,3,4,1,1,1};std::memcpy(pose.data(),values.data(),40);out.set(fixture_receiver+0x338,pose);
    out.set(fixture_point,std::array<float,3>{22,33,44});return out;
}
bool resolve(read::Intent& value,std::string&){value.region=18;value.character_sha=std::string(64,'a');return true;}
struct Cleanup {
    uintptr_t active=fixture_receiver;std::vector<std::string> calls;bool fail=false;
    bool current(uintptr_t,uintptr_t& out,std::string&){calls.push_back("current");out=active;return true;}
    bool source(const read::Source&,std::string& error){calls.push_back("source");if(fail){error="source lifetime cause";return false;}return true;}
    bool cancel(uintptr_t,std::string&){calls.push_back("cancel");active=0x600000000;return true;}
    bool clear(uintptr_t,std::string&){calls.push_back("clear");return true;}
};
}
int main(){
    auto source_memory=memory();read::Source captured;std::string error;
    need(read::read(source_memory,fixture_image,fixture_receiver,fixture_cursor,fixture_point,resolve,captured,error),"actual complete original source captured");
    need(captured.intent.type==7&&captured.intent.region==18&&captured.visual==0x500000000,"global type, selected Root Region and visual witness retained");
    need(captured.intent.pose[4]==std::bit_cast<uint32_t>(2.f)&&captured.intent.point[0]==std::bit_cast<uint32_t>(22.f),"release Region point is independent of spawned pose");
    for(const auto offset:{uintptr_t(0),uintptr_t(0x148),uintptr_t(0x330),uintptr_t(0x338),uintptr_t(0x3b1)}){
        auto changed=memory();changed.change=fixture_receiver+offset;read::Source rejected;error.clear();
        need(!read::read(changed,fixture_image,fixture_receiver,fixture_cursor,fixture_point,resolve,rejected,error)&&!error.empty(),"changed source field rejected with cause");
    }
    auto changed=memory();changed.change=fixture_point;read::Source rejected;need(!read::read(changed,fixture_image,fixture_receiver,fixture_cursor,fixture_point,resolve,rejected,error),"borrowed release point changed during capture");
    auto invalid=memory();invalid.set(fixture_receiver+0x3b1,uint8_t(0));need(!read::read(invalid,fixture_image,fixture_receiver,fixture_cursor,fixture_point,resolve,rejected,error),"invalid native source flag rejected");
    invalid=memory();invalid.set(fixture_cursor+0x190,uintptr_t(19));need(!read::read(invalid,fixture_image,fixture_receiver,fixture_cursor,fixture_point,resolve,rejected,error),"foreign active tool rejected");
    Cleanup cleanup;need(read::cleanup(cleanup,captured,error)&&cleanup.calls==std::vector<std::string>{"current","source","cancel","clear","current"},"exact native source cleanup once");
    Cleanup other;other.active=77;need(read::cleanup(other,captured,error)&&other.active==77&&other.calls==std::vector<std::string>{"current","source","clear","current"},"different player tool remains active");
    Cleanup stale;stale.fail=true;need(!read::cleanup(stale,captured,error)&&error=="source lifetime cause"&&stale.active==fixture_receiver,"source lifetime failure performs no cleanup mutation");
    std::cout<<checks<<" Boss source/cleanup checks, "<<failures<<" failures\n";return failures?1:0;
}
