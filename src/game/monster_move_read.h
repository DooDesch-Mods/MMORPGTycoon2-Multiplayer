#pragma once
#include "monster_zone_read.h"

namespace mt2mp::game::monster_move_read {
struct Actor {
    uintptr_t character=0,actor=0,skeleton=0;
    bool on_map=false,visible=false;
    std::array<float,3> spawn{};
    bool in_view=false,distance_visible=false;
    bool operator==(const Actor&)const=default;
};
template<class Read>
bool actor(uintptr_t image,uintptr_t object,Read&& field,Actor& out,std::string& error){
    out={};uintptr_t ap=0,set=0,notify=0;uint8_t on=2,visible=2,in_view=2,distance_visible=2;
    if(!field(object,0,ap,error))return false;
    if(ap!=image+0x16c0770){error="Monster Move class differs";return false;}
    if(!field(ap,0x290,set,error)||!field(ap,0x2a0,notify,error))return false;
    if(set!=image+0x39a920||notify!=image+0x3b9eb0){error="Monster Move native virtual methods differ";return false;}
    if(!field(object,0x368,on,error)||!field(object,0x6f0,out.character,error)||!field(object,0x82c,out.spawn,error))return false;
    if(!out.character||!field(out.character,0,out.actor,error)||!out.actor||
       !field(out.actor,0x40,out.skeleton,error)||!out.skeleton){if(error.empty())error="Monster Move actor binding absent";return false;}
    if(!field(out.skeleton,0xe4,visible,error)||!field(object,0x1c0,in_view,error)||!field(object,0x808,distance_visible,error))return false;
    if(on>1||visible>1||in_view>1||distance_visible>1){error="Monster Move visibility outside boolean domain";return false;}
    for(float value:out.spawn)if(!std::isfinite(value)){error="Monster Move spawn point is not finite";return false;}
    out.on_map=on!=0;out.visible=visible!=0;out.in_view=in_view!=0;out.distance_visible=distance_visible!=0;return true;
}
inline bool same_actor(const Actor& a,const Actor& b){return a.character==b.character&&a.actor==b.actor&&a.skeleton==b.skeleton;}
// Skeleton visibility can change through local Character/renderer updates.
inline bool same_registered_actor(const Actor& a,const Actor& b){return same_actor(a,b)&&a.on_map==b.on_map&&a.spawn==b.spawn;}
template<class Read,class Invoke>
bool restore(uintptr_t image,uintptr_t object,Read&& field,Invoke&& original,
             Actor& before,Actor& after,unsigned& calls,std::string& error){
    calls=0;if(!actor(image,object,field,before,error))return false;
    if(!before.on_map&&before.visible){error="Monster Move off-map actor remains visible before original restore";return false;}
    if(!before.on_map){++calls;if(!original(object,error))return false;}
    if(!actor(image,object,field,after,error))return false;
    if(!after.on_map||(calls&&!after.visible)||!same_actor(before,after)||before.spawn!=after.spawn){
        error="Monster Move original SetOnMap did not restore stable registered actor";return false;}
    return true;
}
inline bool unchanged_others(const monster_zone_read::State& a,const monster_zone_read::State& b,
                             uint64_t uid,std::string& error){
    if(a.manager!=b.manager||a.monster_manager!=b.monster_manager||a.zones!=b.zones||
       a.monsters.size()!=b.monsters.size()||a.next_monster_tag!=b.next_monster_tag||a.created_monsters!=b.created_monsters){
        error="Monster Move changed manager, Zone, count or counters";return false;}
    unsigned matches=0;
    for(size_t i=0;i<a.monsters.size();++i){auto expected=a.monsters[i];const auto& actual=b.monsters[i];
        if(expected.uid==uid){++matches;expected.position=actual.position;expected.rotation=actual.rotation;expected.scale=actual.scale;}
        if(expected!=actual){error="Monster Move changed identity, order or another roster member";return false;}}
    if(matches!=1){error="Monster Move target not unique in before roster";return false;}
    return true;
}
}
