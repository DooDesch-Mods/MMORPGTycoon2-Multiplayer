#include "startingpoint_read.h"
#include <array>
#include <cstring>
#include <set>
#include <vector>

namespace mt2mp::game::initial_release {
namespace {
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool bytes(Memory& m,uintptr_t p,void* out,size_t size,std::string& e){
    if(!p||!size||p>UINTPTR_MAX-size)return fail(e,"startingpoint release memory range invalid");
    if(!m.copy(p,out,size,e)){if(e.empty())e="startingpoint release read failed without cause";return false;}return true;
}
template<class T>bool at(Memory& m,uintptr_t p,size_t off,T& out,std::string& e){
    if(!p||p>UINTPTR_MAX-off)return fail(e,"startingpoint release owner/offset invalid");
    return bytes(m,p+off,&out,sizeof(out),e);
}
bool type(Memory& m,uintptr_t p,uintptr_t expected,const char* label,std::string& e){
    uintptr_t actual=0;if(!at(m,p,0,actual,e))return false;
    return actual==expected||fail(e,std::string("startingpoint release vtable differs: ")+label);
}
bool key(Memory& m,uintptr_t p,size_t off,std::string& out,std::string& e){
    uintptr_t data=0;uint64_t size=0;
    if(!at(m,p,off,data,e)||!at(m,p,off+8,size,e))return false;
    if(size>200)return fail(e,"startingpoint release key bytes="+std::to_string(size)+" required<=200");
    std::array<char,201> buffer{};
    if(!bytes(m,data,buffer.data(),size_t(size)+1,e))return false;
    if(buffer[size]||std::memchr(buffer.data(),0,size_t(size)))return fail(e,"startingpoint release key terminator differs");
    out.assign(buffer.data(),size_t(size));return true;
}
bool guard(Memory& m,uintptr_t image,std::string& e){
    struct Entry {uintptr_t rva;std::array<unsigned char,16> bytes;};
    constexpr Entry entries[]={
        {0x4df220,{0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x20,0x31,0xdb,0x48,0x89,0xd7,0x8b}},
        {0x71e90,{0x57,0x56,0x53,0x48,0x83,0xec,0x20,0x48,0x89,0x51,0x70,0x48,0x89,0xce,0x48,0x89}},
        {0x4e34a0,{0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x81,0xec,0xc8}}};
    for(const auto& entry:entries){std::array<unsigned char,16> actual{};
        if(!bytes(m,image+entry.rva,actual.data(),actual.size(),e))return false;
        if(actual!=entry.bytes)return fail(e,"startingpoint release code guard differs RVA="+std::to_string(entry.rva));}
    return true;
}
}
bool read_startingpoint_release(Memory& m,uintptr_t image,uintptr_t game_state,
                               uintptr_t startingpoint,StartingpointRelease& out,std::string& e){
    out={};e.clear();
    if(!image||image>UINTPTR_MAX-0x1800000||!game_state||!startingpoint)
        return fail(e,"startingpoint release current binding absent");
    if(!guard(m,image,e)||!type(m,game_state,image+0x16de920,"GameState",e)||
       !type(m,startingpoint,image+0x16dc330,"Startingpoint TechItem",e))return false;
    std::string tech_key;if(!key(m,startingpoint,0x20,tech_key,e))return false;
    if(tech_key!="startingpoint")return fail(e,"startingpoint release supplied Tech key differs");
    uintptr_t current_state=0,manager=0,current_manager=0,release=0;
    if(!at(m,image,0x13df2a0,current_state,e)||!at(m,game_state,0x190,manager,e)||
       !at(m,image,0x13df4f0,current_manager,e))return false;
    if(current_state!=game_state||!manager||manager!=current_manager)
        return fail(e,"startingpoint release GameState/ReleaseManager current owner differs");
    if(!type(m,manager,image+0x1714ed0,"ReleaseManager",e)||!at(m,manager,0x38,release,e)||
       !type(m,release,image+0x16c12b0,"Release",e))return false;
    StartingpointRelease sample;uintptr_t data=0;int32_t capacity=0;
    if(!at(m,release,0x150,data,e)||!at(m,release,0x158,sample.change_count,e)||
       !at(m,release,0x15c,capacity,e)||!at(m,release,0x1c4,sample.mutation_counter,e))return false;
    if(sample.change_count<0||capacity<sample.change_count||capacity>4096||(!data&&sample.change_count))
        return fail(e,"startingpoint release change bounds invalid count="+std::to_string(sample.change_count)+
            " capacity="+std::to_string(capacity)+" required 0<=count<=capacity<=4096");
    std::vector<uintptr_t> changes(size_t(sample.change_count));
    if(!changes.empty()&&!bytes(m,data,changes.data(),changes.size()*sizeof(uintptr_t),e))return false;
    std::set<uintptr_t> unique;
    for(auto change:changes){
        if(!change||!unique.insert(change).second)return fail(e,"startingpoint release change array has null/duplicate member");
        if(!type(m,change,image+0x1786d10,"Change",e))return false;
        uintptr_t owner=0;std::string feature_key;
        if(!at(m,change,0x70,owner,e)||!key(m,change,0x18,feature_key,e))return false;
        if(owner==startingpoint&&feature_key=="tech_release")++sample.feature_count;
    }
    out=sample;return true;
}
}
