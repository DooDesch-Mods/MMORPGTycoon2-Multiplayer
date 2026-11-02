#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace mt2mp::game::toon_weapon_visual::animation_observation {
struct Active {
    uintptr_t instance=0,definition=0;
    uint32_t time_bits=0,speed_bits=0,duration_bits=0;
    int32_t mode=0;uint8_t playing=0,paused=0,direction=0;
    std::string name;
    std::vector<int32_t> catalog_indexes,semantic_aliases;
    bool operator==(const Active&)const=default;
};
struct State {
    uintptr_t clips_data=0,active_data=0;
    int32_t clips_count=0,clips_capacity=0,active_count=0,active_capacity=0,effective_index=-1;
    std::array<int32_t,49> semantics{};
    std::vector<uintptr_t> clips;
    std::vector<Active> active;
    bool operator==(const State&)const=default;
};
template<class Read,class Name>
bool sample(Read& read,Name& name,uintptr_t character,uintptr_t actor,State& out,std::string& error){
    out={};
    const auto fail=[&](const char* cause){if(error.empty())error=cause;return false;};
    if(!character||!actor||!read(actor,0x18,out.clips_data,error)||!read(actor,0x20,out.clips_count,error)||
       !read(actor,0x24,out.clips_capacity,error)||!read(actor,0x30,out.active_data,error)||
       !read(actor,0x38,out.active_count,error)||!read(actor,0x3c,out.active_capacity,error))return fail("Animation owner arrays unreadable");
    if(out.clips_count<0||out.clips_count>4096||out.clips_capacity<out.clips_count||
       out.active_count<0||out.active_count>64||out.active_capacity<out.active_count||
       (out.clips_count&&!out.clips_data)||(out.active_count&&!out.active_data)||
       out.clips_data>UINTPTR_MAX-size_t(out.clips_count)*sizeof(uintptr_t)||
       out.active_data>UINTPTR_MAX-size_t(out.active_count)*sizeof(uintptr_t))return fail("Animation owner array bounds differ");
    out.clips.resize(size_t(out.clips_count));
    for(size_t i=0;i<out.clips.size();++i)if(!read(out.clips_data,i*sizeof(uintptr_t),out.clips[i],error))return false;
    for(size_t i=0;i<out.semantics.size();++i){
        if(!read(character,8+i*sizeof(int32_t),out.semantics[i],error))return false;
        if(out.semantics[i]>=out.clips_count)return fail("Animation semantic index outside current catalog");
    }
    std::set<uintptr_t> unique;
    for(int32_t i=0;i<out.active_count;++i){
        Active row;
        if(!read(out.active_data,size_t(i)*sizeof(uintptr_t),row.instance,error)||!row.instance||!unique.insert(row.instance).second)
            return fail("Animation active instance is null or duplicated");
        for(size_t j=0;j<out.clips.size();++j)if(out.clips[j]==row.instance)row.catalog_indexes.push_back(int32_t(j));
        if(row.catalog_indexes.empty())return fail("Animation active instance is absent from current catalog");
        for(size_t j=0;j<out.semantics.size();++j)
            if(out.semantics[j]>=0&&out.clips[size_t(out.semantics[j])]==row.instance)row.semantic_aliases.push_back(int32_t(j));
        if(!read(row.instance,0,row.definition,error)||!row.definition||!read(row.instance,8,row.time_bits,error)||
           !read(row.instance,0xc,row.speed_bits,error)||!read(row.instance,0x10,row.playing,error)||
           !read(row.instance,0x11,row.paused,error)||!read(row.instance,0x12,row.direction,error)||
           !read(row.definition,0x68,row.mode,error)||!read(row.definition,0x6c,row.duration_bits,error)||
           !name(row.definition,row.name,error))return fail("Animation instance or definition unreadable");
        if(row.name.size()>256||row.playing>1||row.paused>1||row.direction>1||row.mode<0||row.mode>3||
           !std::isfinite(std::bit_cast<float>(row.time_bits))||!std::isfinite(std::bit_cast<float>(row.speed_bits))||
           !std::isfinite(std::bit_cast<float>(row.duration_bits)))return fail("Animation native state is invalid");
        if(row.playing)out.effective_index=i;
        out.active.push_back(std::move(row));
    }
    error.clear();return true;
}
}
