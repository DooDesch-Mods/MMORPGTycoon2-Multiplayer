#pragma once
#include "runtime.h"
#include "lifetime.h"
#include "creation/native_calls.h"
#include "developer_staff/read.h"
#include "placement_ui_probe/ui_probe.h"
#include <mutex>
namespace mt2mp::game::bug_authority {
namespace creation=developer_staff::bug_creation;
struct SavedChildren {uintptr_t collection=0;std::vector<uintptr_t> children;};
struct Runtime {
    uintptr_t image=0;uint32_t thread=0;
    bool ready=false,locked=false,snapshot_armed=false,snapshot_host=false,snapshot_sealed=false;
    std::string cause,snapshot_uuid,snapshot_sha;
    Context context;Binding binding;uintptr_t tracker=0,lounge=0;
    Lifetimes lifetimes;
    std::vector<SavedChildren> saved;
    std::string action;
    std::set<std::string> confirmed_actions;
    uint64_t publication=0,random_occurrence=0,specific_occurrence=0,record_occurrence=0;
    uint64_t publication_random=0,publication_specific=0,publication_record=0;
    std::recursive_mutex gate;
    placement_ui_probe::ProcessMemory memory;
};
Runtime& runtime();
bool reject(const std::string&,std::string&);
bool native_code(std::string&);
bool install_native(std::string&);
creation::SetAnchor anchor_original();
bool create_replica(creation::NativeServices&,const creation::Lease&,const creation::Value&,creation::Receipt&);
bool native_remove(uintptr_t collection,uintptr_t bug,bool& attempted,std::string&);
bool local_members(uintptr_t collection,std::vector<uintptr_t>&,std::string&);
bool active_world(Context&,Binding&,uint64_t&,bool&,std::string&);
bool witness_birth(uintptr_t collection,uintptr_t bug,unsigned lane,std::string&);
bool witness_removal(uintptr_t tracker,uintptr_t bug,std::string&);
void original_registered(uintptr_t caller,void* context,int32_t id,void* object) noexcept;
void original_children(uintptr_t caller,void* reader,int32_t count) noexcept;
template<class T> bool field(uintptr_t object,size_t offset,T& value,std::string& error){
    if(!object||object>UINTPTR_MAX-offset||object+offset>UINTPTR_MAX-sizeof(T))return fail(error,"Bug local owner/address overflow");
    return runtime().memory.copy(object+offset,&value,sizeof value,error);
}
}
