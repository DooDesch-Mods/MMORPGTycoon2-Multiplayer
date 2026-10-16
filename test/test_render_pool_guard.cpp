#include "render_pool_guard/invoke.h"
#include <atomic>
#include <array>
#include <iostream>
#include <type_traits>
namespace guard=mt2mp::game::render_pool_guard;
namespace {
unsigned checks=0;
void need(bool value,const char* cause){
    ++checks;
    if(!value){std::cerr<<"FAIL "<<cause<<'\n';ExitProcess(1);}
}
void signaled(HANDLE event,const char* cause){need(WaitForSingleObject(event,10000)==WAIT_OBJECT_0,cause);}
HANDLE event(){HANDLE value=CreateEventW(nullptr,TRUE,FALSE,nullptr);need(value!=nullptr,"CreateEventW failed");return value;}
HANDLE start(LPTHREAD_START_ROUTINE entry,void* context){HANDLE value=CreateThread(nullptr,0,entry,context,0,nullptr);need(value!=nullptr,"CreateThread failed");return value;}
void close(HANDLE value){need(CloseHandle(value)!=FALSE,"CloseHandle failed");}
void signal(HANDLE value){if(!SetEvent(value)){std::cerr<<"FAIL worker SetEvent failed\n";ExitProcess(4);}}
struct Calls {int unary=0,binary=0;void* self=nullptr;void* argument=nullptr;};
Calls calls;
void unary(void* self){++calls.unary;calls.self=self;}
void binary(void* self,void* argument){++calls.binary;calls.self=self;calls.argument=argument;}
void* nested_argument=nullptr;
void unary_reentrant(void* self){++calls.unary;guard::invoke(binary,self,nested_argument);}
void binary_reentrant(void* self,void* argument){++calls.binary;calls.argument=argument;guard::invoke(unary,self);}

struct Concurrent {
    HANDLE entered=nullptr,release=nullptr,probed=nullptr;
    std::atomic<int> inside=0,overlap=0,holder_calls=0,contender_calls=0;
    bool lock_was_held=false;
    void* actual_argument=nullptr;
};
void hold(void* self){
    auto& s=*static_cast<Concurrent*>(self);++s.holder_calls;
    if(s.inside.fetch_add(1)!=0)++s.overlap;
    signal(s.entered);
    if(WaitForSingleObject(s.release,10000)!=WAIT_OBJECT_0){std::cerr<<"FAIL holder release wait\n";ExitProcess(2);}
    --s.inside;
}
void contend(void* self,void* argument){
    auto& s=*static_cast<Concurrent*>(self);++s.contender_calls;s.actual_argument=argument;
    if(s.inside.fetch_add(1)!=0)++s.overlap;
    --s.inside;
}
DWORD WINAPI holder(void* self){guard::invoke(hold,self);return 0;}
DWORD WINAPI contender(void* self){
    auto& s=*static_cast<Concurrent*>(self);
    const bool acquired=TryEnterCriticalSection(&guard::pool_lock)!=FALSE;
    s.lock_was_held=!acquired;if(acquired)LeaveCriticalSection(&guard::pool_lock);
    signal(s.probed);guard::invoke(contend,self,&s.release);return 0;
}
struct Stress {std::atomic<int> inside=0,overlap=0,calls=0;};
void stress_unary(void* self){auto& s=*static_cast<Stress*>(self);if(s.inside.fetch_add(1)!=0)++s.overlap;++s.calls;SwitchToThread();--s.inside;}
void stress_binary(void* self,void* argument){if(self!=argument){std::cerr<<"FAIL stress Binary argument\n";ExitProcess(3);}stress_unary(self);}
DWORD WINAPI stress_a(void* self){for(int i=0;i<2000;++i)guard::invoke(stress_unary,self);return 0;}
DWORD WINAPI stress_b(void* self){for(int i=0;i<2000;++i)guard::invoke(stress_binary,self,self);return 0;}

constexpr DWORD test_seh_code=0xe0421731;
struct ExceptionObservation {DWORD code=0,flags=0,count=0;ULONG_PTR values[2]{};};
int exception_calls=0;
void raise(void* self,void* argument){
    ++exception_calls;const ULONG_PTR values[]={reinterpret_cast<ULONG_PTR>(self),reinterpret_cast<ULONG_PTR>(argument)};
    RaiseException(test_seh_code,EXCEPTION_NONCONTINUABLE,2,values);
}
void raise_unary(void* self){raise(self,nested_argument);}
LONG observe(EXCEPTION_POINTERS* pointers,ExceptionObservation* out){
    const auto* record=pointers->ExceptionRecord;out->code=record->ExceptionCode;out->flags=record->ExceptionFlags;out->count=record->NumberParameters;
    if(record->NumberParameters==2){out->values[0]=record->ExceptionInformation[0];out->values[1]=record->ExceptionInformation[1];}
    return EXCEPTION_EXECUTE_HANDLER;
}
void catch_binary(bool wrapped,void* self,void* argument,ExceptionObservation* out){
    __try {if(wrapped)guard::invoke(raise,self,argument);else raise(self,argument);}
    __except(observe(GetExceptionInformation(),out)) {return;}
}
void catch_unary(void* self,ExceptionObservation* out){
    __try {guard::invoke(raise_unary,self);}
    __except(observe(GetExceptionInformation(),out)) {return;}
}
struct Released {bool acquired=false;int calls=0;};
void after_exception(void* self){++static_cast<Released*>(self)->calls;}
DWORD WINAPI test_released(void* self){
    auto& s=*static_cast<Released*>(self);s.acquired=TryEnterCriticalSection(&guard::pool_lock)!=FALSE;
    if(s.acquired){LeaveCriticalSection(&guard::pool_lock);guard::invoke(after_exception,self);}return 0;
}
void require_released(){Released s;HANDLE thread=start(test_released,&s);signaled(thread,"post-SEH thread stalled");close(thread);need(s.acquired&&s.calls==1,"SEH left the lock owned or blocked the next original");}
bool same(const ExceptionObservation& a,const ExceptionObservation& b){return a.code==b.code&&a.flags==b.flags&&a.count==b.count&&a.values[0]==b.values[0]&&a.values[1]==b.values[1];}
}
int main(){
    static_assert(std::is_same_v<guard::Unary,void(*)(void*)>);
    static_assert(std::is_same_v<guard::Binary,void(*)(void*,void*)>);
    InitializeCriticalSection(&guard::pool_lock);
    int receiver=0,argument=0;
    for(void* self:std::array<void*,2>{&receiver,nullptr}){
        calls={};guard::invoke(unary,self);need(calls.unary==1&&calls.binary==0&&calls.self==self,"Unary changed self or original call count");
        for(void* arg:std::array<void*,2>{&argument,nullptr}){
            calls={};guard::invoke(binary,self,arg);need(calls.unary==0&&calls.binary==1&&calls.self==self&&calls.argument==arg,"Binary changed arguments or original call count");
        }
    }
    calls={};nested_argument=&argument;guard::invoke(unary_reentrant,&receiver);
    need(calls.unary==1&&calls.binary==1&&calls.self==&receiver&&calls.argument==&argument,"Unary-to-Binary reentrancy failed");
    calls={};guard::invoke(binary_reentrant,&receiver,&argument);
    need(calls.unary==1&&calls.binary==1&&calls.self==&receiver&&calls.argument==&argument,"Binary-to-Unary reentrancy failed");
    Concurrent concurrent;concurrent.entered=event();concurrent.release=event();concurrent.probed=event();
    HANDLE first=start(holder,&concurrent);signaled(concurrent.entered,"holder did not enter original");
    HANDLE second=start(contender,&concurrent);signaled(concurrent.probed,"contender did not probe lock");
    need(concurrent.lock_was_held,"actual Unary wrapper did not own the shared lock");
    need(concurrent.contender_calls.load()==0,"Binary original entered while Unary original held lock");
    need(SetEvent(concurrent.release)!=FALSE,"holder release signal failed");signaled(first,"holder stalled");signaled(second,"contender stalled after release");
    need(concurrent.overlap==0&&concurrent.holder_calls==1&&concurrent.contender_calls==1&&concurrent.actual_argument==&concurrent.release,"cross-signature exclusion or exact calls failed");
    close(first);close(second);close(concurrent.entered);close(concurrent.release);close(concurrent.probed);
    Stress stress;first=start(stress_a,&stress);second=start(stress_b,&stress);signaled(first,"Unary stress stalled");signaled(second,"Binary stress stalled");
    need(stress.calls==4000&&stress.inside==0&&stress.overlap==0,"two-thread originals overlapped or count changed");close(first);close(second);
    ExceptionObservation direct,wrapped;
    exception_calls=0;catch_binary(false,&receiver,&argument,&direct);
    if(!(exception_calls==1&&direct.code==test_seh_code&&(direct.flags&EXCEPTION_NONCONTINUABLE)!=0&&direct.count==2))
        std::cerr<<"Direct SEH actual calls="<<exception_calls<<" code="<<direct.code<<" flags="<<direct.flags<<" count="<<direct.count<<'\n';
    need(exception_calls==1&&direct.code==test_seh_code&&(direct.flags&EXCEPTION_NONCONTINUABLE)!=0&&direct.count==2,"direct SEH baseline failed");
    exception_calls=0;catch_binary(true,&receiver,&argument,&wrapped);
    need(exception_calls==1&&same(direct,wrapped),"Binary changed/suppressed SEH code, flags, payload or original count");require_released();
    exception_calls=0;wrapped={};catch_unary(&receiver,&wrapped);
    need(exception_calls==1&&same(direct,wrapped),"Unary changed/suppressed SEH code, flags, payload or original count");require_released();
    DeleteCriticalSection(&guard::pool_lock);
    std::cout<<"PASS "<<checks<<" actual RenderBuffer invoke wrapper checks\n";return 0;
}
