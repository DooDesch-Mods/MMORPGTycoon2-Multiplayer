#include "zone_memory_fixture.h"
namespace a=mt2mp::game::npc_zone_append;
const q::Budget budget{128,512,8192,4096};
const std::string action="01234567-89ab-cdef-0123-456789abcdef";
struct Test:Fixture {
 q::SavedPool saved;a::Receipt receipt;std::string e,key;uintptr_t added=0xb0000;
 Test(){w.origin={"fedcba98-7654-3210-fedc-ba9876543210",std::string(64,'a'),2,4};
  q::Array before;auto link=[](uintptr_t,int32_t& id,std::string&){id=42;return true;};
  require(q::observe_array(m,w.image,zm+0x48,budget,link,before,e),e);saved={w.origin,67,before,{}};}
 void begin(){require(a::begin(m,w,saved,budget,action,receipt,e),e);}
 void append(){for(size_t n=0;n<0x700;++n)m.data[added+n]=m.data.at(zone+n);
  m.put(added+0x18,uint64_t(77));m.put(zm+0x58,int32_t(2));m.put(zm+0x5c,int32_t(2));m.put(0x61008,added);}
 bool finish(){return a::finish(m,w,saved,budget,receipt,added,77,saved.array.zones[0].content,key,e);}
};
int main(){try{unsigned count=0;
 {Test t;t.begin();t.append();require(t.finish(),t.e);require(t.key=="action:"+action,"canonical action key");
  require(q::zone_key(t.saved,t.saved.array.zones[0])==std::string(64,'a')+":world:zone:42","saved key preserved");
  require(q::zone_key(t.saved,t.saved.array.zones[1])==t.key,"new key bound");
  require(q::validate_saved(t.m,t.w,t.saved,budget,t.e),t.e);
  require(!t.finish()&&t.e.find("already consumed")!=std::string::npos,t.e);++count;}
 auto bad=[&](auto mutate,const char* cause){Test t;t.begin();t.append();const auto prior=t.saved;mutate(t);
  require(!t.finish()&&t.e.find(cause)!=std::string::npos,std::string(cause)+": "+t.e);
  require(a::same_pool(prior,t.saved),"failed receipt must not publish pool");
  require(t.receipt.consumed,"failed receipt consumed");++count;};
 bad([](Test& t){++t.w.origin.world_generation;},"bound world");
 bad([](Test& t){++t.w.selection;},"bound world");
 bad([](Test& t){t.m.put(t.zone+0x560,int32_t(9));},"existing saved member");
 bad([](Test& t){t.m.put(0x61000,t.added);t.m.put(0x61008,t.zone);},"existing saved member");
 bad([](Test& t){t.m.put(t.added+0x18,uint64_t(99));},"identity/content");
 bad([](Test& t){t.m.put(t.added+0x178,uintptr_t(99));},"reciprocal owner");
 bad([](Test& t){t.m.put(t.added+0x560,int32_t(3));},"identity/content");
 bad([](Test& t){t.m.put(t.zm+0x58,int32_t(3));t.m.put(t.zm+0x5c,int32_t(3));t.m.put(0x61010,uintptr_t(0xc0000));t.m.put(0xc0018,uint64_t(88));},"unrecorded additional");
 bad([](Test& t){t.m.put(t.zm+0x58,int32_t(1));},"exactly one");
 {Test t;t.begin();t.append();require(!q::validate_saved(t.m,t.w,t.saved,budget,t.e)&&t.e=="quest zone is absent from the bound snapshot array",t.e);++count;}
 {Test t;t.begin();t.append();require(t.finish(),t.e);a::Receipt second;require(!a::begin(t.m,t.w,t.saved,budget,action,second,t.e)&&t.e=="Zone append action already bound",t.e);++count;}
 {Test t;require(!a::begin(t.m,t.w,t.saved,budget,"invalid",t.receipt,t.e)&&t.e=="Zone append action UUID invalid",t.e);++count;}
 {Test t;t.begin();t.append();auto required=t.saved.array.zones[0].content;require(!a::finish(t.m,t.w,t.saved,budget,t.receipt,t.zone,55,required,t.key,t.e)&&t.e.find("reuses existing")!=std::string::npos,t.e);++count;}
 {Test t;t.m.put(t.zone+0x560,int32_t(9));require(!a::begin(t.m,t.w,t.saved,budget,action,t.receipt,t.e)&&t.e=="quest zone saved membership/order/content changed",t.e);++count;}
 std::cout<<count<<" actual append checks passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
