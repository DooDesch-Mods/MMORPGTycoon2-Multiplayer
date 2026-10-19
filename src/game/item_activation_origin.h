#pragma once
#include <cstdint>

namespace mt2mp::game::item_activation_origin {
namespace detail {
struct Forwarded {uintptr_t image,item,caller;bool activate;};
inline thread_local const Forwarded* current=nullptr;
}

// Item::Activate tail-dispatches to its Type. The owning Item hook must carry
// its native caller across the extra trampoline call for this exact Item.
inline void forward(void(*original)(void*),void* item,uintptr_t image,uintptr_t caller,bool activate) {
    const detail::Forwarded frame{image,reinterpret_cast<uintptr_t>(item),caller,activate};
    const auto* previous=detail::current;detail::current=&frame;
#ifdef _MSC_VER
    __try {original(item);}
    __finally {detail::current=previous;}
#else
    try {original(item);}catch(...){detail::current=previous;throw;}
    detail::current=previous;
#endif
}

inline uintptr_t caller(uintptr_t image,uintptr_t end,uintptr_t item,uintptr_t physical) {
    if(!image||end<=image)return 0;
    if(physical>=image&&physical<end)return physical;
    const auto* frame=detail::current;
    if(!frame||!frame->activate||!item||frame->image!=image||frame->item!=item||
       frame->caller<image||frame->caller>=end)return 0;
    return frame->caller;
}
}
