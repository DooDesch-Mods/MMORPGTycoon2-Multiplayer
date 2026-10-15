#pragma once
#include "tutorial_development.h"
#include <cstring>
#include <set>
#include <vector>

namespace mt2mp::game::tutorial_replication::start_intent {
struct Window {uintptr_t address=0,tabs=0,selected=0;bool operator==(const Window&)const=default;};
template<class Memory>
bool window(Memory& memory,const tutorial_development::StartingpointSnapshot& native,
            uintptr_t pay_receiver,Window& out,std::string& error,bool after_creation=false){
    out={};
    const auto fail=[&](const char* cause){error=cause;return false;};
    if(!native.origin.mode||!native.image||!native.creation||!native.row||!native.record||
       (!after_creation&&(native.project||native.row->project_present||native.row->purchased))||
       (after_creation&&(!native.project||!native.row->project_present||!native.row->purchased))||
       native.row->deployed||native.row->custom||
       pay_receiver!=native.creation->source||!pay_receiver)
        return fail("Startingpoint StartTech current record/catalogue/no-project binding differs");
    struct Witness{uintptr_t address;std::vector<unsigned char> bytes;};std::vector<Witness> reads;
    const auto read=[&](uintptr_t p,void* value,size_t count){if(!p||p>UINTPTR_MAX-count){error="Startingpoint StartTech UI range overflow";return false;}Witness w{p,std::vector<unsigned char>(count)};if(!memory.copy(p,w.bytes.data(),count,error))return false;std::memcpy(value,w.bytes.data(),count);reads.push_back(std::move(w));return true;};
    const auto at=[&]<class T>(uintptr_t p,size_t offset,T& value){if(!p||p>UINTPTR_MAX-offset){error="Startingpoint StartTech UI owner overflow";return false;}return read(p+offset,&value,sizeof(value));};
    const auto text=[&](uintptr_t p,size_t offset,std::string& value){uintptr_t data=0;uint64_t count=0;if(!at(p,offset,data)||!at(p,offset+8,count))return false;if(count>200){error="Startingpoint StartTech window name exceeds200";return false;}std::array<char,201> bytes{};if(!read(data,bytes.data(),size_t(count)+1))return false;if(bytes[count]||std::memchr(bytes.data(),0,size_t(count))){error="Startingpoint StartTech window name terminator differs";return false;}value.assign(bytes.data(),size_t(count));return true;};
    const auto mode=reinterpret_cast<uintptr_t>(native.origin.mode);uintptr_t data=0;int32_t count=0,capacity=0;
    if(!at(mode,0xa0,data)||!at(mode,0xa8,count)||!at(mode,0xac,capacity))return false;
    if(count<1||capacity<count||capacity>512)return fail("Startingpoint StartTech Mode window bounds invalid");
    std::set<uintptr_t> seen;
    for(int32_t i=0;i<count;++i){uintptr_t entry=0;std::string id;if(!at(data,size_t(i)*8,entry))return false;if(!entry||!seen.insert(entry).second)return fail("Startingpoint StartTech Mode window ownership duplicated");if(!text(entry,0x1b0,id))return false;
        if(id=="development"){if(out.address)return fail("Startingpoint StartTech DevelopmentWindow duplicated");out.address=entry;}}
    if(!out.address)return fail("Startingpoint StartTech owned DevelopmentWindow absent");
    uintptr_t addresspoint=0,logic=0;uint8_t visible=0;
    if(!at(out.address,0,addresspoint)||!at(out.address,0x318,logic)||!at(out.address,0x45f,visible)||
       !at(out.address,0x540,out.tabs)||!at(out.address,0x610,out.selected))return false;
    if(addresspoint!=native.image+0x1722e60||logic||visible!=1||!out.tabs||out.selected!=pay_receiver)
        return fail("Startingpoint StartTech actual window/selected catalogue owner differs");
    for(const auto& observed:reads){std::vector<unsigned char> after(observed.bytes.size());if(!memory.copy(observed.address,after.data(),after.size(),error))return false;if(after!=observed.bytes)return fail("Startingpoint StartTech UI owner changed during capture");}
    error.clear();return true;
}
}
