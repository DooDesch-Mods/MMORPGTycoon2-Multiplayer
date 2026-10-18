#include <array>
#include <cstring>

void span_cases(){
    struct Allocations {
        std::array<void*,8> rows{};
        ~Allocations(){for(auto pointer:rows)if(pointer&&!VirtualFree(pointer,0,MEM_RELEASE))
            std::cerr<<"owned span allocation release failed "<<GetLastError()<<'\n';}
    } owned;
    SYSTEM_INFO system{};GetSystemInfo(&system);
    const size_t page=system.dwPageSize, pages=32;
    for(auto& pointer:owned.rows){pointer=VirtualAlloc(nullptr,page*pages,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        need(pointer!=nullptr,"owned multi-span allocation");}
    const auto address=[&](size_t row,size_t index){return static_cast<unsigned char*>(owned.rows[row])+page*index+17;};
    {
        t::Scope take(t::Part::Take);
        const auto before=count(t::Part::MemoryVirtualQuery);
        const auto first_miss=count(t::Part::PageScopeMiss),first_backward=count(t::Part::PageScopeBackwardOverlap);
        {
            t::PageReadScope pure;
            for(size_t index=0;index<pages;++index)for(size_t row=0;row<owned.rows.size();++row){
                MEMORY_BASIC_INFORMATION expected{},actual{};
                SetLastError(753);need(VirtualQuery(address(row,index),&expected,sizeof expected)==sizeof expected,"direct multi-span metadata");
                const auto expected_error=GetLastError();SetLastError(753);
                need(t::virtual_query(address(row,index),&actual,sizeof actual,t::Part::QueryPlacement)==sizeof actual,"wrapped multi-span metadata");
                need(std::memcmp(&expected,&actual,sizeof actual)==0&&GetLastError()==expected_error,"all native metadata fields and LastError equal on interior page");
            }
            std::cout<<"alternating region queries="<<count(t::Part::MemoryVirtualQuery)-before<<" required="<<owned.rows.size()<<'\n';
            need(count(t::Part::MemoryVirtualQuery)==before+owned.rows.size(),"alternating 256 pages need only eight OS region queries");
            need(count(t::Part::PageScopeMiss)==first_miss+owned.rows.size()&&count(t::Part::PageScopeBackwardOverlap)==first_backward,"ascending reads count first misses without backward overlap");
            const auto before_boundary=count(t::Part::MemoryVirtualQuery);
            DWORD old=0;
            {
                t::PageReadScope original_boundary;
                need(VirtualProtect(static_cast<unsigned char*>(owned.rows[0])+page*12,page,PAGE_NOACCESS,&old)!=0,"owned callback boundary changes one page");
            }
            MEMORY_BASIC_INFORMATION actual{};
            need(t::virtual_query(address(0,12),&actual,sizeof actual,t::Part::QueryPlacement)==sizeof actual&&actual.Protect==PAGE_NOACCESS,"outer interval cleared after original boundary");
            need(t::virtual_query(address(0,12),&actual,sizeof actual,t::Part::QueryPlacement)==sizeof actual&&actual.Protect==PAGE_NOACCESS,"unreadable interval remains uncached");
            need(count(t::Part::MemoryVirtualQuery)==before_boundary+2,"unreadable pages each query OS");
            for(const size_t index:{size_t(11),size_t(13)}){
                MEMORY_BASIC_INFORMATION expected{};
                need(VirtualQuery(address(0,index),&expected,sizeof expected)==sizeof expected,"direct split-region metadata");
                need(t::virtual_query(address(0,index),&actual,sizeof actual,t::Part::QueryPlacement)==sizeof actual&&
                    std::memcmp(&expected,&actual,sizeof actual)==0,"cached region never crosses changed protection boundary");
            }
            need(count(t::Part::PageScopeBackwardOverlap)==first_backward,"scope reset and split protection are not backward overlap");
        }
        MEMORY_BASIC_INFORMATION actual{};
        const auto after_scope=count(t::Part::MemoryVirtualQuery);
        need(t::virtual_query(address(1,2),&actual,sizeof actual,t::Part::QueryPlacement)==sizeof actual&&
            count(t::Part::MemoryVirtualQuery)==after_scope+1,"scope exit discards all intervals");
        {
            t::PageReadScope overlaps;
            const auto before_overlap=count(t::Part::MemoryVirtualQuery);
            const auto misses=count(t::Part::PageScopeMiss),backward=count(t::Part::PageScopeBackwardOverlap);
            for(const size_t index:{size_t(16),size_t(5),size_t(24),size_t(7),size_t(31)}){
                MEMORY_BASIC_INFORMATION expected{};actual={};
                SetLastError(971);need(VirtualQuery(address(1,index),&expected,sizeof expected)==sizeof expected,"direct overlapping suffix metadata");
                const auto expected_error=GetLastError();SetLastError(971);
                need(t::virtual_query(address(1,index),&actual,sizeof actual)==sizeof actual&&
                    std::memcmp(&expected,&actual,sizeof actual)==0&&GetLastError()==expected_error,"overlapping suffixes preserve native BaseAddress and RegionSize");
                // Alternate another native allocation to exercise the interval lookup.
                need(t::virtual_query(address(2,0),&actual,sizeof actual)==sizeof actual,"alternate overlapping lookup span");
            }
            need(count(t::Part::MemoryVirtualQuery)==before_overlap+3,"overlapping suffixes need two region queries plus alternating allocation");
            need(count(t::Part::PageScopeMiss)==misses+3&&count(t::Part::PageScopeBackwardOverlap)==backward+1,"one descending suffix counted among unchanged three OS misses");
            MEMORY_BASIC_INFORMATION expected{};SetLastError(421);
            const auto expected_result=VirtualQuery(address(1,7),&expected,3);
            const auto expected_error=GetLastError();SetLastError(421);actual={};
            const auto before_failure=count(t::Part::MemoryVirtualQuery);
            need(t::virtual_query(address(1,7),&actual,3)==expected_result&&GetLastError()==expected_error&&
                std::memcmp(&expected,&actual,sizeof actual)==0,"nonstandard length preserves exact native result/error/output");
            need(count(t::Part::MemoryVirtualQuery)==before_failure+1,"nonstandard length never uses cached metadata");
            need(count(t::Part::PageScopeMiss)==misses+3&&count(t::Part::PageScopeBackwardOverlap)==backward+1,"nonstandard query bypass is not a scoped cache miss");
        }
        {
            t::PageReadScope descending;
            const auto os=count(t::Part::MemoryVirtualQuery),misses=count(t::Part::PageScopeMiss);
            const auto backward=count(t::Part::PageScopeBackwardOverlap);
            for(size_t i=pages;i>0;--i){
                MEMORY_BASIC_INFORMATION expected{};actual={};
                SetLastError(673);
                need(VirtualQuery(address(3,i-1),&expected,sizeof expected)==sizeof expected,"direct descending suffix metadata");
                const auto expected_error=GetLastError();SetLastError(673);
                need(t::virtual_query(address(3,i-1),&actual,sizeof actual)==sizeof actual&&
                     std::memcmp(&expected,&actual,sizeof actual)==0&&GetLastError()==expected_error,
                     "descending miss counters preserve all OS metadata and LastError");
            }
            need(count(t::Part::MemoryVirtualQuery)==os+2&&count(t::Part::PageScopeMiss)==misses+2&&
                 count(t::Part::PageScopeBackwardOverlap)==backward+1,
                 "descending homogeneous allocation uses two queries and one backward overlap");
            need(t::virtual_query(address(3,31),&actual,sizeof actual)==sizeof actual&&
                 count(t::Part::PageScopeMiss)==misses+2&&count(t::Part::PageScopeBackwardOverlap)==backward+1,
                 "later cache hit does not count another miss or overlap");
            {t::PageReadScope native(t::PageBoundary::OriginalCall);}
            need(t::virtual_query(address(3,0),&actual,sizeof actual)==sizeof actual&&
                 count(t::Part::PageScopeMiss)==misses+3&&count(t::Part::PageScopeBackwardOverlap)==backward+1,
                 "native invalidation creates a fresh miss without a retained suffix witness");
        }
        need(t::totals[size_t(t::Part::PageScopeMiss)].total_ns==0&&
             t::totals[size_t(t::Part::PageScopeBackwardOverlap)].total_ns==0,
             "suffix diagnostics are event counts without invented timing");
        need(t::snapshot().contains("page_scope_miss")&&t::snapshot().contains("page_scope_backward_overlap"),
             "existing status includes suffix miss counters");
        try{t::PageReadScope unwind;
            need(t::virtual_query(address(1,3),&actual,sizeof actual)==sizeof actual,"prime exception scope");
            throw std::runtime_error("owned scope unwind");
        }catch(const std::runtime_error& e){need(std::string(e.what())=="owned scope unwind","original exception retained");}
        need(t::page_read_scope==nullptr,"exception removes all scope state");
        partition();
    }
    {t::PageReadScope outside;MEMORY_BASIC_INFORMATION info{};
        const auto misses=count(t::Part::PageScopeMiss),backward=count(t::Part::PageScopeBackwardOverlap);
        need(t::virtual_query(address(4,20),&info,sizeof info)==sizeof info&&t::virtual_query(address(4,1),&info,sizeof info)==sizeof info,"outside Take descending reads still execute");
        need(count(t::Part::PageScopeMiss)==misses&&count(t::Part::PageScopeBackwardOverlap)==backward,"outside Take reads do not add suffix diagnostic events");
    }
    // The original attribution cases below start with their own zeroed counters.
    t::totals={};
}
