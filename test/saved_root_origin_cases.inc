#include <optional>
namespace saved_root_origin_fixture {
struct WorldState;struct LookupApi;
namespace opened=mt2mp::game::building_mixed_identity::opened_file;
#include "saved_root_batch_lifetime_type.inc"
#include "saved_root_origin_same.inc"
inline unsigned raw_reads=0,uid_reads=0,value_reads=0,final_reads=0;
inline opened::CurrentWorld native_world;
inline std::string raw_failure,member_failure;
inline bool change_on_finalize=false;
struct Proof {
    std::string failure;
    bool healthy(std::string& error)const{if(failure.empty())return true;error=failure;return false;}
    bool reject(std::string& error){if(failure.empty())failure=error;error=failure;return false;}
};
bool raw_current(opened::CurrentWorld& out,std::string& error){++raw_reads;if(!raw_failure.empty()){error=raw_failure;return false;}out=native_world;return true;}
struct RootSceneryReadBatch::State {
    Proof proof;
    std::optional<opened::CurrentWorld> cached_origin;
    uint64_t origin_revision=0;
    bool fresh_origin=false;
    bool finish_body(std::string& error){
        ++final_reads;
        if(!member_failure.empty()){error=member_failure;return false;}
        if(change_on_finalize)native_world.map=reinterpret_cast<void*>(99);
        return true;
    }
#include "saved_root_origin_state.inc"
};
struct NativeOrigin {bool current(opened::CurrentWorld&,std::string&);};
#include "saved_root_batch_lifetime_body.inc"
#include "saved_root_origin_route.inc"
void reset(){raw_reads=uid_reads=value_reads=final_reads=0;raw_failure.clear();member_failure.clear();change_on_finalize=false;
    native_world={};native_world.game=reinterpret_cast<void*>(1);native_world.mode=reinterpret_cast<void*>(2);
    native_world.state=reinterpret_cast<void*>(3);native_world.world=reinterpret_cast<void*>(4);native_world.map=reinterpret_cast<void*>(5);
    native_world.uuid="current-snapshot";native_world.sha256=std::string(64,'a');native_world.world_epoch=7;native_world.generation=8;native_world.bytes=123;
}
bool object(NativeOrigin& origin,std::string& error){opened::CurrentWorld world;if(!origin.current(world,error))return false;++uid_reads;++value_reads;return true;}
}
void saved_root_origin_cases(){
    namespace f=saved_root_origin_fixture;
    std::string error;f::NativeOrigin origin;f::opened::CurrentWorld world;
    f::reset();need(origin.current(world,error)&&origin.current(world,error)&&f::raw_reads==2,"outside traversal native Origin stays uncached");
    f::reset();{
        f::RootSceneryReadBatch batch;
        for(unsigned i=0;i<100;++i)need(f::object(origin,error),"actual cached Origin route retains fresh per-object reads");
        need(f::raw_reads==1&&f::uid_reads==100&&f::value_reads==100,"one complete Origin admission does not cache object UID or values");
        need(batch.finish(error)&&f::raw_reads==3&&f::final_reads==1,"actual finalizer brackets complete owner validation with two uncached World proofs");
    }
    need(origin.current(world,error)&&f::raw_reads==4,"actual RAII exit restores uncached Origin route");
    f::reset();{
        f::RootSceneryReadBatch outer;need(origin.current(world,error),"Origin admission failed");
        {f::RootSceneryReadBatch inner;need(origin.current(world,error)&&f::raw_reads==1,"nested current route reuses exact outer Origin");}
        need(outer.finish(error)&&f::raw_reads==3,"nested RAII preserves full outer final validation");
    }
    for(unsigned field=0;field<10;++field){
        f::reset();f::RootSceneryReadBatch batch;need(origin.current(world,error),"fresh Origin setup failed");
        auto& changed=f::native_world;
        if(field==0)changed.game=reinterpret_cast<void*>(99);if(field==1)changed.mode=reinterpret_cast<void*>(99);
        if(field==2)changed.state=reinterpret_cast<void*>(99);if(field==3)changed.world=reinterpret_cast<void*>(99);if(field==4)changed.map=reinterpret_cast<void*>(99);
        if(field==5)++changed.generation;if(field==6)++changed.world_epoch;if(field==7)++changed.bytes;if(field==8)changed.uuid="foreign";if(field==9)changed.sha256=std::string(64,'b');
        need(!batch.finish(error)&&error.find("World changed")!=std::string::npos&&f::final_reads==0,"fresh final admission rejects every complete Origin field before owner publication");
        error.clear();need(!origin.current(world,error)&&error.find("World changed")!=std::string::npos,"original complete Origin failure remains sticky");
    }
    f::reset();{f::RootSceneryReadBatch batch;need(origin.current(world,error),"boundary setup failed");
        {t::PageReadScope original(t::PageBoundary::OriginalCall);}
        need(!origin.current(world,error)&&error.find("original native call")!=std::string::npos&&f::raw_reads==1,"actual native OriginalCall invalidates cached Origin before reuse");
        need(!batch.finish(error)&&f::final_reads==0,"native boundary failure blocks final publication");}
    f::reset();{f::RootSceneryReadBatch batch;f::raw_failure="original Framework World read failure";
        need(!origin.current(world,error)&&error==f::raw_failure,"actual admission preserves upstream native read failure");
        f::raw_failure.clear();need(!batch.finish(error)&&error=="original Framework World read failure","failed admission cannot recover as successful publication");}
    f::reset();{f::RootSceneryReadBatch batch;need(origin.current(world,error),"post-owner setup failed");f::change_on_finalize=true;
        need(!batch.finish(error)&&f::raw_reads==3&&error.find("World changed")!=std::string::npos,"uncached closing Origin rejects change during complete owner validation");}
    f::reset();{f::RootSceneryReadBatch batch;need(origin.current(world,error),"member failure setup failed");f::member_failure="original final UID removed";
        need(!batch.finish(error)&&error==f::member_failure&&f::raw_reads==2,"actual finalizer preserves owner failure and does not publish or continue native reads");}
    f::reset();{f::RootSceneryReadBatch batch;need(batch.finish(error)&&f::raw_reads==0,"unused empty traversal does not fabricate a native World proof");}
}
