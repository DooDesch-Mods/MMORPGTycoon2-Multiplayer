#include "retire.h"
#include "initial_release/release_entry.h"
#include <Windows.h>
#include <cstring>
namespace mt2mp::game::developer_staff::retirement {
uint32_t invoke_original(uintptr_t image,uintptr_t developer,Route route,Call* out){
    initial_release::Record record{};uint32_t fault=0;
    using PropertyLoad=bool(*)(const void*,void*,void*,void*);
    __try{
        reinterpret_cast<initial_release::RecordCtor>(image+0x967370)(&record,"active false");out->constructed=true;
        const auto base=reinterpret_cast<uintptr_t>(&record);
        const auto token=*reinterpret_cast<const uintptr_t*>(base+0x38);
        const auto count=*reinterpret_cast<const int32_t*>(base+0x40);
        // Record equality compares the label and every argument token.
        out->record_confirmed=reinterpret_cast<initial_release::RecordEquals>(image+0x1113d80)(&record,"active false")&&count==1&&token&&
            *reinterpret_cast<const int32_t*>(token)==1&&*reinterpret_cast<const uint64_t*>(token+0x10)==5&&
            std::memcmp(*reinterpret_cast<const char* const*>(token+8),"false",6)==0;
        if(out->record_confirmed){
            out->attempted=true;
            if(route==Route::Arriving){reinterpret_cast<void(*)(void*)>(image+0x3a5330)(reinterpret_cast<void*>(developer));out->arrival_returned=true;}
            // Scalar bool Load ignores ObjectContext. It registers no object or link.
            out->property_accepted=reinterpret_cast<PropertyLoad>(image+0xd258b0)(reinterpret_cast<const void*>(image+0x13c8f40),reinterpret_cast<void*>(developer),&record,nullptr);
            out->property_returned=true;
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){fault=GetExceptionCode();}
    if(out->constructed){out->cleanup_attempted=true;
        __try{reinterpret_cast<initial_release::RecordDtor>(image+0x9676a0)(&record);out->cleanup_confirmed=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){out->cleanup_exception=GetExceptionCode();}}
    return fault;
}
}
