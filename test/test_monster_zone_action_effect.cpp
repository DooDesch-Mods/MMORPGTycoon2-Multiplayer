#include "monster_zone_effect.h"
#include <functional>
#include <iostream>
#include <stdexcept>
namespace z=mt2mp::game::monster_zone_read;
namespace effect=mt2mp::game::monster_zone_effect;
void need(bool v,const std::string& e){if(!v)throw std::runtime_error(e);}
z::State fresh(){z::State s;s.manager=100;s.monster_manager=200;return s;}
z::State spawned(){auto s=fresh();z::Zone zone;zone.value.pointer=300;zone.value.uid=11;
 zone.value.content.type=1;zone.value.content.level=1;zone.value.content.monster_type=0;
 for(int32_t i=0;i<4;++i){z::Monster m;m.pointer=400+uintptr_t(i);m.uid=12+uint64_t(i);m.zone=300;m.tag=i;
  m.slot=1;m.type=0;m.level=1;m.status=3;zone.monsters.push_back(m.uid);s.monsters.push_back(m);}
 s.zones.push_back(zone);s.next_monster_tag=4;s.created_monsters=4;return s;}
int main(){try{unsigned count=0;std::string e;const auto before=fresh(),after=spawned();
 need(effect::created(before,after,300,11,0,1,e),e);++count;
 auto zero=after;zero.monsters.clear();zero.zones[0].monsters.clear();zero.next_monster_tag=zero.created_monsters=0;
 need(effect::created(before,zero,300,11,0,1,e),e);++count;
 const std::vector<std::function<void(z::State&)>> bad{
  [](auto& s){++s.manager;},[](auto& s){++s.monster_manager;},[](auto& s){s.zones.clear();},
  [](auto& s){s.zones.push_back(s.zones[0]);},[](auto& s){++s.zones[0].value.uid;},
  [](auto& s){++s.zones[0].value.pointer;},[](auto& s){++s.zones[0].value.content.type;},
  [](auto& s){++s.zones[0].value.content.level;},[](auto& s){++s.zones[0].value.content.monster_type;},
  [](auto& s){s.zones[0].monsters.pop_back();},[](auto& s){++s.region_random[0];},
  [](auto& s){++s.next_monster_tag;},[](auto& s){++s.created_monsters;},
  [](auto& s){s.monsters[0].pointer=0;},[](auto& s){s.monsters[0].uid=0;},
  [](auto& s){s.monsters[1].pointer=s.monsters[0].pointer;},[](auto& s){s.monsters[1].uid=s.monsters[0].uid;},
  [](auto& s){++s.monsters[0].zone;},[](auto& s){++s.monsters[0].slot;},
  [](auto& s){++s.monsters[0].type;},[](auto& s){++s.monsters[0].level;},
  [](auto& s){++s.monsters[0].status;},[](auto& s){++s.monsters[0].rank;},
  [](auto& s){std::swap(s.monsters[0],s.monsters[1]);}};
 for(const auto& change:bad){auto s=after;change(s);need(!effect::created(before,s,300,11,0,1,e)&&!e.empty(),"invalid original effect accepted");++count;}
 auto old=before;old.zones.push_back(after.zones[0]);need(!effect::created(old,after,300,11,0,1,e),"old identity accepted");++count;
 std::cout<<count<<" actual original effect cases PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
