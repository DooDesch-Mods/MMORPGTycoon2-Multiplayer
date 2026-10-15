#include "admission_read.h"
#include "admission_code.h"
#include <array>
#include <set>
#include <cstring>

namespace mt2mp::game::startingpoint_adoption {
namespace {
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T> bool at(initial_release::Memory& m,uintptr_t p,size_t off,T& v,std::string& e){
    if(!p||p>UINTPTR_MAX-off||p+off>UINTPTR_MAX-sizeof(v))return fail(e,"startingpoint admission address range invalid");
    if(m.copy(p+off,&v,sizeof(v),e))return true;
    if(e.empty())e="startingpoint admission memory copy failed";
    return false;
}
bool ap(initial_release::Memory& m,uintptr_t p,uintptr_t wanted,std::string& e){
    uintptr_t actual=0;return at(m,p,0,actual,e)&&(actual==wanted||fail(e,"startingpoint admission owner address point differs"));
}
}
bool read_admission(initial_release::Memory& m,uintptr_t image,uintptr_t state,
                    uintptr_t tech,Admission& out,std::string& e){
    out={};e.clear();
    if(!image||image>UINTPTR_MAX-0x1b00000)return fail(e,"startingpoint admission image range invalid");
    for(const auto& guard:code_guards){std::array<unsigned char,16> actual{};
        if(!m.copy(image+guard.rva,actual.data(),actual.size(),e)){if(e.empty())e="startingpoint admission code read failed";return false;}
        if(actual!=guard.bytes)return fail(e,"startingpoint admission code differs RVA="+std::to_string(guard.rva));
    }
    uintptr_t global_state=0,manager=0,global_manager=0;
    if(!at(m,image,0x13df2a0,global_state,e)||!ap(m,state,image+0x16de920,e)||
       !at(m,state,0x190,manager,e)||!at(m,image,0x13df4f0,global_manager,e))return false;
    if(state!=global_state||!manager||manager!=global_manager)return fail(e,"startingpoint admission current State/ReleaseManager differs");
    if(!ap(m,manager,image+0x1714ed0,e)||!at(m,manager,0x38,out.release,e)||
       !ap(m,out.release,image+0x16c12b0,e)||!ap(m,tech,image+0x16dc330,e))return false;
    uintptr_t key=0;uint64_t key_size=0;std::array<char,14> name{};
    if(!at(m,tech,0x20,key,e)||!at(m,tech,0x28,key_size,e))return false;
    if(key_size!=13)return fail(e,"startingpoint admission Tech key length differs");
    if(!m.copy(key,name.data(),name.size(),e)){if(e.empty())e="startingpoint admission Tech key read failed";return false;}
    if(std::memcmp(name.data(),"startingpoint",14))return fail(e,"startingpoint admission Tech key differs");
    if(!at(m,tech,0xf9,out.repeatable,e)||!at(m,tech,0x160,out.modifiers,e)||
       !at(m,tech,0xe0,out.art_pack_bytes,e)||!at(m,tech,0x210,out.item_feature,e)||
       !at(m,image,0x13b9820,out.events_enabled,e))return false;
    uintptr_t head=0;uint64_t count=0;
    constexpr uintptr_t bucket=0x1a2dd00+0x38*0x38;
    if(!at(m,image,bucket+0x10,head,e)||!at(m,image,bucket+0x18,count,e))return false;
    if(count>64)return fail(e,"startingpoint admission event38 listener count exceeds64");
    std::set<uintptr_t> nodes,values;
    while(head){
        if(nodes.size()>=count||!nodes.insert(head).second)return fail(e,"startingpoint admission event38 linked list count/cycle differs");
        Listener row;uintptr_t next=0;
        if(!at(m,head,0,next,e)||!at(m,head,8,row.address,e))return false;
        if(!row.address||!values.insert(row.address).second)return fail(e,"startingpoint admission event38 null/duplicate listener");
        if(!at(m,row.address,0,row.address_point,e)||!at(m,row.address_point,0,row.handler,e))return false;
        out.listeners.push_back(row);head=next;
    }
    if(nodes.size()!=count)return fail(e,"startingpoint admission event38 linked list count differs");
    if(out.events_enabled!=1)return fail(e,"startingpoint admission game events disabled/invalid");
    if(out.repeatable||out.modifiers||out.art_pack_bytes||out.item_feature!=10)
        return fail(e,"startingpoint admission Tech has repeat/modifier/art/feature effects outside slice");
    // Every runtime listener is inspected. A static direct-caller scan is not an
    // exhaustive subscription receipt, and another Release could mutate too.
    if(out.listeners.size()!=1)return fail(e,"startingpoint admission event38 requires exactly current Release listener");
    const auto& row=out.listeners.front();
    if(row.address!=out.release+0x18||row.address_point!=image+0x16c1370||row.handler!=image+0x4e4cf0)
        return fail(e,"startingpoint admission event38 listener is not current Release subobject/thunk");
    return true;
}
}
