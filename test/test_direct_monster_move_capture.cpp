#include "direct_monster_fixture.h"
#include "move_kiss/move_kiss.h"
#include <cstring>
#include <iostream>
namespace mk=mt2mp::move_kiss;
namespace mt2mp::move_kiss {
bool execute_once(const Calls&,uintptr_t,const std::array<unsigned char,0x6c>&,Receipt&,std::string& e){e="unexpected native execution in capture";return false;}
}
struct Memory:mk::Memory {
 std::map<uintptr_t,unsigned char> bytes;
 template<class T>void put(uintptr_t p,const T& v){auto* b=reinterpret_cast<const unsigned char*>(&v);for(size_t n=0;n<sizeof(v);++n)bytes[p+n]=b[n];}
 bool read(uintptr_t p,void* out,size_t size,std::string& e)override{auto* b=static_cast<unsigned char*>(out);for(size_t n=0;n<size;++n){auto it=bytes.find(p+n);if(it==bytes.end()){e="missing capture field";return false;}b[n]=it->second;}return true;}
};
struct Services:mk::Services {
 mk::Family family=mk::Family::DirectMonster;mk::State before;std::string key=std::string("action:")+action_id,cause;
 unsigned submitted=0,bound=0;bool main=true,online=true,drift=false;mk::Intent captured;
 Services(){before.alive=true;before.rotation={0,0,0,1};before.scale={1,1,1};}
 bool main_thread()const noexcept override{return main;}
 bool active(mk::Origin& o,std::string&)override{if(!online)return false;o={snapshot_id,1,2,0x100};return true;}
 bool bind_target(const mk::Origin&,uintptr_t ghost,uintptr_t object,uint64_t uid,mk::State& s,std::string& k,mk::Family& f,std::string& e)override{
  if(ghost!=0x3000||object!=0x4000||uid!=0x100000001ull){e="capture exact identity differs";return false;}
  ++bound;s=before;k=key;f=drift&&bound==2?mk::Family::Npc:family;return true;}
 bool submit_move(const mk::Intent& i,std::string&)override{++submitted;captured=i;return true;}
 void reject(const std::string& e)noexcept override{cause=e;}
};
Memory fixture(){Memory m;constexpr uintptr_t image=0x140000000,b=0x1000;
 m.put(b,image+0x172f1f0);m.put(b+0x401,uint8_t(1));m.put(b+0x499,uint8_t(0));m.put(b+0x458,image+0x1747300);
 m.put(b+0x460,uintptr_t(0x2000));m.put(b+0x468,int32_t(1));m.put(b+0x46c,int32_t(1));m.put(0x2008,uintptr_t(0x3000));m.put(0x2010,uintptr_t(0x4000));
 m.put(0x4018,uint64_t(0x100000001ull));m.put(0x3000,image+0x1705230);m.put(0x3100,std::array<float,4>{0,0,0,1});m.put(0x3110,std::array<float,3>{10,20,30});m.put(0x311c,std::array<float,3>{1,1,1});return m;}
int main(){try{unsigned checks=0;
 for(auto family:{mk::Family::DirectMonster,mk::Family::Npc,mk::Family::Prop}){auto m=fixture();Services s;s.family=family;if(family==mk::Family::Prop){s.before.type="building";s.before.variant="test";}
  mk::Capture capture(m,s,0x140000000);need(capture.purchase(0x1000,0x5c2a20)==mk::Route::Suppress&&s.submitted==1&&s.captured.family==family&&s.captured.after.x==10,s.cause);++checks;}
 {auto m=fixture();Services s;s.family=mk::Family::Prop;mk::Capture c(m,s,0x140000000);need(c.purchase(0x1000,0x5c2a20)==mk::Route::Suppress&&!s.submitted&&s.cause=="move_registry_state_invalid","empty type inferred as NPC");++checks;}
 {auto m=fixture();Services s;s.key="saved:0";mk::Capture c(m,s,0x140000000);need(c.purchase(0x1000,0x5c2a20)==mk::Route::Suppress&&!s.submitted&&!s.cause.empty(),"Monster saved fallback accepted");++checks;}
 {auto m=fixture();Services s;s.drift=true;mk::Capture c(m,s,0x140000000);need(c.purchase(0x1000,0x5c2a20)==mk::Route::Suppress&&!s.submitted&&s.cause=="move_capture_witness_changed","family drift accepted");++checks;}
 {auto m=fixture();Services s;mk::Capture c(m,s,0x140000000);need(c.purchase(0x1000,0)==mk::Route::Forward&&!s.bound,"foreign caller captured");s.main=false;need(c.purchase(0x1000,0x5c2a20)==mk::Route::Forward&&!s.bound,"foreign thread captured");checks+=2;}
 {auto m=fixture();Services s;m.put(0x1499,uint8_t(1));mk::Capture c(m,s,0x140000000);need(c.purchase(0x1000,0x5c2a20)==mk::Route::Suppress&&!s.submitted&&s.cause=="move_duplicate_not_supported","duplicate captured");++checks;}
 std::cout<<checks<<" actual Move Purchase body cases passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
