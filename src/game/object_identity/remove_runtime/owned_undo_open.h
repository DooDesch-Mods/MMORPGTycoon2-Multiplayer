#pragma once
#include "copy/remove_native_contract.h"
#include <cstring>
#include <limits>

namespace mt2mp::remove_runtime {
namespace native=build_actions::private_native;
// Only the retained Remove owner may authorize an installed Undo.Open route.
bool read_owned_undo_open(uintptr_t image,native::UndoOpenFn&,std::string&);

inline bool validate_owned_undo_open_bytes(native::Memory& memory,uintptr_t target,uintptr_t original,std::string& error){
    if(!target||!original||target>(std::numeric_limits<uintptr_t>::max)()-14){error="remove_runtime_owned_Open_address_invalid";return false;}
    unsigned char entry[14]{},trampoline[20]{};
    if(!memory.copy(target,entry,sizeof entry,error)||!memory.copy(original,trampoline,sizeof trampoline,error))return false;
    constexpr unsigned char tail[]{0x28,0x48,0x83,0x3d,0xe2,0x10,0x50,0x01,0};
    constexpr unsigned char relocated[]{0x56,0x53,0x48,0x83,0xec,0x28,0xff,0x25,0,0,0,0};
    uintptr_t back=0;std::memcpy(&back,trampoline+12,sizeof back);
    if(entry[0]!=0xe9||std::memcmp(entry+5,tail,sizeof tail)){error="remove_runtime_owned_Open_entry_tail_differs";return false;}
    if(std::memcmp(trampoline,relocated,sizeof relocated)||back!=target+6){error="remove_runtime_owned_Open_trampoline_differs";return false;}
    return true;
}
}
