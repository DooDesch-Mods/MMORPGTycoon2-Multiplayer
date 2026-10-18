namespace actual_jobs {
namespace job_census=staff::job_census;
using Capture=staff::Capture;
size_t calls=0,fail_at=0,throw_at=0,change_at=0;
const job_census::Read copy=[](uintptr_t p,void* out,size_t n,std::string& e){
    ++calls;++copies;
    if(calls==fail_at)return fail(e,"original census read failure");
    if(calls==throw_at)throw std::runtime_error("original census read exception");
    if(calls==change_at)put(dev(0),0x4d0,uintptr_t(17));
    return memory.copy(p,out,n,e);
};
#include "scoped_jobs.inc"
}
void job_scope_cases(staff::Capture capture){
    namespace jobs=actual_jobs;namespace census=staff::job_census;
    const auto queue=image+0x17000,entries=image+0x18000,job=image+0x19000;
    put(fixture_state,0x1a8,queue);put(image,0x13df230,queue);put(queue,0,image+0x16db570);put(queue+0x18,0,image+0x16ef400);
    put(queue,0x20,entries);put(queue,0x28,int32_t(0));put(queue,0x2c,int32_t(4));
    std::string e;census::Census baseline,scoped;
    auto q=queries();jobs::calls=0;
    need(census::read(jobs::copy,capture,baseline,e),"original empty Job census: "+e);
    const auto raw_queries=queries()-q,raw_reads=jobs::calls;
    q=queries();jobs::calls=0;
    need(jobs::read_jobs(capture,scoped,e),e);
    need(scoped==baseline&&jobs::calls==raw_reads&&raw_reads==814,"actual double observations and all50 authorized Developer reads retained");
    need(raw_queries==814&&queries()-q<10,"pure census scope removes OS queries without removing byte reads");
    std::cout<<"empty Job census OS queries "<<raw_queries<<" -> "<<queries()-q<<", live reads="<<raw_reads<<'\n';
    need(timing::page_read_scope==nullptr,"census helper returns before caller can mutate");
    q=queries();jobs::calls=0;for(int i=0;i<8;++i)need(jobs::read_jobs(capture,scoped,e),e);
    need(jobs::calls==6512&&queries()-q<80,"eight steady-frame census invocations retain6512 reads with fresh scopes");
    put(entries,0,job);put(queue,0x28,int32_t(1));put(job,0,image+0x17427a0);put(job,0x18,int32_t(1));put(job,0xc4,uint8_t(1));
    put(job,0xb8,dev(0));put(dev(0),0x4c8,job);capture.state.rows[0].job_link=true;
    jobs::calls=0;q=queries();need(census::read(jobs::copy,capture,baseline,e),e);const auto linked_reads=jobs::calls,linked_queries=queries()-q;
    jobs::calls=0;q=queries();need(jobs::read_jobs(capture,scoped,e),e);
    need(scoped==baseline&&jobs::calls==linked_reads&&scoped.jobs.size()==1&&scoped.jobs[0].reciprocal&&scoped.developers[0].reciprocal,"actual linked Job/Developer reads and reciprocity unchanged");
    need(queries()-q<10&&linked_queries==linked_reads,"linked census retains reads with metadata reuse");
    put(job,0xc4,uint8_t(0));need(jobs::read_jobs(capture,scoped,e)&&!scoped.jobs[0].valid,"later independent census sees changed native Job value");
    put(dev(0),0x4c8,uintptr_t(0));capture.state.rows[0].job_link=false;put(queue,0x28,int32_t(0));
    jobs::calls=0;jobs::change_at=408;
    need(!jobs::read_jobs(capture,scoped,e)&&e=="Job census changed between two observations","second census detects live non-expected-field change");
    jobs::change_at=0;put(dev(0),0x4d0,uintptr_t(0));
    put(dev(0),0x4c4,int32_t(1));need(!jobs::read_jobs(capture,scoped,e)&&e=="Job census Developer changed from authorized pool","authorized pool transition failure retained");put(dev(0),0x4c4,int32_t(0));
    jobs::calls=0;jobs::fail_at=200;need(!jobs::read_jobs(capture,scoped,e)&&e=="original census read failure","original failed copy cause retained");jobs::fail_at=0;
    jobs::calls=0;jobs::throw_at=200;bool caught=false;
    try{jobs::read_jobs(capture,scoped,e);}catch(const std::runtime_error& x){caught=std::string(x.what())=="original census read exception";}
    jobs::throw_at=0;need(caught&&timing::page_read_scope==nullptr,"original exception unwinds helper without suppression");
    DWORD old=0;need(VirtualProtect(reinterpret_cast<void*>(queue),4096,PAGE_NOACCESS,&old)!=0,"change queue protection after helper returns");
    need(!jobs::read_jobs(capture,scoped,e)&&e=="error: native placement UI page unreadable","post-boundary unreadable queue retains real adapter error");
    DWORD ignored=0;need(VirtualProtect(reinterpret_cast<void*>(queue),4096,old,&ignored)!=0,"restore owned queue page");
    need(jobs::read_jobs(capture,scoped,e)&&timing::page_read_scope==nullptr,"post-boundary census freshly succeeds");
}
