#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace mt2mp::build_actions::private_native {
using DestroyProp=void(*)(void* selection,void* prop);
struct ConstPropArray {uintptr_t addresspoint=0;const uintptr_t* data=nullptr;int32_t count=0,capacity=0;};
static_assert(sizeof(ConstPropArray)==0x18);
static_assert(offsetof(ConstPropArray,data)==8&&offsetof(ConstPropArray,count)==0x10);
using SetHighlightedProps=void(*)(void* selection,const ConstPropArray*);
struct CallReceipt {bool attempted=false,returned=false;uint32_t seh_code=0;std::string error;};
// Entrypoints and receivers require an exact-build, world and lease preflight.
CallReceipt call_destroy(DestroyProp,uintptr_t selection,uintptr_t target);
CallReceipt call_highlights(SetHighlightedProps,uintptr_t selection,uintptr_t array_addresspoint,const std::vector<uintptr_t>& survivors);
}
