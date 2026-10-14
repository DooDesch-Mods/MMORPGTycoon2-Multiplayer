#pragma once
#include "native_catalog.h"
namespace mt2mp::scenery_kiss {
bool validate_ui_bytes(native::Memory&,uintptr_t image,const std::array<uintptr_t,6>& originals,const std::array<uintptr_t,6>& entries,bool enabled,std::string&);
bool permission_binding(native::Memory&,uintptr_t image,uintptr_t shard,uintptr_t region,uintptr_t art,uintptr_t& manager,std::string&);
bool validate_ui_originals(uintptr_t image,std::string&);
bool validate_ui_trampolines(uintptr_t image,const std::array<uintptr_t,6>&,std::string&);
bool validate_ui_routes(uintptr_t image,const std::array<uintptr_t,6>& originals,const std::array<uintptr_t,6>& entries,std::string&);
bool create_permission(native::host::ProcessMemory&,uintptr_t image,uintptr_t actual_root_shard,const State&,int32_t category,uintptr_t& manager,int32_t& cost,std::string&);
}
