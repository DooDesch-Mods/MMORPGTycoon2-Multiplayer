#include "mt2mp/hooks/original_body.h"
#include "ui_probe.h"
#include "../control_input/control_runtime.h"
#include "mt2mp/core/hash.h"
#include "codeguards.h"
#include "scenery_probe_guards.h"
#include "scene_mode_guards.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "live_host_state/memory_query_timing.h"
#include <cmath>
#include <cstring>
#include <exception>
namespace placement_ui_probe {
namespace {
bool fail(std::string& e,const char* text){e=text;return false;}
bool context_actual(const Context& c,std::string& e){return c.image==reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr))&&c.world&&c.epoch&&c.generation&&c.thread&&GetCurrentThreadId()==c.thread||fail(e,"error: placement UI requires actual process/mainthread/world binding");}
bool scalar_request(const Request& r,std::string& e){if(r.token.empty()||r.token.size()>128||r.token.find('\0')!=std::string::npos||r.type.empty()||r.type.size()>256||r.type.find('\0')!=std::string::npos||r.variant.empty()||r.variant.size()>256||r.variant.find('\0')!=std::string::npos)return fail(e,"error: placement UI job/type/variant invalid");for(float v:r.position)if(!std::isfinite(v))return fail(e,"error: placement UI world position is not finite");return true;}
std::uint32_t copy_seh(std::uintptr_t p,void* out,std::size_t n)noexcept{__try{std::memcpy(out,reinterpret_cast<const void*>(p),n);return 0;}__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}}
bool find_mode_pane_seh(const Binding& b,std::uintptr_t window,std::uintptr_t& button,Receipt& receipt)noexcept{
    // FindPane takes a const GNU string reference and only reads its data and length.
    struct NativeString{const char* data;std::uint64_t size;char local[16];};
    const NativeString name{"mode_single",11,{}};
    static_assert(sizeof(NativeString)==32);
    __try{
        button=reinterpret_cast<std::uintptr_t(__fastcall*)(void*,const NativeString*)>(b.context.image+0x81d860)(reinterpret_cast<void*>(window),&name);
        return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){receipt.exception=GetExceptionCode();return false;}
}
bool call_seh(unsigned step,const Binding& b,const Request& r,std::uintptr_t list,std::int32_t index,Receipt& receipt)noexcept{
    __try{
        receipt.attempted_mask|=std::uint32_t{1}<<step;
        switch(step){
        case 0:reinterpret_cast<void(__fastcall*)(void*)>(b.context.image+0x580870)(reinterpret_cast<void*>(b.button));break;
        case 1:reinterpret_cast<void(__fastcall*)(void*,int)>(b.context.image+0x7420f0)(reinterpret_cast<void*>(list),index);break;
        case 2:reinterpret_cast<void(__fastcall*)(void*,void*,const float*)>(b.context.image+(b.family==Family::Scenery?0x5d5960:0x593890))(reinterpret_cast<void*>(b.behaviour),reinterpret_cast<void*>(b.cursor),r.position.data());break;
        case 3:reinterpret_cast<void(__fastcall*)(void*,void*,const float*)>(b.context.image+(b.family==Family::Scenery?0x5cf440:0x593810))(reinterpret_cast<void*>(b.behaviour),reinterpret_cast<void*>(b.cursor),r.position.data());break;
        case 4:reinterpret_cast<void(__fastcall*)(void*,void*,const float*)>(b.context.image+(b.family==Family::Scenery?0x5d0600:0x5912e0))(reinterpret_cast<void*>(b.behaviour),reinterpret_cast<void*>(b.cursor),r.position.data());break;
        case 5:reinterpret_cast<void(__fastcall*)(void*)>(b.context.image+0x73ffe0)(reinterpret_cast<void*>(list));break;
        case 7:if(!reinterpret_cast<bool(__fastcall*)(void*,const float*)>(b.context.image+0x5863e0)(reinterpret_cast<void*>(b.cursor),r.position.data()))return fail(receipt.cause,"error: original cursor world projection failed");break;
        case 6:reinterpret_cast<void(__fastcall*)(void*,float)>(b.context.image+0x5d4620)(reinterpret_cast<void*>(b.behaviour),0.0f);break;
        default:return false;
        }
        receipt.returned_mask|=std::uint32_t{1}<<step;return true;
    }__except(EXCEPTION_EXECUTE_HANDLER){receipt.exception=GetExceptionCode();return false;}
}
}
bool ProcessMemory::copy(std::uintptr_t p,void* out,std::size_t n,std::string& e){if(!p||!n||p>UINTPTR_MAX-(n-1))return fail(e,"error: native placement UI memory range invalid");MEMORY_BASIC_INFORMATION page{};if(!mt2mp::game::live_host_state::apply_timing::virtual_query(reinterpret_cast<const void*>(p),&page,sizeof(page),mt2mp::game::live_host_state::apply_timing::Part::QueryPlacement)||page.State!=MEM_COMMIT||(page.Protect&(PAGE_GUARD|PAGE_NOACCESS))||n>page.RegionSize||p-std::uintptr_t(page.BaseAddress)>page.RegionSize-n)return fail(e,"error: native placement UI page unreadable");auto code=copy_seh(p,out,n);if(code){e="error: native placement UI read SEH="+std::to_string(code);return false;}return true;}
bool Probe::initialize(const Context& c,std::string& e){if(context_.image)return fail(e,"error: placement UI probe already initialized");if(!context_actual(c,e))return false;ProcessMemory memory;for(const auto& g:guards){
    if(g.rva==0x207820){bool owned=false;
        const auto sha=mt2mp::sha256(std::vector<unsigned char>(g.bytes,g.bytes+g.size));
        if(!mt2mp::control_input::runtime::verify_item_activate(c.image,g.size,sha,owned,e))return false;
        if(owned)continue;
    }
    std::vector<unsigned char> actual(g.size);if(!memory.copy(c.image+g.rva,actual.data(),actual.size(),e))return false;if(g.rva==0x5854e0){if(!mt2mp::hooks::framework::verify_original_bytes(c.image+g.rva,actual.data(),g.bytes,g.size,e)){e="error: placement UI original body differs at RVA="+std::to_string(g.rva)+" original="+e;return false;}}else if(std::memcmp(actual.data(),g.bytes,g.size)){e="error: placement UI original body differs at RVA="+std::to_string(g.rva);return false;}}for(const auto& g:scenery_guards){std::vector<unsigned char> actual(g.size);if(!memory.copy(c.image+g.rva,actual.data(),actual.size(),e))return false;if(std::memcmp(actual.data(),g.bytes,g.size)){e="error: Scenery UI original body differs at RVA="+std::to_string(g.rva);return false;}}for(const auto& g:scene_mode_guards){std::vector<unsigned char> actual(g.size);if(!memory.copy(c.image+g.rva,actual.data(),actual.size(),e))return false;if(std::memcmp(actual.data(),g.bytes,g.size)){e="error: Scenery mode selector original body differs";return false;}}context_=c;return true;}
Receipt Probe::arm(const Request& r,RootServices& services){Receipt receipt;receipt.token=r.token;Binding binding;ProcessMemory memory;
    try{
        Context c;if(!context_.image||!scalar_request(r,receipt.cause)||!services.actual_context(c,receipt.cause)||!context_actual(c,receipt.cause)||c!=context_){if(receipt.cause.empty())receipt.cause="error: placement UI probe context changed";services.record_receipt(r,receipt);return receipt;}
        if(!read_existing_tool(memory,c,r,binding,receipt.cause)||!services.reserve_job_once(r,binding,receipt.cause)){if(receipt.cause.empty())receipt.cause="error: native placement UI job reservation failed";services.record_receipt(r,receipt);return receipt;}
        receipt.phase=Phase::Attempted;
        auto step=[&](unsigned s,std::uintptr_t list,std::int32_t index){Context current;if(!services.actual_context(current,receipt.cause)||current!=c||!context_actual(current,receipt.cause)){if(receipt.cause.empty())receipt.cause="error: placement UI world changed between native callbacks";return false;}if(!services.actual_input_routes(binding,receipt.cause))return false;return call_seh(s,binding,r,list,index,receipt);};
        bool ok=step(0,0,0);
        if(ok&&binding.family==Family::Scenery){
            std::uintptr_t window=0,button=0;ok=read_scenery_mode_window(memory,binding,window,receipt.cause);
            if(ok){ok=find_mode_pane_seh(binding,window,button,receipt);if(!ok)receipt.cause="error: original Scenery mode lookup SEH="+std::to_string(receipt.exception);}
            if(ok)ok=read_scenery_mode_button(memory,binding,window,button,receipt.cause);
            if(ok)ok=step(5,button,0);
            if(ok)ok=step(6,0,0);
        }
        std::int32_t index=-1;std::uintptr_t list=0;
        if(ok)ok=read_palette_index(memory,binding,r,index,list,receipt.cause);
        if(ok&&list)ok=step(1,list,index);
        if(ok&&binding.family==Family::Building){
            // Cursor.Update processes hover before Build.Update. Release belongs after
            // that hover so the native preview does not replace a pending placement.
            ok=step(7,0,0);
            receipt.phase=ok?Phase::ArmedAwaitingGameUpdate:Phase::Unknown;
            if(!ok&&receipt.cause.empty())receipt.cause="error: original cursor warp failed; job cannot replay";
            services.record_receipt(r,receipt);return receipt;
        }
        if(ok)ok=step(2,0,0);
        if(ok)ok=read_live_pose_and_pending(memory,binding,r,false,receipt.cause);
        if(ok)ok=step(3,0,0);
        if(ok&&binding.family==Family::Scenery)services.record_receipt(r,receipt);
        if(ok)ok=step(4,0,0);
        if(ok&&binding.family==Family::Scenery){
            Receipt admission;ok=services.read_actual_admission(r,admission,receipt.cause);
            if(ok&&admission.phase==Phase::Unknown){receipt.cause=admission.cause;ok=false;}
            else if(ok&&admission.capture_count){
                ok=merge_release_admission(r,admission,receipt,receipt.cause);
                if(ok){services.record_receipt(r,receipt);return receipt;}
            }
        }
        if(ok)ok=read_live_pose_and_pending(memory,binding,r,true,receipt.cause);
        if(ok&&binding.family==Family::Scenery){ok=read_scenery_release_pose(memory,binding,r,receipt.native_pose,receipt.cause);receipt.native_pose_measured=ok;}
        if(ok)receipt.phase=Phase::ArmedAwaitingGameUpdate;
        else{receipt.phase=Phase::Unknown;if(receipt.cause.empty())receipt.cause="error: native UI callbacks failed, exception="+std::to_string(receipt.exception);receipt.cause+="; job cannot replay";}
    }catch(const std::exception& e){receipt.phase=receipt.attempted_mask?Phase::Unknown:Phase::Rejected;receipt.cause=std::string("error: placement UI probe exception: ")+e.what();}
    catch(...){receipt.phase=receipt.attempted_mask?Phase::Unknown:Phase::Rejected;receipt.cause="error: placement UI probe unknown exception";}
    services.record_receipt(r,receipt);return receipt;
}
Receipt complete_build_input_at_update(const Request& r,const Binding& b,Receipt receipt,RootServices& services){
    constexpr std::uint32_t input_mask=(1u<<2)|(1u<<3)|(1u<<4);
    if(b.family!=Family::Building||receipt.token!=r.token||receipt.phase!=Phase::ArmedAwaitingGameUpdate||
       receipt.capture_count||receipt.host_confirmed_count||receipt.exception||
       !(receipt.returned_mask&(1u<<7))||(receipt.attempted_mask&input_mask)){
        receipt.phase=Phase::Unknown;receipt.cause="error: building input boundary is not an unconsumed armed job";
        services.record_receipt(r,receipt);return receipt;
    }
    // Publish consumption before any callback. A callback failure cannot replay input.
    receipt.phase=Phase::Attempted;services.record_receipt(r,receipt);
    try{
        ProcessMemory memory;
        auto step=[&](unsigned s){
            Context current;
            if(!services.actual_context(current,receipt.cause)||current!=b.context||!context_actual(current,receipt.cause)){
                if(receipt.cause.empty())receipt.cause="error: placement UI world changed at normal Build.Update";
                return false;
            }
            if(!services.actual_input_routes(b,receipt.cause))return false;
            return call_seh(s,b,r,0,0,receipt);
        };
        bool ok=step(2);
        if(ok)ok=read_live_pose_and_pending(memory,b,r,false,receipt.cause);
        if(ok)ok=step(3);
        if(ok)ok=step(4);
        if(ok)ok=read_live_pose_and_pending(memory,b,r,true,receipt.cause);
        receipt.phase=ok?Phase::ArmedAwaitingGameUpdate:Phase::Unknown;
        if(!ok&&receipt.cause.empty())receipt.cause="error: original building input failed at normal Update; no replay";
    }catch(const std::exception& cause){receipt.phase=Phase::Unknown;receipt.cause=std::string("error: building input boundary exception: ")+cause.what();}
    catch(...){receipt.phase=Phase::Unknown;receipt.cause="error: building input boundary nonstandard exception";}
    services.record_receipt(r,receipt);return receipt;
}
bool merge_release_admission(const Request& r,const Receipt& admission,Receipt& out,std::string& e){
    if(admission.token!=r.token||admission.phase!=Phase::CapturedAwaitingHost||admission.capture_count!=1||admission.host_confirmed_count||admission.action_uuid.empty()||!admission.native_pose_measured||!admission.cause.empty())return fail(e,"error: original Release admission is not an exact captured request");
    const auto attempted=out.attempted_mask,returned=out.returned_mask,exception=out.exception;
    if(attempted!=127||returned!=127||exception)return fail(e,"error: original Release callback attempts/returns incomplete");
    out=admission;out.attempted_mask=attempted;out.returned_mask=returned;out.exception=exception;return true;
}
bool classify_actual_admission(const Request& r,Receipt& actual,std::string& e){if(actual.token!=r.token||actual.capture_count>1||actual.host_confirmed_count>1||actual.host_confirmed_count>actual.capture_count||(actual.capture_count&&actual.action_uuid.empty())){actual.phase=Phase::Unknown;return fail(e,"error: actual placement capture/Host receipt binding differs");}if(actual.phase==Phase::Unknown){if(e.empty())e="error: actual placement admission remains Unknown";return false;}if(actual.host_confirmed_count==1)actual.phase=Phase::HostConfirmed;else if(actual.capture_count==1)actual.phase=Phase::CapturedAwaitingHost;else if(actual.phase==Phase::CapturedAwaitingHost||actual.phase==Phase::HostConfirmed){actual.phase=Phase::Unknown;return fail(e,"error: placement admission phase has no actual capture receipt");}return true;}
Receipt Probe::observe(const Request& r,RootServices& services){Receipt actual;actual.token=r.token;try{Context c;if(!services.actual_context(c,actual.cause)||c!=context_||!context_actual(c,actual.cause)||!services.read_actual_admission(r,actual,actual.cause)){if(actual.cause.empty())actual.cause="error: actual placement admission receipt unavailable";actual.phase=Phase::Unknown;return actual;}if(!classify_actual_admission(r,actual,actual.cause)&&actual.cause.empty())actual.cause="error: actual admission classification failed";return actual;}catch(const std::exception& e){actual.phase=Phase::Unknown;actual.cause=std::string("error: placement admission read exception: ")+e.what();return actual;}catch(...){actual.phase=Phase::Unknown;actual.cause="error: placement admission read unknown exception";return actual;}}
}
