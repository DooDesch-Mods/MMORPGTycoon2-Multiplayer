#include "direct_monster_fixture.h"
#include "direct_monster_identity.h"
#include "monster_move_read.h"
#include <map>
#include <cstring>
#include <iostream>
namespace id=mt2mp::game::direct_monster_identity;
namespace mr=mt2mp::game::monster_move_read;
struct Memory {
 std::map<uintptr_t,std::vector<unsigned char>> fields;
 template<class T>void put(uintptr_t p,const T& v){auto& b=fields[p];b.resize(sizeof(v));std::memcpy(b.data(),&v,sizeof(v));}
 template<class T>bool read(uintptr_t p,size_t o,T& v,std::string& e){auto it=fields.find(p+o);if(it==fields.end()||it->second.size()!=sizeof(v)){e="fixture field absent";return false;}std::memcpy(&v,it->second.data(),sizeof(v));return true;}
};
int main(){try{std::string e,key;unsigned checks=0;id::Registry registry;id::Scope scope{snapshot_id,1,2,0x100,0x200,0x300};id::Entry entry;
 constexpr uint64_t uid=0xdeadbeef00000001ull;need(registry.stage(action_id,scope,0x1000,uid,e),e);
 need(!registry.lookup(action_id,scope,entry,e),"pending birth accepted");need(registry.confirm(action_id,e),e);
 need(registry.lookup(action_id,scope,entry,e)&&entry.uid==uid&&entry.pointer==0x1000,e);checks+=3;
 for(int field=0;field<6;++field){auto wrong=scope;switch(field){case 0:wrong.snapshot="other";break;case 1:++wrong.epoch;break;case 2:++wrong.generation;break;case 3:++wrong.root;break;case 4:++wrong.region;break;default:++wrong.manager;}
  need(!registry.lookup(action_id,wrong,entry,e)&&entry.pointer==0,"stale scope accepted");++checks;}
 need(!registry.resolve(scope,0x1000,1,key,e),"low32 UID alias accepted");need(registry.resolve(scope,0x1000,uid,key,e)&&key==std::string("action:")+action_id,e);checks+=2;
 Memory m;constexpr uintptr_t image=0x140000000,object=0x1000,ap=image+0x16c0770;
 m.put(object,ap);m.put(ap+0x290,image+0x39a920);m.put(ap+0x2a0,image+0x3b9eb0);
 m.put(object+0x1c0,uint8_t(0));m.put(object+0x808,uint8_t(0));m.put(object+0x368,uint8_t(0));m.put(object+0x6f0,uintptr_t(0x2000));m.put(object+0x82c,std::array<float,3>{1,2,3});
 m.put(0x2000,uintptr_t(0x3000));m.put(0x3040,uintptr_t(0x4000));m.put(0x40e4,uint8_t(0));
 auto read=[&](uintptr_t p,size_t o,auto& v,std::string& x){return m.read(p,o,v,x);};
 unsigned calls=0,actual=0;mr::Actor before,after;auto invoke=[&](uintptr_t p,std::string&){need(p==object,"wrong original target");++actual;m.put(object+0x368,uint8_t(1));m.put(0x40e4,uint8_t(1));return true;};
 need(mr::restore(image,object,read,invoke,before,after,calls,e)&&calls==1&&actual==1&&after.visible,e);++checks;
 need(mr::restore(image,object,read,invoke,before,after,calls,e)&&calls==0&&actual==1,e);++checks;
 m.put(0x40e4,uint8_t(0));need(mr::restore(image,object,read,invoke,before,after,calls,e)&&calls==0&&actual==1&&!after.visible&&after.on_map,"local hidden registered actor was reactivated");++checks;
 auto render_changed=after;render_changed.visible=true;need(mr::same_registered_actor(after,render_changed),"local rendering changed registered identity");++checks;
 ++render_changed.skeleton;need(!mr::same_registered_actor(after,render_changed),"replaced skeleton accepted");++checks;
 m.put(0x40e4,uint8_t(1));m.put(object+0x368,uint8_t(0));need(!mr::restore(image,object,read,invoke,before,after,calls,e)&&calls==0,"inconsistent visibility mutated");++checks;
 m.put(object,uintptr_t(image+0x1742980));need(!mr::restore(image,object,read,invoke,before,after,calls,e)&&calls==0,"NPC class accepted");++checks;
 mt2mp::game::monster_zone_read::State first;first.manager=1;first.monster_manager=2;
 mt2mp::game::monster_zone_read::Monster row;row.pointer=object;row.uid=uid;first.monsters={row};auto last=first;last.monsters[0].position={4,5,6};
 need(mr::unchanged_others(first,last,uid,e),e);need(registry.lookup(action_id,scope,entry,e)&&entry.uid==uid,e);checks+=2;
 ++last.monsters[0].uid;need(!mr::unchanged_others(first,last,uid,e),"changed identity accepted");++checks;
 first.monsters.push_back(row);last=first;need(!mr::unchanged_others(first,last,uid,e),"duplicate target accepted");++checks;
 std::cout<<checks<<" Move identity/visibility/roster cases passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
