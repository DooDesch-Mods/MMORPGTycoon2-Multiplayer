#pragma once
#include "scenery_witness.h"
#include "prop_lease.h"
#include "native_open_witness.h"
#include <memory>

namespace mt2mp::build_actions::private_native::host {
namespace opened=mt2mp::game::building_mixed_identity::opened_file;
using FindUid=void*(*)(uint64_t);
using DynamicCast=void*(*)(void*,const void*,const void*,ptrdiff_t);
struct LookupApi {FindUid find=nullptr;DynamicCast cast=nullptr;};
bool resolve_lookup(Memory&,uintptr_t,std::string_view,LookupApi&,std::string&);
class ProcessMemory final:public Memory {public:bool copy(uintptr_t,void*,size_t,std::string&)override;};
struct Origin {
    virtual ~Origin()=default;
    virtual bool current(opened::CurrentWorld&,std::string&)=0;
};
// Its implementation calls the existing sealed NativeOpen runtime, not a ready flag.
class NativeOrigin final:public Origin {public:bool current(opened::CurrentWorld&,std::string&)override;};
struct WorldState {
    opened::CurrentWorld origin;
    uintptr_t root=0,selection=0,active_graph=0;
    int32_t saves=-1,edits=-1;
};
class RootSceneryReadBatch {
    struct State;
    std::shared_ptr<State> state_;
    RootSceneryReadBatch* previous_=nullptr;
    static thread_local RootSceneryReadBatch* current_;
    static bool reuse_current(opened::CurrentWorld&,bool&,std::string&);
    static bool remember_current(const opened::CurrentWorld&,std::string&);
    static bool reject_current(std::string&);
    static bool reuse_world(WorldState&,bool,uintptr_t,uint32_t,LookupApi,bool&,std::string&);
    friend class Reader;
public:
    RootSceneryReadBatch();
    ~RootSceneryReadBatch();
    RootSceneryReadBatch(const RootSceneryReadBatch&)=delete;
    RootSceneryReadBatch& operator=(const RootSceneryReadBatch&)=delete;
    bool finish(std::string&);
    static bool read_current(opened::CurrentWorld&,std::string&);
};
class Reader {
public:
    Reader(Memory& memory,Origin& origin,uintptr_t image,uint32_t thread,LookupApi api)
        :memory_(memory),origin_(origin),image_(image),thread_(thread),api_(api){}
    bool current(WorldState&,bool require_idle,std::string&);
    bool prop(void*,uint64_t expected_uid,mt2mp::prop_lease::Prop&,std::string&);
    bool saved_root_scenery(void*,uint64_t expected_uid,int32_t saved_region_id,uintptr_t saved_region,mt2mp::prop_lease::Prop&,std::string&);
    uint32_t thread()const noexcept{return thread_;}
private:
    Memory& memory_;Origin& origin_;uintptr_t image_;uint32_t thread_;LookupApi api_;
    bool busy_=false,world_busy_=false;
    bool region_member(const WorldState&,uintptr_t shard,uintptr_t region,std::string&);
    bool main_member(uintptr_t object,uintptr_t shard,uintptr_t region,std::string&);
    bool typed(uintptr_t object,uint64_t uid,uintptr_t type,std::string&);
};
bool same_world(const opened::CurrentWorld&,const opened::CurrentWorld&);
}
