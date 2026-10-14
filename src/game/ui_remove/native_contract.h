#pragma once
#include "ui_capture.h"
#include <string_view>

namespace mt2mp::action_session::ui_capture {
struct Region {uintptr_t allocation_base=0;bool committed=false,executable=false,image=false,writable=false;};
class Memory {
public:
    virtual ~Memory()=default;
    virtual bool copy(uintptr_t,void*,size_t,std::string& cause)=0;
    virtual bool region(uintptr_t,Region&,std::string& cause)=0;
};
bool validate_unpatched(Memory&,uintptr_t image,std::string_view exact_exe_sha,std::string& cause);
bool validate_trampoline(Memory&,uintptr_t image,uintptr_t trampoline,std::string& cause);
bool validate_own_enabled_route(Memory&,uintptr_t image,uintptr_t trampoline,uintptr_t own_entry,std::string& cause);
// return_slot is the address returned by _AddressOfReturnAddress inside the sole
// DestroyProps entry. Offsets require the complete exact original frame guards.
bool classify_origin(Memory&,uintptr_t image,uintptr_t return_slot,Origin&,std::string& cause);
bool read_native_array(Memory&,uintptr_t image,NativePropArray*,ArrayObservation&,std::string& cause);
}
