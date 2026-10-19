#include "dungeon_instance_native_guard.h"
#include <cstring>
#include <iostream>

namespace native=mt2mp::game::dungeon_instance_native;
namespace life=mt2mp::game::dungeon_instance_lifecycle;
namespace {
struct Memory {
    std::array<unsigned char,1024> bytes{};std::vector<size_t> writes;
    template<class T>void put(size_t offset,T value){std::memcpy(bytes.data()+offset,&value,sizeof value);}
    template<class T>bool operator()(uintptr_t p,size_t offset,T& value,std::string& error){
        if(p!=100||offset+sizeof value>bytes.size()){error="actual test memory owner/span differs";return false;}
        std::memcpy(&value,bytes.data()+offset,sizeof value);return true;
    }
    bool store(uintptr_t p,size_t offset,const void* old_value,const void* value,size_t size,std::string& error){
        if(p!=100||offset+size>bytes.size()||std::memcmp(bytes.data()+offset,old_value,size)){
            error="actual test guarded store mismatch";return false;}
        writes.push_back(offset);std::memcpy(bytes.data()+offset,value,size);return true;
    }
};
life::Event event(){life::Event value;value.exterior="action:aaaaaaaa-bbbb-cccc-dddd-123456789abc";
    value.operation="aaaaaaaa-bbbb-cccc-dddd-123456789abc";value.ordinal=1;return value;}
}
int main()try{
    unsigned checks=0;std::string error;
    const auto check=[&](bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(std::string(message)+": "+error);};
    native::guard::Pool before{10,20,30,1000,2000,17,{{2,40,41,42},{8,50,51,52}}};
    auto after=before;after.counter=18;after.members.push_back({17,60,61,62});auto value=event();
    check(native::guard::transition(before,after,life::Effect::Make,0,60,value,error),"actual Make transition rejected");
    check(value.index==17&&value.before==std::vector<int32_t>{2,8}&&value.after==std::vector<int32_t>{2,8,17},"native index replaced by slot");
    auto changed=after;changed.members[0].instance=90;value=event();
    check(!native::guard::transition(before,changed,life::Effect::Make,0,60,value,error),"replaced survivor accepted");
    value=event();check(!native::guard::transition(before,after,life::Effect::Make,0,90,value,error),"foreign Make return accepted");
    changed=after;changed.exterior_uid++;value=event();
    check(!native::guard::transition(before,changed,life::Effect::Make,0,60,value,error),"changed original owner UID accepted");
    after=before;after.members.erase(after.members.begin());value=event();
    check(native::guard::transition(before,after,life::Effect::Destroy,40,0,value,error)&&value.index==2,"actual Destroy transition rejected");
    after.counter++;value=event();
    check(!native::guard::transition(before,after,life::Effect::Destroy,40,0,value,error),"Destroy changed native next counter");
    for(const auto effect:{life::Effect::Reset,life::Effect::ForcePlayersOut}){
        value=event();check(native::guard::transition(before,before,effect,50,0,value,error)&&value.index==8,"whole Reset/Force transition rejected");
        value=event();check(!native::guard::transition(before,before,effect,70,0,value,error),"foreign Reset/Force member accepted");
    }
    Memory memory;constexpr uintptr_t image=0x100000;
    memory.put<uintptr_t>(0,image+0x1719450);memory.put<int32_t>(0x18,8);
    memory.put<uintptr_t>(0x30,10);memory.put<uintptr_t>(0x38,51);memory.put<uintptr_t>(0x40,52);
    memory.put<uintptr_t>(0x68,700);memory.put<uint32_t>(0x1c,0x3f800000);memory.put<uint32_t>(0x24,0xbf800000);
    memory.put<uint32_t>(0x70,0x41200000);memory.put<uint8_t>(0x20,1);memory.put<uint8_t>(0x28,1);
    memory.put<uint8_t>(0x74,1);memory.put<uint8_t>(0x75,0);
    native::guard::Update saved;check(native::guard::update(memory,image,100,saved,error),"actual saved Update fields rejected");
    for(const auto offset:native::guard::float_offsets)memory.put<uint32_t>(offset,0);
    for(const auto offset:native::guard::flag_offsets)memory.put<uint8_t>(offset,0);
    memory.put<uintptr_t>(0x68,0);memory.put<uint32_t>(0xcc,123);
    auto store=[&](uintptr_t p,size_t offset,const void* old_value,const void* target,size_t bytes,std::string& e){return memory.store(p,offset,old_value,target,bytes,e);};
    check(native::guard::restore_update(memory,store,image,saved,error),"saved scalar guard failed");
    uintptr_t party=1;uint32_t presentation=0;
    check(memory(100,0x68,party,error)&&!party,"scalar guard restored stale weak Party");
    check(memory(100,0xcc,presentation,error)&&presentation==123,"scalar guard reverted unscaled presentation");
    check(std::none_of(memory.writes.begin(),memory.writes.end(),[](size_t offset){return offset==0x68||offset==0xcc;}),"scalar guard wrote an unrelated field");
    memory.writes.clear();memory.put<uintptr_t>(0x30,11);
    check(!native::guard::restore_update(memory,store,image,saved,error)&&memory.writes.empty(),"changed Instance owner received stale scalar writes");
    check(native::guard::suppress_force(true,false,false,0xb78b5),"unowned timeout Force escaped Clock0 guard");
    check(!native::guard::suppress_force(false,false,false,0xb78b5),"Host timeout Force suppressed");
    check(!native::guard::suppress_force(true,true,false,0xb78b5),"actual scoped action Force suppressed");
    check(!native::guard::suppress_force(true,false,true,0xb78b5),"original lifecycle replay Force suppressed");
    check(!native::guard::suppress_force(true,false,false,0xb6b4d),"nested original Reset Force suppressed");
    check(native::guard::suppress_dip(true,0xa795f,100,100),"own Make camera Dip not suppressed");
    check(!native::guard::suppress_dip(false,0xa795f,100,100),"normal UI Make camera Dip suppressed");
    check(!native::guard::suppress_dip(true,0xa795f,200,100),"foreign camera manager Dip suppressed");
    check(!native::guard::suppress_dip(true,0x50a880,100,100),"other normal UI Dip suppressed");
    bool restore=false;
    check(native::guard::camera_restore(100,200,200,true,restore,error)&&restore,"own born displayed Shard not restored");
    check(native::guard::camera_restore(100,100,200,false,restore,error)&&!restore,"unchanged camera needs fabricated write");
    check(!native::guard::camera_restore(100,300,200,true,restore,error)&&!restore,"foreign displayed Shard accepted");
    check(!native::guard::camera_restore(100,200,200,false,restore,error)&&!restore,"unsuppressed original UI Dip overwritten");
    std::cout<<checks<<" actual Instance transition, saved-scalar, weak-Party and camera guard checks passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
