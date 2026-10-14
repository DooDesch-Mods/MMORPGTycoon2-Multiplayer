#pragma once
#include "native_catalog.h"
namespace mt2mp::scenery_kiss {
bool move_code(native::Memory&,uintptr_t image,std::string&);
bool region_binding(native::Memory&,uintptr_t image,uintptr_t root,uintptr_t region,uintptr_t& manager,std::string&);
bool move_destination(native::host::ProcessMemory&,uintptr_t image,uintptr_t root,const State&,std::string&);
bool cancel_code(native::Memory&,uintptr_t image,std::string&);
bool cancel_before(native::Memory&,uintptr_t image,uintptr_t receiver,std::string&);
bool cancel_after(native::Memory&,uintptr_t receiver,std::string&);
}
