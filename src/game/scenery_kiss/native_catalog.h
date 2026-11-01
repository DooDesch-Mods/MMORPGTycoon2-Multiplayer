#pragma once
#include "scenery_state.h"
#include "vendor/nlohmann/json.hpp"
#include "mt2mp/game/native_engine.h"
namespace mt2mp::build_actions::private_native::host {class ProcessMemory;}

namespace mt2mp::scenery_kiss {
// Code proof lives only across one synchronous identity census. Mutable
// manager, strings, object ownership and state remain per-read observations.
class CatalogCodeReadBatch {
    CatalogCodeReadBatch* previous_=nullptr;
    static thread_local CatalogCodeReadBatch* current_;
    uintptr_t image_=0;
    uint32_t thread_=0;
    uint64_t revision_=0;
    bool finished_=false,failed_=false;
    bool context(uintptr_t,std::string&);
public:
    CatalogCodeReadBatch();
    ~CatalogCodeReadBatch();
    CatalogCodeReadBatch(const CatalogCodeReadBatch&)=delete;
    CatalogCodeReadBatch& operator=(const CatalogCodeReadBatch&)=delete;
    static bool verify(native::Memory&,uintptr_t,std::string&);
    bool finish(std::string&);
};
class NativeCatalog final:public Catalog {
    native::host::ProcessMemory& memory_;uintptr_t image_;uint32_t thread_;
public:
    NativeCatalog(native::host::ProcessMemory& m,uintptr_t image,uint32_t thread):memory_(m),image_(image),thread_(thread){}
    bool identify(int32_t,int32_t,std::string&,std::string&,std::string&)override;
    bool classify_directory(const std::string&,bool& scenery,std::string&);
    bool resolve(const State&,int32_t& category,int32_t& variant_hash,std::string&);
};
bool resolve_ui_scenery_catalog(uintptr_t image,uint32_t thread,const std::string& type,const std::string& variant,bool& scenery,int32_t& category,int32_t& hash,std::string&);
// Building uses its existing reader; Scenery uses the same Root UID registry and Prop census.
bool read_native_scenery_samples(nlohmann::json&,std::string&);
bool read_action_prop(uint64_t uid,game::native::BuildingState&,std::string&);
}
