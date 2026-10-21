#include "dungeon_scene_identity_runtime.h"
#include "mt2mp/game/native_engine.h"
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <utility>
namespace scene_scenery_native_fixture {
namespace a=mt2mp::action_session;
namespace c=mt2mp::game::dungeon_gizmo_control;
namespace dungeon_gizmo_control=mt2mp::game::dungeon_gizmo_control;
namespace dungeon_gizmo_identity=mt2mp::game::dungeon_gizmo_identity;
namespace native=mt2mp::game::native;
namespace runtime_types=mt2mp::game::dungeon_scene_identity_runtime;
using ControlCommand=c::Command;
struct NativeImage {uintptr_t image=0x10000000;};
inline NativeImage state;
inline bool fail(std::string& error,const std::string& cause){error=cause;return false;}
struct Memory {
    std::map<uintptr_t,unsigned char> fields;
    template<class T>void put(uintptr_t address,const T& value){const auto* bytes=reinterpret_cast<const unsigned char*>(&value);
        for(size_t i=0;i<sizeof value;++i)fields[address+i]=bytes[i];}
    bool copy(uintptr_t address,void* out,size_t size,std::string& error){auto* bytes=static_cast<unsigned char*>(out);
        for(size_t i=0;i<size;++i){const auto found=fields.find(address+i);
            if(found==fields.end())return fail(error,"controlled exact ReadMemory failure address="+std::to_string(address+i));bytes[i]=found->second;}
        return true;
    }
};
template<class T>bool field(Memory& memory,uintptr_t object,size_t offset,T& value,std::string& error){return memory.copy(object+offset,&value,sizeof value,error);}
struct Alias {uintptr_t object=0;uint64_t uid=0;std::string key;};
struct Boundary {
    runtime_types::Census current;
    std::map<uint64_t,native::BuildingState> scenery;
    std::vector<Alias> scenery_aliases,gizmo_aliases;
    std::vector<std::string> trace;std::string failure;
    int32_t price=35;bool allowed=true;uint32_t capability_exception=0;
    unsigned scenery_publications=0,gizmo_publications=0;
};
inline Boundary boundary;
inline bool step(const char* name,std::string& error){boundary.trace.emplace_back(name);
    return boundary.failure!=name||fail(error,std::string("controlled original ")+name+" cause");}
inline bool original_result(uint32_t code,const char* operation,std::string& error){return !code||fail(error,std::string(operation)+" SEH="+std::to_string(code));}
namespace scenery_kiss {
inline bool read_action_prop(uint64_t uid,native::BuildingState& out,std::string& error){
    if(!step("read-scenery",error))return false;const auto row=boundary.scenery.find(uid);
    if(row==boundary.scenery.end())return fail(error,"controlled native Scenery full UID absent");out=row->second;return true;
}
}
namespace dungeon_gizmo_control_calls {
inline uint32_t capability(uintptr_t,bool,int32_t& price,bool& allowed){boundary.trace.emplace_back("capability");price=boundary.price;allowed=boundary.allowed;return boundary.capability_exception;}
}
namespace remove_diagnostics {
namespace scene_scenery {
inline bool resolve(const runtime_types::World& world,uintptr_t object,uint64_t uid,std::string& key,std::string& error){
    if(!step("resolve-scenery",error))return false;
    if(world.uuid!=boundary.current.world.uuid||world.world_epoch!=boundary.current.world.world_epoch||world.generation!=boundary.current.world.generation)
        return fail(error,"controlled Scenery current native Scene lifetime differs");
    for(const auto& row:boundary.scenery_aliases)if(row.object==object&&row.uid==uid){key=row.key;return true;}
    return fail(error,"controlled Scenery canonical pointer/full UID absent");
}
}
inline bool refresh_moved_building(const std::string&,const std::string&,uint64_t,std::string& error){return step("refresh-scenery",error);}
namespace scene_buildings {
struct Row {std::string key,scene;uintptr_t object=0,shard=0,region=0,manager=0;uint64_t uid=0;bool scenery=false;};
struct Preview {bool prepared=false,checked=false;};
inline std::vector<Row> last_births,last_retired;
inline bool preview(const runtime_types::World&,const std::vector<Row>& births,const std::vector<Row>& retired,
                    const std::vector<Row>& restored,Preview& out,std::string& error){
    if(!step("preview-scenery",error))return false;
    if(!restored.empty())return fail(error,"controlled current action unexpectedly restores Scenery aliases");
    last_births=births;last_retired=retired;out.prepared=true;return true;
}
inline bool check(Preview& value,std::string& error){if(!value.prepared)return fail(error,"controlled Scenery alias preview absent");
    if(!step("check-scenery",error))return false;value.checked=true;return true;}
inline bool publish(Preview& value,std::string& error){if(!value.checked)return fail(error,"controlled Scenery alias check absent");
    if(!step("publish-scenery",error))return false;++boundary.scenery_publications;return true;}
}
}
namespace dungeon_scene_identity_runtime {
using Census=runtime_types::Census;
struct ClonePreview {bool prepared=false,checked=false;};
inline std::vector<std::pair<std::string,dungeon_gizmo_identity::Member>> last_births,last_retired;
inline bool resolve(uintptr_t object,uint64_t uid,std::string& key,std::string& error){
    if(!step("resolve-gizmo",error))return false;
    for(const auto& row:boundary.gizmo_aliases)if(row.object==object&&row.uid==uid){key=row.key;return true;}
    return fail(error,"controlled Gizmo canonical pointer/full UID absent");
}
inline bool lookup(const std::string& key,dungeon_gizmo_identity::Member& out,std::string& error){
    if(!step("lookup-gizmo",error))return false;
    for(const auto& alias:boundary.gizmo_aliases)if(alias.key==key){
        for(const auto& pool:boundary.current.pools)for(const auto& row:pool.local.members)if(row.object==alias.object&&row.uid==alias.uid){
            out={pool.scene.owner.key,row.object,pool.managers[3],pool.scene.owner.shard,pool.scene.owner.region,row.uid};return true;}}
    return fail(error,"controlled current Gizmo owner absent");
}
inline bool preview_clones(const runtime_types::World&,const std::vector<std::pair<std::string,dungeon_gizmo_identity::Member>>& births,
    const std::vector<std::pair<std::string,dungeon_gizmo_identity::Member>>& retired,ClonePreview& out,Census& actual,std::string& error){
    if(!step("preview-gizmos",error))return false;last_births=births;last_retired=retired;actual=boundary.current;out.prepared=true;return true;
}
inline bool check_clones(ClonePreview& value,std::string& error){if(!value.prepared)return fail(error,"controlled Gizmo alias preview absent");
    if(!step("check-gizmos",error))return false;value.checked=true;return true;}
inline bool publish_clones(ClonePreview& value,std::string& error){if(!value.checked)return fail(error,"controlled Gizmo alias check absent");
    if(!step("publish-gizmos",error))return false;++boundary.gizmo_publications;return true;}
}
struct Sample {
    struct Source {runtime_types::Census scenes;} source;
    std::map<std::string,std::pair<std::string,c::Scenery>> scenery;
};
// Only the publication/current-reader boundary is instantiated here. The full
// native ControlPort transaction remains covered by its existing consumer.
struct ControlPort {
    Memory& memory;const runtime_types::Pool* pool=nullptr;
    std::vector<mt2mp::game::duplicate_native::Handle> sources;
    std::vector<dungeon_gizmo_identity::Member> created;
    std::vector<bool> created_scenery;
};
#include "scene_scenery_native_body.inc"
}
