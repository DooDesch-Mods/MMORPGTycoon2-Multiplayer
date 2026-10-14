#include "hook_dispatch.h"
#include <exception>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

namespace mt2mp::remove_runtime {
namespace {
int fault(HookDispatch& d,const char* op,uint32_t code)noexcept{
    if(code!=0xe06d7363)d.listener().original_failed(op,"native original SEH; exception continues",code);
    return EXCEPTION_CONTINUE_SEARCH;
}
void invoke_void(HookDispatch& d,const char* op,void(*fn)(void*),void* p){
    try{fn(p);}catch(const std::exception& ex){d.listener().original_failed(op,ex.what(),0);throw;}
    catch(...){d.listener().original_failed(op,"native original non-standard C++ exception",0);throw;}
}
void* invoke_open(HookDispatch& d,native::UndoOpenFn fn,const void* p){
    try{return fn(p);}catch(const std::exception& ex){d.listener().original_failed("Undo.Open",ex.what(),0);throw;}
    catch(...){d.listener().original_failed("Undo.Open","native original non-standard C++ exception",0);throw;}
}
void absent(HookDispatch& d,const char* op)noexcept{
    d.listener().original_failed(op,"typed original was never published",0);
    RaiseFailFastException(nullptr,nullptr,0);
}
}
bool HookDispatch::publish_disabled(const native::RemoveEntrypoints& e)noexcept{
    if(published()||inflight()||!e.clear||!e.open||!e.lock||!e.prop_destructor)return false;
    originals_=e;published_.store(true,std::memory_order_release);return true;
}
void HookDispatch::clear(void* p,uintptr_t caller){
    enter();
    __try {
        if(!published()){absent(*this,"ClearSelection");return;}
        if(!listener_.forward_clear(uintptr_t(p),caller))return;
        __try {invoke_void(*this,"ClearSelection",originals_.clear,p);}
        __except(fault(*this,"ClearSelection",GetExceptionCode())){RaiseFailFastException(nullptr,nullptr,0);}
    }__finally {leave();}
}
void* HookDispatch::open(const void* p,uintptr_t caller){
    void* graph=nullptr;enter();
    __try {
        if(!published()){absent(*this,"Undo.Open");return nullptr;}
        __try {graph=invoke_open(*this,originals_.open,p);listener_.open_returned(caller,uintptr_t(graph));}
        __except(fault(*this,"Undo.Open",GetExceptionCode())){RaiseFailFastException(nullptr,nullptr,0);}
    }__finally {leave();}
    return graph;
}
void HookDispatch::lock(void* p,uintptr_t caller){
    bool observed=false,returned=false;enter();
    __try {
        if(!published()){absent(*this,"Graph.Lock");return;}
        listener_.lock_enter(caller,uintptr_t(p));observed=true;
        __try {invoke_void(*this,"Graph.Lock",originals_.lock,p);returned=true;}
        __except(fault(*this,"Graph.Lock",GetExceptionCode())){RaiseFailFastException(nullptr,nullptr,0);}
    }__finally {if(observed)listener_.lock_returned(uintptr_t(p),returned);leave();}
}
void HookDispatch::destructor(void* p){
    enter();
    __try {
        if(!published()){absent(*this,"Prop.Dtor");return;}
        listener_.destructor_enter(uintptr_t(p));
        __try {invoke_void(*this,"Prop.Dtor",originals_.prop_destructor,p);}
        __except(fault(*this,"Prop.Dtor",GetExceptionCode())){RaiseFailFastException(nullptr,nullptr,0);}
    }__finally {leave();}
}
}
