#pragma once
#include "release_edit_state.h"
#include <cstddef>
#include <algorithm>
#include <cstring>
namespace mt2mp::game::release_edit_native {
struct alignas(8) String {
    const char* data=nullptr;uint64_t length=0;std::array<char,16> storage{};
    explicit String(const std::string& value){length=value.size();if(length<storage.size()){std::copy(value.begin(),value.end(),storage.begin());data=storage.data();}else{data=value.data();std::memcpy(storage.data(),&length,sizeof length);}}
    String(const String&)=delete;String& operator=(const String&)=delete;
};
static_assert(sizeof(String)==32&&offsetof(String,storage)==16);
struct alignas(8) Returned {std::array<unsigned char,64> localized{};std::array<unsigned char,32> text{};};
uint32_t default_step(uintptr_t image,uintptr_t release,bool announcement,Returned&,unsigned step)noexcept;
uint32_t set_type(uintptr_t image,uintptr_t release,const std::array<int32_t,3>& version,int32_t type,uint32_t& returned)noexcept;
uint32_t assign(uintptr_t image,uintptr_t target,const String&)noexcept;
uint32_t custom(uintptr_t release,bool before,bool after)noexcept;
uint32_t reset(uintptr_t image,uintptr_t release,bool announcement)noexcept;
uint32_t query_reset(uintptr_t image,uintptr_t game,const String& key,const String& extra,const String& command)noexcept;
uint32_t refresh(uintptr_t image,uintptr_t window)noexcept;
uint32_t pane_set(uintptr_t image,uintptr_t pane,const String&)noexcept;
}
