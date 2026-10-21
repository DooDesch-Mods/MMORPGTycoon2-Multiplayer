namespace thought_destroy_fixture {
namespace a=mt2mp::action_session;namespace values=mt2mp::game::toon_thought;
namespace building_mixed_identity::opened_file{struct CurrentWorld{};}
namespace live::apply_timing{enum class PageBoundary{OriginalCall};struct PageReadScope{explicit PageReadScope(PageBoundary){}};}
struct Runtime {uintptr_t image=thought_lifetime_fixture::image_base;std::optional<a::Context> gm_context=a::Context{};std::optional<a::Binding> gm_binding=a::Binding{};bool gm_suspended=false;};
Runtime runtime_;
struct Published {Runtime* value=&runtime_;Runtime* load(std::memory_order){return value;}} published;
bool enabled=true,allow_world=true,allow_resolve=true,allow_remove=true,original_removes=false;size_t original_calls=0,corrected=0,deferred=0,failed=0;
std::string fault;constexpr uintptr_t object_identity=0x444444,proxy_identity=0x555555;
namespace world_zone_runtime {
struct Current{uintptr_t object=0;};
bool resolve_current(const a::Binding&,const std::string&,uintptr_t object,uint64_t,Current& out,std::string& e){if(!allow_resolve){e="fixture current Zone unavailable";return false;}out.object=object;return true;}
}
bool joined(Runtime&,a::Context&,a::Binding&,building_mixed_identity::opened_file::CurrentWorld&,std::string& e){if(!allow_world){e="fixture current World unavailable";return false;}return true;}
template<class T>bool at(Runtime&,uintptr_t object,size_t offset,T& value,std::string& e){return thought_lifetime_fixture::read(object,offset,value,e);}
bool ap(Runtime& r,uintptr_t object,uintptr_t required,std::string& e){uintptr_t value=0;return at(r,object,0,value,e)&&value==r.image+required;}
void retain_failure(const std::string& e){++failed;fault=e;}
void defer(const std::string& e){++deferred;need(!e.empty(),"Thought destruction guard lost cause");}
bool zone_current(Runtime&,const a::Context&,const a::Binding&,const world_zone_runtime::Current&,std::string& e){if(!allow_world){e="fixture current World unavailable";return false;}return true;}
bool remove(Runtime&,const a::Context&,const a::Binding&,const world_zone_runtime::Current& zone,uintptr_t object,bool& attempted,std::string& e){need(original_calls==1,"opaque cleanup ran before original D1 observers");attempted=true;if(!allow_remove){e="fixture original Zone removal failed";return false;}need(zone.object==thought_lifetime_fixture::zone_owner&&object==object_identity,"Thought D1 cleanup identity changed");return true;}
void original_destroy(void*){++original_calls;if(original_removes){thought_lifetime_fixture::put(thought_lifetime_fixture::list_data+8,uintptr_t{22});thought_lifetime_fixture::put(thought_lifetime_fixture::zone_owner+0x5a8,int32_t{2});}}
#include "thought_destroy_actual.inc"
void reset(){thought_lifetime_fixture::reset();published.value=&runtime_;runtime_={};enabled=allow_world=allow_resolve=allow_remove=true;original_removes=false;original_calls=corrected=deferred=failed=0;fault.clear();
    thought_lifetime_fixture::put(object_identity,runtime_.image+0x16fdef0);thought_lifetime_fixture::put(object_identity+0x88,thought_lifetime_fixture::zone_owner);thought_lifetime_fixture::put(object_identity+0x90,proxy_identity);thought_lifetime_fixture::put(proxy_identity,thought_lifetime_fixture::zone_owner);thought_lifetime_fixture::put(thought_lifetime_fixture::zone_owner+0x18,uint64_t{7});thought_lifetime_fixture::put(thought_lifetime_fixture::list_data+8,object_identity);}
void run(){reset();destroy(reinterpret_cast<void*>(object_identity));need(original_calls==1&&corrected==1&&!failed&&!deferred,"actual bound Thought D1 correction did not preserve original once");
    reset();runtime_.gm_context.reset();destroy(reinterpret_cast<void*>(1));need(original_calls==1&&!corrected&&!failed&&!deferred,"unbound Thought D1 touched coop ownership");
    reset();runtime_.gm_suspended=true;destroy(reinterpret_cast<void*>(1));need(original_calls==1&&!corrected&&!failed&&!deferred,"suspended Thought D1 touched coop ownership");
    reset();allow_world=false;destroy(reinterpret_cast<void*>(object_identity));need(original_calls==1&&deferred&&!failed,"unavailable World dropped original D1 or invented attempted cleanup");
    reset();allow_resolve=false;destroy(reinterpret_cast<void*>(object_identity));need(original_calls==1&&deferred&&!failed,"retiring/unavailable Zone dropped original D1");
    reset();allow_remove=false;destroy(reinterpret_cast<void*>(object_identity));need(original_calls==1&&failed==1&&!fault.empty(),"attempted cleanup failure lost permanent cause or original D1");
    reset();thought_lifetime_fixture::put(thought_lifetime_fixture::list_data+8,uintptr_t{77});destroy(reinterpret_cast<void*>(object_identity));need(original_calls==1&&!corrected&&!failed&&!deferred,"already removed Zone link replayed cleanup");
    reset();original_removes=true;destroy(reinterpret_cast<void*>(object_identity));need(original_calls==1&&!corrected&&!failed&&!deferred,"original indirect observer cleanup was repeated");
}
}
