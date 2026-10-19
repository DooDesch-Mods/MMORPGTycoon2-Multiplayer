#pragma once
#include "release_graph.h"
#include "subscriber_lifecycle/append_calls.h"
#include "subscriber_lifecycle/capture.h"
#include "subscriber_lifecycle/cohort.h"
#include "gm_ticket_storage.h"
#include "gm_ticket_state.h"
#include <functional>

namespace mt2mp::game::gm_ticket::native {
using Key=std::function<bool(uintptr_t,std::string&,std::string&)>;
using Resolve=std::function<bool(const std::string&,uintptr_t&,std::string&)>;
using Guard=std::function<bool(std::string&)>;
using Adopt=std::function<bool(const std::vector<uintptr_t>&,const std::vector<uintptr_t>&,std::string&)>;
struct CaptureReceipt {subscriber_lifecycle::CaptureReceipt original;bool confirmed=false;};
struct ApplyReceipt {
    subscriber_lifecycle::first_append::DecodeReceipt records;
    subscriber_lifecycle::LookupReceipt lookup;storage::Receipt storage_transfer;
    storage::Value previous_storage,fresh_storage;
    bool context_constructed=false,context_closed=false,load_attempted=false,load_returned=false;
    bool adopted=false,readback_confirmed=false,previous_retired=false,confirmed=false,unknown=false;
    uint32_t exception=0;uintptr_t retained_context=0,retained_record=0,retained_array=0;
};
bool capture(uintptr_t image,subscriber_lifecycle::Memory&,uintptr_t queue,const Key&,release_graph::Value&,CaptureReceipt&,std::string&);
bool apply(uintptr_t image,subscriber_lifecycle::Memory&,uintptr_t queue,const release_graph::Value&,const Key&,const Resolve&,const Guard&,const Adopt&,ApplyReceipt&,std::string&);
bool lookup(void* context,int32_t id,subscriber_lifecycle::Lookup original,void*& result);
uint32_t transform(uintptr_t image,uintptr_t object,const MasterValue&)noexcept;
uint32_t on_map(uintptr_t image,uintptr_t object,bool value)noexcept;
uint32_t shard(uintptr_t image,uintptr_t object,uintptr_t root)noexcept;
uint32_t spatial(uintptr_t image,uintptr_t object)noexcept;
uint32_t presentation(uintptr_t image,uintptr_t object,float delta)noexcept;
uint32_t pool(uintptr_t image,uintptr_t lounge,int32_t count)noexcept;
uint32_t compare(uintptr_t address,unsigned bytes,uint64_t expected,uint64_t desired,bool& stored)noexcept;
}
