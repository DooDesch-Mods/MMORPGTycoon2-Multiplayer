#pragma once
#include "record_decode.h"
#include "native_calls.h"
namespace mt2mp::subscriber_lifecycle::first_append {
using QuestResolve = void(*)(void*,void*);
using ProgressShift = void*(*)(void*,const void*,size_t);
struct AppendApi {
    DecodeApi records;
    Api calls;
    QuestResolve quest_resolve=nullptr;
    Unary progress_construct=nullptr,progress_destroy=nullptr;
    QuestResolve progress_npc_assign=nullptr,progress_add=nullptr;
    ProgressShift progress_shift=nullptr;
    Unary context_construct=nullptr, context_destroy=nullptr, context_abort=nullptr;
};
bool resolve_append_api(Memory&,uintptr_t image,AppendApi&,std::string&);
bool context_call(Unary,void*,CallReceipt&,std::string&);
bool quest_link_call(QuestResolve,void* link,void* context,CallReceipt&,std::string&);
bool allocate_progress(Allocate,uintptr_t&,CallReceipt&,std::string&);
bool shift_progress(ProgressShift,void*,const void*,size_t,CallReceipt&,std::string&);
bool registry_lookup(uintptr_t image,uint64_t uid,uintptr_t&,std::string&);
}
