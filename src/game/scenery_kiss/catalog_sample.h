#pragma once
#include "scenery_state.h"
namespace mt2mp::scenery_kiss {
struct CatalogSample {uint64_t uid=0;int32_t category=0,variant_hash=0;State state;};
bool read_catalog_sample(native::Memory&,Catalog&,uintptr_t image,uintptr_t object,uint64_t uid,uintptr_t actual_root,CatalogSample&,std::string&);
}
