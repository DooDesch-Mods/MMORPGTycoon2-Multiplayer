#include "runtime_internal.h"
namespace mt2mp::game::bug_authority {
bool work_birth(uintptr_t bug,uintptr_t job,std::string& error){
    auto& r=runtime();Context context;Binding binding;uint64_t publication=0;bool active=false;
    if(!active_world(context,binding,publication,active,error))return false;
    if(!active)return true;
    if(context.role!=action_session::Role::Host)return fail(error,"Client original Bug.MakeJob ran outside authoritative replica path");
    if(!work_bindings(error))return false;
    const auto* local=r.lifetimes.pointer(bug);uintptr_t back=0,parent=0,ap=0;int32_t type=0;
    if(!local||!job)return fail(error,"Original Bug.MakeJob lacks witnessed Bug and returned Job");
    if(!field(bug,0x48,back,error)||!field(bug,0x50,parent,error)||!field(job,0,ap,error)||!field(job,0x18,type,error))return false;
    uintptr_t selected=0;if(!field(job,0x38,selected,error))return false;
    if(back!=job||selected!=bug||parent!=local->parent||ap!=r.image+0x17427a0||type!=7)
        return fail(error,"Original Bug.MakeJob reciprocal type/parent readback differs");
    if(r.job_publication!=publication&&r.action.empty()){r.job_publication=publication;r.job_occurrence=0;}
    auto& occurrence=r.action.empty()?r.job_occurrence:r.job_action_occurrence;
    if(occurrence==UINT64_MAX)return fail(error,"Bug Job original birth occurrence exhausted");
    Birth birth{r.action.empty()?BirthKind::Publication:BirthKind::Action,r.action.empty()?context.snapshot_uuid:r.action,r.action.empty()?publication:0,++occurrence};
    JobRow row;row.bug=bug_identity(local->row);row.birth=std::move(birth);
    return r.jobs.born({std::move(row),job,bug,parent,false},error);
}
bool work_saved(uintptr_t job,uintptr_t context,int32_t link,std::string& error){
    auto& r=runtime();if(!r.snapshot_armed)return true;
    uintptr_t ap=0,actual=0;if(!field(job,0,ap,error)||!field(job,8,actual,error))return false;
    if(ap!=r.image+0x17427a0||(!r.snapshot_host&&actual!=context)||!context||link<=0)return fail(error,"Original Job snapshot registration class/context/linkId differs");
    for(const auto& saved:r.saved_jobs)if(saved.job==job||saved.link==link)return fail(error,"Original Job snapshot receiver/linkId repeated");
    r.saved_jobs.push_back({job,context,link});return true;
}
}
