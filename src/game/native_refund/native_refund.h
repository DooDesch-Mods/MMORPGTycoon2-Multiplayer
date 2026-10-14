#pragma once
#include "object_identity/remove_runtime/copy/world_prop_reader.h"
#include <cstdint>
#include <string>
namespace mt2mp::game::native_refund {
struct Receipt {
    bool attempted=false,returned=false,confirmed=false;
    int32_t value=0;
    uint32_t seh=0;
    uintptr_t getter=0;
};
using Invoke=uint32_t(*)(uintptr_t,uintptr_t,int32_t&);
uint32_t invoke_actual(uintptr_t,uintptr_t,int32_t&);
bool read(build_actions::private_native::Memory&,uintptr_t,uint32_t,uintptr_t,uint64_t,uintptr_t,Receipt&,std::string&,Invoke=invoke_actual);
}
