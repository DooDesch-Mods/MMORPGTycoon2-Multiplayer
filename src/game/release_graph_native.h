#pragma once
#include "release_graph.h"
#include "subscriber_lifecycle/capture.h"
#include "subscriber_lifecycle/cohort.h"
#include "subscriber_lifecycle/record_decode.h"
#include "release_current_owner.h"
#include "release_demand_owner.h"
#include <functional>
namespace mt2mp::game::release_graph_native {
using Key=std::function<bool(uintptr_t,std::string&,std::string&)>;
struct Receipt {subscriber_lifecycle::CaptureReceipt original;bool confirmed=false;};
bool capture(uintptr_t image,subscriber_lifecycle::Memory&,uintptr_t manager,const Key&,release_graph::Value&,Receipt&,std::string&,release_graph::Kind kind=release_graph::Kind::Manager);
using Resolve=std::function<bool(const std::string&,uintptr_t&,std::string&)>;
using Guard=std::function<bool(std::string&)>;
struct ApplyReceipt {
    subscriber_lifecycle::first_append::DecodeReceipt records;
    subscriber_lifecycle::LookupReceipt lookup;
    release_current_owner::Receipt current;
    release_demand_owner::Receipt prepared_storage;
    bool context_constructed=false,load_attempted=false,load_returned=false,context_closed=false;
    bool postresolve_suppressed=false,readback_confirmed=false,old_release_retired=false,confirmed=false,unknown=false;
    uintptr_t retained_context=0,retained_record=0,old_release=0,new_release=0,retained_array=0;
    uint32_t exception=0;
};
using PostResolve=void(*)(void*,void*);
bool set_postresolve_original(PostResolve,std::string&);
void postresolve_entry(void*,void*);
// Called by the already installed ObjectContext Lookup owner before its ordinary path.
bool lookup(void* context,int32_t id,subscriber_lifecycle::Lookup original,void*& result);
bool apply(uintptr_t image,subscriber_lifecycle::Memory&,uintptr_t manager,const release_graph::Value&,
           const Key&,const Resolve&,const Guard&,ApplyReceipt&,std::string&);
}
