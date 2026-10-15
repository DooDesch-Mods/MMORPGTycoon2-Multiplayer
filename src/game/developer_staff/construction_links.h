#pragma once
#include "construction_calls.h"
#include "subscriber_lifecycle/cohort.h"
namespace mt2mp::game::subscriber_lifecycle_append {
struct DeveloperPairRequest {
    uintptr_t image=0,developer=0,job=0,developer_property=0,job_property=0;
    uint32_t thread=0;
    bool attach=false;
    std::string building_key;
    bool(*fresh)(void*,std::string&)=nullptr;
    void* user=nullptr;
};
struct DeveloperPairReceipt {
    developer_staff::construction::Call developer,job;
    mt2mp::subscriber_lifecycle::LookupReceipt lookup;
    bool attempted=false,confirmed=false,unknown=false,context_closed=false;
};
// Uses the existing installed borrowed-object lookup scope. Existing objects
// never enter a Context's AddObject list or its destructor's Resolve list.
bool bind_developer_pair(const DeveloperPairRequest&,DeveloperPairReceipt&,std::string&);
}
