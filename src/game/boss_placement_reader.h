#pragma once
#include "typed_action/boss_placement_action.h"
#include "npc_quest_targets.h"
#include <bit>
#include <cmath>
#include <cstring>

namespace mt2mp::game::boss_placement_reader {
namespace q=npc_quest_targets;
using Intent=action_session::BossPlacementIntent;
inline constexpr uintptr_t addresspoint=0x172f040,cursor_addresspoint=0x1786dd0;
inline constexpr uintptr_t release_rva=0x58b020;
struct Source {
    uintptr_t receiver=0,cursor=0,visual=0;
    std::array<unsigned char,0x6c> transform{};
    Intent intent;
    bool operator==(const Source&)const=default;
};
inline bool sha(const std::string& value){return value.size()==64&&value.find_first_not_of("0123456789abcdef")==std::string::npos;}
inline bool finite(uint32_t value){const auto real=std::bit_cast<float>(value);return std::isfinite(real)&&std::abs(real)<=100000000;}
inline bool valid(const Intent& value){
    if(value.type<0||value.region<0||!sha(value.character_sha))return false;
    for(auto component:value.pose)if(!finite(component))return false;
    for(auto component:value.point)if(!finite(component))return false;
    double norm=0;for(size_t index=0;index<4;++index){const auto real=std::bit_cast<float>(value.pose[index]);norm+=double(real)*real;}
    if(std::abs(norm-1)>0.001)return false;
    for(size_t index=7;index<10;++index)if(std::bit_cast<float>(value.pose[index])<=0)return false;
    return true;
}
inline bool transform(const Intent& value,std::array<unsigned char,0x6c>& out,std::string& error){
    out={};if(!valid(value))return q::fail(error,"Boss placement complete semantic transform or Character witness is invalid");
    std::memcpy(out.data(),value.pose.data(),sizeof(value.pose));out[0x28]=1;return true;
}
template<class Memory>bool current(Memory& memory,uintptr_t image,uintptr_t expected_cursor,uintptr_t& behaviour,std::string& error){
    uintptr_t cursor=0;behaviour=0;
    if(!q::at(memory,image,0x13df9d0,cursor,error)||!q::typed(memory,image,cursor,cursor_addresspoint,error)||
       !q::at(memory,cursor,0x190,behaviour,error))return false;
    return cursor==expected_cursor||q::fail(error,"Boss placement actual source Cursor changed");
}
// Resolve proves the selected original Character and the actual release-point
// Region. Both copies are read while the same local Cursor is still active.
template<class Memory,class Resolve>
bool read(Memory& memory,uintptr_t image,uintptr_t receiver,uintptr_t cursor,uintptr_t point,
          Resolve&& resolve,Source& out,std::string& error){
    out={};uintptr_t active=0;uint8_t enabled=0;
    if(!receiver||!point||!current(memory,image,cursor,active,error)||active!=receiver||
       !q::typed(memory,image,receiver,addresspoint,error)||!q::at(memory,receiver,0x3b1,enabled,error)||enabled!=1)
        return q::fail(error,error.empty()?"Boss release requires its active valid original Cursor":error);
    Source first;first.receiver=receiver;first.cursor=cursor;
    if(!q::at(memory,receiver,0x148,first.intent.type,error)||!q::at(memory,receiver,0x330,first.visual,error)||!first.visual||
       !q::at(memory,receiver,0x338,first.transform,error)||!q::at(memory,point,0,first.intent.point,error))
        return q::fail(error,error.empty()?"Boss native source visual or complete captured values are absent":error);
    std::memcpy(first.intent.pose.data(),first.transform.data(),sizeof(first.intent.pose));
    if(!resolve(first.intent,error)||!valid(first.intent))return q::fail(error,error.empty()?"Boss original selected Character or release Region is invalid":error);
    Source last=first;uint8_t again=0;
    if(!current(memory,image,cursor,active,error)||active!=receiver||!q::typed(memory,image,receiver,addresspoint,error)||
       !q::at(memory,receiver,0x3b1,again,error)||!q::at(memory,receiver,0x148,last.intent.type,error)||
       !q::at(memory,receiver,0x330,last.visual,error)||!q::at(memory,receiver,0x338,last.transform,error)||
       !q::at(memory,point,0,last.intent.point,error)||again!=1||last!=first)
        return q::fail(error,error.empty()?"Boss native source changed during complete capture":error);
    out=std::move(first);return true;
}
// Cancel only the captured local tool. A different active tool belongs to the
// player and remains active; no pointer from the request is used for cleanup.
template<class Backend>
bool cleanup(Backend& backend,const Source& source,std::string& error){
    uintptr_t active=0;if(!backend.current(source.cursor,active,error))return false;
    if(!backend.source(source,error))return false;
    if(active==source.receiver&&!backend.cancel(source.cursor,error))return false;
    if(!backend.clear(source.receiver,error)||!backend.current(source.cursor,active,error))return false;
    return active!=source.receiver||q::fail(error,"Boss original source cleanup retained the captured active tool");
}
}
