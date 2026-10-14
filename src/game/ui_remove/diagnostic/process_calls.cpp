#include "ui_diagnostic.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace mt2mp::action_session::ui_diagnostic {
namespace {
uint32_t select_native(const Api* api,const Context* context,Receipt* receipt) {
    __try {
        receipt->selection_attempted=true;api->select(reinterpret_cast<void*>(context->manager),reinterpret_cast<void*>(context->target));
        api->info(reinterpret_cast<void*>(context->manager));receipt->selection_returned=true;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
    return 0;
}
uint32_t command_native(const Api* api,const Window* window,Receipt* receipt) {
    NativeRecordStorage record{};uint32_t fault=0;
    __try {
        api->construct(&record,api->destroy_label);receipt->record_constructed=true;
        receipt->record_label_confirmed=api->label_equals(&record,api->destroy_label);
        if(receipt->record_label_confirmed) {
            receipt->ui_command_attempted=true;
            receipt->ui_command_accepted=api->command(reinterpret_cast<void*>(window->window),&record);
            receipt->ui_command_returned=true;
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){fault=GetExceptionCode();}
    if(receipt->record_constructed) {
        receipt->record_cleanup_attempted=true;
        __try {api->destroy(&record);receipt->record_cleanup_confirmed=true;}
        __except(EXCEPTION_EXECUTE_HANDLER){receipt->cleanup_exception=GetExceptionCode();}
    }
    return fault;
}
class ProcessCalls final:public Calls {
    bool select(const Api& api,const Context& context,Receipt& receipt,std::string& cause)override {
        receipt.native_exception=select_native(&api,&context,&receipt);
        if(receipt.native_exception){cause="native selection/show SEH="+std::to_string(receipt.native_exception);return false;}
        if(!receipt.selection_returned){cause="native selection/show did not return";return false;}
        cause.clear();return true;
    }
    bool command(const Api& api,const Window& window,Receipt& receipt,std::string& cause)override {
        receipt.native_exception=command_native(&api,&window,&receipt);
        if(receipt.native_exception){cause="native typed InfoWindow UICommand SEH="+std::to_string(receipt.native_exception);return false;}
        if(receipt.cleanup_exception){cause="native vsRecord cleanup SEH="+std::to_string(receipt.cleanup_exception);return false;}
        if(!receipt.record_label_confirmed){cause="native vsRecord label differs from typed Destroy command";return false;}
        if(!receipt.ui_command_returned||!receipt.ui_command_accepted){cause="native typed InfoWindow did not accept Destroy command";return false;}
        if(!receipt.record_cleanup_confirmed){cause="native vsRecord cleanup not confirmed";return false;}
        cause.clear();return true;
    }
};
}
Calls& process_calls(){static ProcessCalls calls;return calls;}
}
