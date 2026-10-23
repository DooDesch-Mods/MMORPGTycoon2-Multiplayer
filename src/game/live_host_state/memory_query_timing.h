#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "apply_timing.h"
#include <map>
#include <cstring>
namespace mt2mp::game::live_host_state::apply_timing {
class PageReadScope;
inline thread_local PageReadScope* page_read_scope=nullptr;
enum class PageBoundary {Reader,OriginalCall};
inline thread_local uint64_t original_call_revision=0;
inline bool reader_query_scope()noexcept;
namespace detail {
template<auto Query,class Clock=std::chrono::steady_clock>
inline SIZE_T query(LPCVOID address,PMEMORY_BASIC_INFORMATION information,SIZE_T length,Part category=Part::QueryOther){
    if(!take_depth&&!host_capture_depth)return Query(address,information,length);
    const bool scoped=page_read_scope!=nullptr;
    const auto phase=query_phase;
    const auto management=management_query_parts;
    const bool reader_scoped=reader_query_scope();
    const DWORD incoming=GetLastError();
    const auto started=Clock::now();
    SetLastError(incoming);
    const SIZE_T result=Query(address,information,length);
    const DWORD error=GetLastError();
    const auto ns=uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-started).count());
    if(host_capture_depth){record(Part::HostMemoryVirtualQuery,ns);record(host_query_part,ns);}
    if(take_depth){
        record(Part::MemoryVirtualQuery,ns);
        record(category,ns);
        if(management.all!=Part::Count){record(Part::ClientQueryManagementTotalOs,ns);record(management.all,ns);}
        if(category==Part::QueryWorldProp){
            record(reader_scoped?Part::WorldPropScopedOs:Part::WorldPropUnscopedOs,ns);
            if(management.all!=Part::Count)record(reader_scoped?management.world_prop_scoped:management.world_prop_unscoped,ns);
        }
        if(category==Part::QueryPlacement){
            record(scoped?Part::PlacementScopedOs:Part::PlacementUnscopedOs,ns);
            record(scoped?scoped_phase_part(phase):unscoped_phase_part(phase),ns);
        }
    }
    SetLastError(error);
    return result;
}
}
// Only synchronous reader bodies open this scope. Stored entries contain page
// metadata, never the bytes, class identities or owners checked by each reader.
class PageReadScope {
    friend SIZE_T virtual_query(LPCVOID,PMEMORY_BASIC_INFORMATION,SIZE_T,Part);
    PageReadScope* previous_=nullptr;
    bool enabled_=false;
    PageBoundary boundary_=PageBoundary::Reader;
    std::map<uintptr_t,MEMORY_BASIC_INFORMATION> spans_;
    MEMORY_BASIC_INFORMATION last_span_{};
    bool last_span_valid_=false;
    bool prefix_attempted_=false;
    void clear()noexcept{spans_.clear();last_span_valid_=false;prefix_attempted_=false;}
    void clear_parent()noexcept{
        if(!previous_)return;
        if(take_depth)record(boundary_==PageBoundary::OriginalCall?Part::OriginalNestedReset:Part::ReaderNestedReset,0);
        previous_->clear();
    }
public:
    explicit PageReadScope(bool enabled=true,PageBoundary boundary=PageBoundary::Reader):enabled_(enabled),boundary_(boundary){
        if(!enabled_)return;
        if(boundary_==PageBoundary::OriginalCall&&original_call_revision!=UINT64_MAX)++original_call_revision;
        previous_=page_read_scope;
        clear_parent();
        page_read_scope=this;
    }
    explicit PageReadScope(PageBoundary boundary):PageReadScope(true,boundary){}
    // A reentrant reader cannot borrow metadata from an original-call body.
    static bool reader_active()noexcept{return page_read_scope&&page_read_scope->boundary_==PageBoundary::Reader;}
    PageReadScope(const PageReadScope&)=delete;
    PageReadScope& operator=(const PageReadScope&)=delete;
    ~PageReadScope(){if(enabled_){page_read_scope=previous_;clear_parent();}}
};
inline bool reader_query_scope()noexcept{return PageReadScope::reader_active();}
inline SIZE_T virtual_query(LPCVOID address,PMEMORY_BASIC_INFORMATION information,SIZE_T length,Part category=Part::QueryOther){
    auto* scope=page_read_scope;
    // Mixed original bodies can change mappings between their own reads.
    if(!scope||scope->boundary_==PageBoundary::OriginalCall||!information||length!=sizeof(MEMORY_BASIC_INFORMATION))return detail::query<::VirtualQuery>(address,information,length,category);
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
    // VirtualQuery describes the complete suffix of a homogeneous region.
    // Keep earlier regions available when this pure reader alternates allocations.
    auto found=scope->spans_.upper_bound(page);
    const auto following=found;
    if(found!=scope->spans_.begin()){
        --found;
        const auto& span=found->second;
        const auto start=found->first;
        if(span.RegionSize<=UINTPTR_MAX-start&&page-start<span.RegionSize){
            std::memcpy(information,&span,sizeof *information);
            information->BaseAddress=reinterpret_cast<void*>(page);
            information->RegionSize=span.RegionSize-(page-start);
            scope->last_span_=span;scope->last_span_valid_=true;
            SetLastError(incoming);return sizeof *information;
        }
    }
    constexpr DWORD readable=PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY;
    MEMORY_BASIC_INFORMATION prefix{};
    bool prefix_used=false;
    SIZE_T result=0;
    DWORD error=incoming;
    // A lower address may share the allocation of a retained suffix. Ask the OS
    // for that prefix once per cache lifetime; AllocationBase alone is not proof.
    if(page_size&&!scope->prefix_attempted_&&following!=scope->spans_.end()){
        const auto& known=following->second;
        const auto allocation=reinterpret_cast<uintptr_t>(known.AllocationBase);
        const auto known_start=following->first;
        if(known.Type==MEM_PRIVATE&&allocation&&allocation<page&&page<known_start&&known.RegionSize<=UINTPTR_MAX-known_start){
            scope->prefix_attempted_=true;
            SetLastError(incoming);
            const auto queried=detail::query<::VirtualQuery>(known.AllocationBase,&prefix,sizeof prefix,category);
            const auto prefix_error=GetLastError();
            if(take_depth)record(Part::PageScopeMiss,0);
            const auto start=reinterpret_cast<uintptr_t>(prefix.BaseAddress);
            if(queried==sizeof prefix&&prefix_error==incoming&&start==allocation&&
               prefix.RegionSize>=page_size&&prefix.RegionSize<=UINTPTR_MAX-start&&
               page>=start&&page-start<prefix.RegionSize&&prefix.State==MEM_COMMIT&&
               !(prefix.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&(prefix.Protect&readable)&&
               start+prefix.RegionSize==known_start+known.RegionSize&&
               prefix.AllocationBase==known.AllocationBase&&prefix.AllocationProtect==known.AllocationProtect&&
               prefix.State==known.State&&prefix.Protect==known.Protect&&prefix.Type==known.Type){
                std::memcpy(information,&prefix,sizeof *information);
                information->BaseAddress=reinterpret_cast<void*>(page);
                information->RegionSize=prefix.RegionSize-(page-start);
                result=sizeof *information;prefix_used=true;
            }
        }
    }
    if(!prefix_used){
        SetLastError(incoming);
        result=detail::query<::VirtualQuery>(address,information,length,category);
        error=GetLastError();
        if(take_depth)record(Part::PageScopeMiss,0);
    }
    if(page_size&&result==sizeof *information&&error==incoming&&information->State==MEM_COMMIT&&
       !(information->Protect&(PAGE_GUARD|PAGE_NOACCESS))&&(information->Protect&readable)){
        const auto start=reinterpret_cast<uintptr_t>(information->BaseAddress);
        if(start==page&&information->RegionSize>=page_size&&information->RegionSize<=UINTPTR_MAX-start){
            if(take_depth&&following!=scope->spans_.end()){
                const auto& known=following->second;
                const auto known_start=following->first;
                if(known_start>start&&known_start-start<information->RegionSize&&
                   known.RegionSize<=UINTPTR_MAX-known_start&&
                   start+information->RegionSize==known_start+known.RegionSize&&
                   information->AllocationBase==known.AllocationBase&&
                   information->AllocationProtect==known.AllocationProtect&&
                   information->State==known.State&&information->Protect==known.Protect&&
                   information->Type==known.Type)
                    record(Part::PageScopeBackwardOverlap,0);
            }
            const auto& retained=prefix_used?prefix:*information;
            scope->spans_.emplace(reinterpret_cast<uintptr_t>(retained.BaseAddress),retained);
            scope->last_span_=retained;scope->last_span_valid_=true;
        }
    }
    SetLastError(error);return result;
}
}
