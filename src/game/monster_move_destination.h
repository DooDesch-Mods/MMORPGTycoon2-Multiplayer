#pragma once
#include <array>
#include <cstdint>
#include <string>

namespace mt2mp::game::monster_move_destination {
// Ordinary Monsters retain their original Region when MapEntity's automatic
// Region flag is false. A geographic boundary does not change that owner.
template<class Read,class Locate>
bool check(uintptr_t object,uintptr_t shard,uintptr_t region,const std::array<float,3>& point,
           Read&& at,Locate&& locate,std::string& error){
    uintptr_t actual_shard=0,actual_region=0,region_shard=0,destination=0,destination_shard=0,dungeon=0;
    uint8_t automatic=2;
    if(!object||!shard||!region){error="Monster Move destination requires actual object/Shard/Region owners";return false;}
    if(!at(object,0x170,actual_shard,error)||!at(object,0x178,actual_region,error)||!at(object,0x490,automatic,error)||
       !at(region,0x70,region_shard,error))return false;
    if(actual_shard!=shard||actual_region!=region||region_shard!=shard){error="Monster Move destination original owner changed";return false;}
    if(automatic!=0){error="Monster Move destination requires the original fixed-Region Monster contract; automatic Region transfer is not admitted";return false;}
    if(!at(shard,0x18,dungeon,error))return false;
    if(dungeon){int32_t active=0;if(!at(dungeon,0x4c8,active,error))return false;
        if(active){error="Monster original MovedByCursor is suppressed by the active Dungeon template guard";return false;}}
    if(!locate(shard,point,destination,error))return false;
    if(!destination){error="Monster Move destination is outside the current original Shard";return false;}
    if(!at(destination,0x70,destination_shard,error))return false;
    if(destination_shard!=shard){error="Monster Move destination belongs to another Shard";return false;}
    return true;
}
}
