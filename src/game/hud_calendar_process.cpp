#include "hud_calendar.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
namespace mt2mp::game::hud_time {
namespace {
DWORD call(OriginalPanelUpdate original,void* receiver,float dt){
    __try{original(receiver,dt);return 0;}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool process_call_calendar_panel(void*,OriginalPanelUpdate original,void* receiver,float dt,std::string& error){
    if(!original){error="calendar_original_panel_null";return false;}
    const DWORD code=call(original,receiver,dt);
    if(code){error="calendar_original_panel_seh="+std::to_string(code);return false;}return true;
}
}
