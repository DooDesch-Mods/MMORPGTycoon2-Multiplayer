#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "apply_timing.h"
#include <unordered_map>
#include <cstring>
namespace mt2mp::game::live_host_state::apply_timing {
namespace detail {
template<auto Query,class Clock=std::chrono::steady_clock>
inline SIZE_T query(LPCVOID address,PMEMORY_BASIC_INFORMATION information,SIZE_T length){
    if(!take_depth)return Query(address,information,length);
    const DWORD incoming=GetLastError();
    const auto started=Clock::now();
    SetLastError(incoming);
    const SIZE_T result=Query(address,information,length);
    const DWORD error=GetLastError();
    const auto ns=uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-started).count());
    record(Part::MemoryVirtualQuery,ns);
    SetLastError(error);
    return result;
}
}
// Only synchronous reader bodies open this scope. Stored entries contain page
// metadata, never the bytes, class identities or owners checked by each reader.
class PageReadScope;
inline thread_local PageReadScope* page_read_scope=nullptr;
class PageReadScope {
    friend SIZE_T virtual_query(LPCVOID,PMEMORY_BASIC_INFORMATION,SIZE_T);
    PageReadScope* previous_=nullptr;
    bool enabled_=false;
    std::unordered_map<uintptr_t,MEMORY_BASIC_INFORMATION> pages_;
    MEMORY_BASIC_INFORMATION last_span_{};
    bool last_span_valid_=false;
    void clear()noexcept{pages_.clear();last_span_valid_=false;}
public:
    explicit PageReadScope(bool enabled=true):enabled_(enabled){
        if(!enabled_)return;
        previous_=page_read_scope;
        if(previous_)previous_->clear();
        page_read_scope=this;
    }
    PageReadScope(const PageReadScope&)=delete;
    PageReadScope& operator=(const PageReadScope&)=delete;
    ~PageReadScope(){if(enabled_){page_read_scope=previous_;if(previous_)previous_->clear();}}
};
inline SIZE_T virtual_query(LPCVOID address,PMEMORY_BASIC_INFORMATION information,SIZE_T length){
    auto* scope=page_read_scope;
    if(!scope||!information||length!=sizeof(MEMORY_BASIC_INFORMATION))return detail::query<::VirtualQuery>(address,information,length);
    const DWORD incoming=GetLastError();
    static const DWORD page_size=[]{SYSTEM_INFO info{};GetSystemInfo(&info);return info.dwPageSize;}();
    const auto pointer=reinterpret_cast<uintptr_t>(address);
    const auto page=page_size?pointer-pointer%page_size:pointer;
    if(scope->last_span_valid_){
        const auto& span=scope->last_span_;
        const auto start=reinterpret_cast<uintptr_t>(span.BaseAddress);
        if(span.RegionSize<=UINTPTR_MAX-start&&page>=start&&page-start<span.RegionSize){
            std::memcpy(information,&span,sizeof *information);
            information->BaseAddress=reinterpret_cast<void*>(page);
            information->RegionSize=span.RegionSize-(page-start);
            SetLastError(incoming);return sizeof *information;
        }
    }
    const auto found=scope->pages_.find(page);
    if(found!=scope->pages_.end()){
        std::memcpy(information,&found->second,sizeof *information);
        SetLastError(incoming);return sizeof *information;
    }
    SetLastError(incoming);
    const auto result=detail::query<::VirtualQuery>(address,information,length);
    const DWORD error=GetLastError();
    constexpr DWORD readable=PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
    if(page_size&&result==sizeof *information&&error==incoming&&information->State==MEM_COMMIT&&
       !(information->Protect&(PAGE_GUARD|PAGE_NOACCESS))&&(information->Protect&readable)){
        scope->pages_.emplace(page,*information);
        const auto start=reinterpret_cast<uintptr_t>(information->BaseAddress);
        if(start==page&&information->RegionSize>=page_size&&information->RegionSize<=UINTPTR_MAX-start){
            scope->last_span_=*information;scope->last_span_valid_=true;
        }
    }
    SetLastError(error);return result;
}
}
