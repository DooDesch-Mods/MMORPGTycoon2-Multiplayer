#pragma once
#include "bug_authority/job_calls.h"
#include "subscriber_lifecycle/cohort.h"
namespace mt2mp::game::subscriber_lifecycle_append {
struct BugPairRequest {
    uintptr_t image=0,bug=0,job=0,bug_property=0,job_property=0;
    uint32_t thread=0;
    bool attach=false;
    std::string occurrence_key;
    bool(*fresh)(void*,std::string&)=nullptr;
    void* user=nullptr;
};
struct BugPairReceipt {
    developer_staff::construction::Call bug,job;
    mt2mp::subscriber_lifecycle::LookupReceipt lookup;
    bool attempted=false,confirmed=false,unknown=false,context_closed=false;
};
// Uses the existing installed borrowed-object lookup scope. Existing objects
// never enter a Context's AddObject list or its destructor's Resolve list.
bool bind_bug_pair(const BugPairRequest&,BugPairReceipt&,std::string&);
}
