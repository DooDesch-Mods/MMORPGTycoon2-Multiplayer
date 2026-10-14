#include "action_process.h"
#include <algorithm>
#include <limits>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
namespace mt2mp::build_actions::private_native {
namespace {
uint32_t destroy_seh(DestroyProp fn,uintptr_t selection,uintptr_t target,bool& attempted,bool& returned){
    __try{attempted=true;fn(reinterpret_cast<void*>(selection),reinterpret_cast<void*>(target));returned=true;return 0;}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
uint32_t highlights_seh(SetHighlightedProps fn,uintptr_t selection,const ConstPropArray* values,bool& attempted,bool& returned){
    __try{attempted=true;fn(reinterpret_cast<void*>(selection),values);returned=true;return 0;}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
CallReceipt call_destroy(DestroyProp fn,uintptr_t selection,uintptr_t target){
    CallReceipt r;
    if(!fn||!selection||!target){r.error="destroy_arguments_invalid";return r;}
    try{r.seh_code=destroy_seh(fn,selection,target,r.attempted,r.returned);if(r.seh_code)r.error="destroy_SEH="+std::to_string(r.seh_code);}
    catch(const std::exception& ex){r.error=std::string("destroy_exception: ")+ex.what();}
    catch(...){r.error="destroy_unknown_exception";}
    return r;
}
CallReceipt call_highlights(SetHighlightedProps fn,uintptr_t selection,uintptr_t array_ap,const std::vector<uintptr_t>& survivors){
    CallReceipt r;
    if(!fn||!selection||!array_ap||survivors.size()>static_cast<size_t>(std::numeric_limits<int32_t>::max())||
       std::find(survivors.begin(),survivors.end(),uintptr_t{0})!=survivors.end()){r.error="highlight_arguments_invalid";return r;}
    const ConstPropArray array{array_ap,survivors.data(),static_cast<int32_t>(survivors.size()),static_cast<int32_t>(survivors.size())};
    try{r.seh_code=highlights_seh(fn,selection,&array,r.attempted,r.returned);if(r.seh_code)r.error="highlight_SEH="+std::to_string(r.seh_code);}
    catch(const std::exception& ex){r.error=std::string("highlight_exception: ")+ex.what();}
    catch(...){r.error="highlight_unknown_exception";}
    return r;
}
}
