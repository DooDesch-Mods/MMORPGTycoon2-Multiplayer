#pragma once
#include "developer_staff/construction_calls.h"
namespace mt2mp::game::bug_authority {
using JobCall=developer_staff::construction::Call;
uint32_t job_link_property(uintptr_t image,uintptr_t property,uintptr_t receiver,
                          bool bug_property,bool attach,void* context,JobCall*);
struct JobBirthCall {
    JobCall allocate,construct,configure,insert;
    uintptr_t job=0;
};
// The caller pins all original bodies, queue membership and the selected Bug's
// birth/parent before each call. Created/fixed DailyCount events are not replayed.
uint32_t construct_job(uintptr_t image,JobBirthCall*);
uint32_t configure_job(uintptr_t image,uintptr_t job,uintptr_t bug,JobCall*);
uint32_t insert_job(uintptr_t image,uintptr_t queue,uintptr_t job,JobCall*);
uint32_t work_property(uintptr_t image,uintptr_t property,uintptr_t load,uintptr_t receiver,const char* text,JobCall*);
uint32_t work_unary(uintptr_t entry,uintptr_t receiver,JobCall*);
uint32_t work_bandwidth(uintptr_t image,uintptr_t building,float value,JobCall*);
uint32_t work_reserve(uintptr_t image,uintptr_t array,int32_t capacity,JobCall*);
uint32_t work_region(uintptr_t image,uintptr_t actor,uintptr_t region,JobCall*);
bool work_store(uintptr_t address,const void* before,const void* target,size_t bytes,uint32_t* exception);
}
