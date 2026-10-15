#include "scalar.h"
#include "initial_release/release_entry.h"
#include <Windows.h>
#include <cstring>
namespace mt2mp::game::developer_staff::scalar {
uint32_t invoke(uintptr_t image,uintptr_t actor,Field field,int32_t value,Receipt* out){
    constexpr const char* records[]={"state Idle","state GoingToJob","state DoingJob","state TransportJob","state GoingHome","state TechGoingHome","state TechDevelop","state TechDebug"};
    const char* text=field==Field::State?records[value]:(value?"active true":"active false");
    const char* expected=field==Field::State?state_name(value):(value?"true":"false");
    initial_release::Record record{};uint32_t fault=0;
    using Load=bool(*)(const void*,void*,void*,void*);
    __try{
        reinterpret_cast<initial_release::RecordCtor>(image+0x967370)(&record,text);out->constructed=true;
        const auto base=reinterpret_cast<uintptr_t>(&record),token=*reinterpret_cast<const uintptr_t*>(base+0x38);
        // Record equality compares the label and every argument token.
        out->record_confirmed=reinterpret_cast<initial_release::RecordEquals>(image+0x1113d80)(&record,text)&&
            *reinterpret_cast<const int32_t*>(base+0x40)==1&&token&&*reinterpret_cast<const int32_t*>(token)==1&&
            *reinterpret_cast<const uint64_t*>(token+0x10)==std::strlen(expected)&&
            std::strcmp(*reinterpret_cast<const char* const*>(token+8),expected)==0;
        if(out->record_confirmed){out->attempted=true;
            const auto fn=image+(field==Field::State?0xd0fd10:0xd258b0),property=image+(field==Field::State?0x13c8fc0:0x13c8f40);
            out->accepted=reinterpret_cast<Load>(fn)(reinterpret_cast<const void*>(property),reinterpret_cast<void*>(actor),&record,nullptr);out->returned=true;}
    }__except(EXCEPTION_EXECUTE_HANDLER){fault=GetExceptionCode();}
    if(out->constructed){
        __try{reinterpret_cast<initial_release::RecordDtor>(image+0x9676a0)(&record);out->cleanup_confirmed=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){out->cleanup_exception=GetExceptionCode();}}
    return fault;
}
}
