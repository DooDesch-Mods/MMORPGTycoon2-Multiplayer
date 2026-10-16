#pragma once
#include "job_lifetime.h"
#include "typed_action/bug_fix_action.h"
namespace mt2mp::game::bug_authority::manual_action {
inline const Row* selected(const State& bugs,const action_session::BugFixIntent& intent){
    for(const auto& row:bugs.rows)if(bug_identity(row)==intent.bug)return &row;return nullptr;
}
template<class Copy>
bool resolve_intent(const action_session::BugFixIntent& intent,const State& bugs,Lifetimes& lifetimes,JobLifetimes& jobs,
                    Copy&& copy,uintptr_t& pointer,std::string& e){
    pointer=0;const auto* row=selected(bugs,intent);
    if(!row||!row->value.known)return fail(e,"BugButton selected known Bug birth no longer exists");
    auto* local=lifetimes.identity(*row);if(!local)return fail(e,"BugButton selected native occurrence is missing");
    if(local->bug>UINTPTR_MAX-0x50)return fail(e,"BugButton selected Job field address overflow");
    uintptr_t job=0;if(!copy(local->bug+0x48,&job,sizeof job,e))return false;
    if(intent.schedule){if(intent.expected_job||job)return fail(e,"BugButton Schedule became stale: selected Bug already has a Job");}
    else{const auto* work=jobs.pointer(job);
        if(!job||!work||!intent.expected_job||work->row.birth!=*intent.expected_job||work->row.bug!=intent.bug)
            return fail(e,"BugButton Cancel became stale: selected Job birth changed");}
    pointer=local->bug;return true;
}
}
