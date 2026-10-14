#include "release_entry.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
namespace mt2mp::game::initial_release {
namespace {
uint32_t original(const Api* api,uintptr_t window,CallReceipt* receipt){
    Record record{};uint32_t fault=0;
    __try {
        api->construct(&record,"DoCelebration");receipt->constructed=true;
        receipt->label_confirmed=api->equals(&record,"DoCelebration");
        if(receipt->label_confirmed){receipt->attempted=true;receipt->accepted=api->command(reinterpret_cast<void*>(window),&record);receipt->returned=true;}
    }__except(EXCEPTION_EXECUTE_HANDLER){fault=GetExceptionCode();}
    if(receipt->constructed){receipt->cleanup_attempted=true;
        __try {api->destroy(&record);receipt->cleanup_confirmed=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){receipt->cleanup_exception=GetExceptionCode();}}
    return fault;
}
class ProcessCalls final:public Calls {
    bool command(const Api& api,uintptr_t window,CallReceipt& receipt,std::string& cause)override{
        receipt.seh_code=original(&api,window,&receipt);
        if(receipt.seh_code){cause="release original UICommand SEH="+std::to_string(receipt.seh_code);return false;}
        if(receipt.cleanup_exception){cause="release vsRecord cleanup SEH="+std::to_string(receipt.cleanup_exception);return false;}
        if(!receipt.label_confirmed){cause="release genuine vsRecord label differs";return false;}
        if(!receipt.returned||!receipt.accepted){cause="release original UICommand did not return accepted";return false;}
        if(!receipt.cleanup_confirmed){cause="release genuine vsRecord cleanup not confirmed";return false;}
        cause.clear();return true;
    }
};
}
Calls& process_calls(){static ProcessCalls calls;return calls;}
CommandClass classify_record(const Api& api,const void* record,uint32_t& seh_code)noexcept{
    seh_code=0;if(!api.equals||!record)return CommandClass::Unreadable;
    __try {return api.equals(record,"DoCelebration")?CommandClass::DoCelebration:CommandClass::Other;}
    __except(EXCEPTION_EXECUTE_HANDLER){seh_code=GetExceptionCode();return CommandClass::Unreadable;}
}
}
