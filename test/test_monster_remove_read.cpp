#include "monster_remove_fixture.h"
#include "monster_remove_read.h"
#include "direct_monster_identity.h"
#include <cstring>
#include <iostream>
namespace rr=mt2mp::game::monster_remove_read;
namespace di=mt2mp::game::direct_monster_identity;
namespace mz=mt2mp::game::monster_zone_read;
struct Memory:mt2mp::npc_census::Memory {
 std::map<uintptr_t,unsigned char> data;
 template<class T>void set(uintptr_t p,T v){auto bytes=reinterpret_cast<const unsigned char*>(&v);for(size_t i=0;i<sizeof v;++i)data[p+i]=bytes[i];}
 bool copy(uintptr_t p,void* out,size_t n,std::string& e)override{auto bytes=static_cast<unsigned char*>(out);for(size_t i=0;i<n;++i){auto f=data.find(p+i);if(f==data.end()){e="fixture unreadable address="+std::to_string(p+i);return false;}bytes[i]=f->second;}return true;}
};
int main(){try{std::string e;unsigned n=0;di::Registry ids;di::Scope scope{snapshot_id,3,4,10,20,30};di::Entry entry;
 need(ids.stage(birth_id,scope,100,0x100000001ull,e),e);need(!ids.lookup(birth_id,scope,entry,e)&&!entry.uid,"pending birth exposed");++n;
 need(ids.confirm(birth_id,e)&&ids.lookup(birth_id,scope,entry,e),e);auto current=entry;
 auto bad=scope;++bad.generation;need(!ids.lookup(birth_id,bad,entry,e)&&!entry.pointer,"stale generation accepted");++n;
 bad=scope;++bad.manager;need(!ids.lookup(birth_id,bad,entry,e),"wrong manager accepted");++n;
 auto wrong=current;wrong.uid=1;need(!ids.retire(birth_id,wrong,e),"UID high word ignored");++n;
 wrong=current;++wrong.pointer;need(!ids.retire(birth_id,wrong,e),"reused pointer accepted");++n;
 need(ids.retire(birth_id,current,e)&&ids.tombstone(birth_id,current),e);need(!ids.lookup(birth_id,scope,entry,e),"retired birth exposed");++n;
 std::string key;need(!ids.resolve(scope,100,current.uid,key,e,birth_id),"pending bypass resurrected tombstone");++n;
 need(!ids.retire(birth_id,current,e)&&!ids.stage(birth_id,scope,100,2,e),"retirement or birth UUID replay accepted");++n;
 need(ids.stage(action_id,scope,100,0x200000001ull,e),"new full UID could not reuse freed address");need(!ids.stage(snapshot_id,scope,200,current.uid,e),"retired full UID reused");++n;
 mz::State before,after;before.manager=after.manager=1;before.monster_manager=after.monster_manager=2;
 before.created_monsters=4;after.created_monsters=5;before.next_monster_tag=after.next_monster_tag=7;
 mz::Monster one,two;one.uid=9;one.pointer=100;two.uid=10;two.pointer=200;before.monsters={one,two};after.monsters={two};
 need(rr::removed_one(before,after,9,e),e);++n;
 std::map<uint64_t,std::string> names;const auto read_name=[](uintptr_t p,std::string& out,std::string&){out=p==100?"Deathblood":"Survivor";return true;};
 need(rr::read_names(before,read_name,names,e)&&rr::unchanged_names(names,after,9,read_name,e),e);++n;
 need(!rr::unchanged_names(names,after,9,[](uintptr_t,std::string& out,std::string&){out="Changed";return true;},e),"changed unrelated name accepted");++n;
 auto invalid=[&](auto f){auto a=before,b=after;f(a,b);need(!rr::removed_one(a,b,9,e),"invalid native removal census accepted");++n;};
 invalid([](auto&,auto& b){b.created_monsters=4;});invalid([](auto&,auto& b){++b.next_monster_tag;});
 invalid([](auto&,auto& b){++b.monsters[0].level;});invalid([](auto& a,auto& b){b.monsters=a.monsters;});
 invalid([](auto& a,auto&){a.monsters.push_back(a.monsters[0]);});invalid([](auto&,auto& b){++b.monster_manager;});
 std::cout<<n<<" actual identity, roster cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
