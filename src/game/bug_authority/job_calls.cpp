#include "job_calls.h"
#include "initial_release/release_entry.h"
#include <Windows.h>
#include <cstring>
namespace mt2mp::game::bug_authority {
uint32_t job_link_property(uintptr_t image,uintptr_t property,uintptr_t receiver,
                          bool bug_property,bool attach,void* context,JobCall* out){
    const char* text=bug_property?(attach?"job 1":"job 0"):(attach?"bug 2":"bug 0");
    initial_release::Record record{};bool constructed=false;
    __try{
        reinterpret_cast<initial_release::RecordCtor>(image+0x967370)(&record,text);constructed=true;
        out->record_confirmed=reinterpret_cast<initial_release::RecordEquals>(image+0x1113d80)(&record,text);
        if(out->record_confirmed){out->attempted=true;
            out->accepted=reinterpret_cast<bool(*)(const void*,void*,void*,void*)>(image+(bug_property?0xe02e90:0xe02760))(
                reinterpret_cast<void*>(property),reinterpret_cast<void*>(receiver),&record,context);
            if(out->accepted)reinterpret_cast<void(*)(const void*,void*,void*)>(image+(bug_property?0xe02fa0:0xe02920))(
                reinterpret_cast<void*>(property),reinterpret_cast<void*>(receiver),context);
            out->returned=true;
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){out->exception=GetExceptionCode();}
    if(constructed){__try{reinterpret_cast<initial_release::RecordDtor>(image+0x9676a0)(&record);out->cleanup_confirmed=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){out->cleanup_exception=GetExceptionCode();}}
    return out->exception;
}
uint32_t construct_job(uintptr_t image,JobBirthCall* out){
    __try{
        out->allocate.attempted=true;
        out->job=reinterpret_cast<uintptr_t(*)(size_t)>(image+0xa02670)(0xc8);
        out->allocate.returned=true;
        if(out->job){out->construct.attempted=true;
            reinterpret_cast<void(*)(uintptr_t)>(image+0x281020)(out->job);out->construct.returned=true;}
    }__except(EXCEPTION_EXECUTE_HANDLER){
        if(out->construct.attempted)out->construct.exception=GetExceptionCode();else out->allocate.exception=GetExceptionCode();}
    return out->allocate.exception?out->allocate.exception:out->construct.exception;
}
uint32_t configure_job(uintptr_t image,uintptr_t job,uintptr_t bug,JobCall* out){
    __try{out->attempted=true;reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x281610)(job,bug);out->returned=true;}
    __except(EXCEPTION_EXECUTE_HANDLER){out->exception=GetExceptionCode();}return out->exception;
}
uint32_t insert_job(uintptr_t image,uintptr_t queue,uintptr_t job,JobCall* out){
    __try{out->attempted=true;reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0xb5ab50)(queue+0x18,job);out->returned=true;}
    __except(EXCEPTION_EXECUTE_HANDLER){out->exception=GetExceptionCode();}return out->exception;
}
uint32_t work_property(uintptr_t image,uintptr_t property,uintptr_t load,uintptr_t receiver,const char* text,JobCall* out){
    initial_release::Record record{};bool constructed=false;
    __try{reinterpret_cast<initial_release::RecordCtor>(image+0x967370)(&record,text);constructed=true;
        out->record_confirmed=reinterpret_cast<initial_release::RecordEquals>(image+0x1113d80)(&record,text);
        if(out->record_confirmed){out->attempted=true;out->accepted=reinterpret_cast<bool(*)(uintptr_t,uintptr_t,void*,void*)>(image+load)(property,receiver,&record,nullptr);out->returned=true;}
    }__except(EXCEPTION_EXECUTE_HANDLER){out->exception=GetExceptionCode();}
    if(constructed){__try{reinterpret_cast<initial_release::RecordDtor>(image+0x9676a0)(&record);out->cleanup_confirmed=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){out->cleanup_exception=GetExceptionCode();}}
    return out->exception;
}
uint32_t work_unary(uintptr_t entry,uintptr_t receiver,JobCall* out){
    __try{out->attempted=true;reinterpret_cast<void(*)(uintptr_t)>(entry)(receiver);out->returned=true;}
    __except(EXCEPTION_EXECUTE_HANDLER){out->exception=GetExceptionCode();}return out->exception;
}
uint32_t work_bandwidth(uintptr_t image,uintptr_t building,float value,JobCall* out){
    __try{out->attempted=true;reinterpret_cast<void(*)(uintptr_t,float)>(image+0x426830)(building+0x7e8,value);out->returned=true;}
    __except(EXCEPTION_EXECUTE_HANDLER){out->exception=GetExceptionCode();}return out->exception;
}
uint32_t work_reserve(uintptr_t image,uintptr_t array,int32_t capacity,JobCall* out){
    __try{out->attempted=true;reinterpret_cast<void(*)(uintptr_t,int32_t)>(image+0xe8080)(array,capacity);out->returned=true;}
    __except(EXCEPTION_EXECUTE_HANDLER){out->exception=GetExceptionCode();}return out->exception;
}
uint32_t work_region(uintptr_t image,uintptr_t actor,uintptr_t region,JobCall* out){
    __try{out->attempted=true;reinterpret_cast<void(*)(uintptr_t,uintptr_t)>(image+0x4d0140)(actor,region);out->returned=true;}
    __except(EXCEPTION_EXECUTE_HANDLER){out->exception=GetExceptionCode();}return out->exception;
}
bool work_store(uintptr_t address,const void* before,const void* target,size_t bytes,uint32_t* exception){
    __try{if(std::memcmp(reinterpret_cast<void*>(address),before,bytes))return false;
        std::memcpy(reinterpret_cast<void*>(address),target,bytes);return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){*exception=GetExceptionCode();return false;}
}
}
