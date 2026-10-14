#include "native_button.h"
#include "button_original.h"
#include <algorithm>
#include <cstring>
#include <exception>
#include <limits>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif
namespace mt2mp::game::time_native_button {
namespace {
bool reject(std::string& error,const char* cause){error=cause;return false;}
bool uuid(const std::string& text) {
    if(text.size()!=36)return false;
    for(size_t i=0;i<text.size();++i) {
        if(i==8||i==13||i==18||i==23){if(text[i]!='-')return false;}
        else if(!((text[i]>='0'&&text[i]<='9')||(text[i]>='a'&&text[i]<='f')))return false;
    }
    return true;
}
bool scale(float value){return value==0||value==1||value==3||value==9;}
bool same_request(const Request& a,const Request& b) {
    return a.token==b.token&&a.snapshot_uuid==b.snapshot_uuid&&a.epoch==b.epoch&&
        a.generation==b.generation&&a.revision==b.revision&&a.scale==b.scale;
}
#ifdef _WIN32
using ButtonActivate=void(__fastcall*)(void*);
DWORD copy_seh(uintptr_t address,void* out,size_t bytes) {
    __try {std::memcpy(out,reinterpret_cast<const void*>(address),bytes);return 0;}
    __except(GetExceptionCode()==0xe06d7363UL?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
DWORD call_seh(ButtonActivate original,void* receiver,bool* attempted) {
    __try {*attempted=true;original(receiver);return 0;}
    __except(GetExceptionCode()==0xe06d7363UL?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool read(uintptr_t address,void* out,size_t bytes,std::string& error) {
    if(!address||!bytes||address>UINTPTR_MAX-(bytes-1))return reject(error,"native_button_read_span_invalid");
    MEMORY_BASIC_INFORMATION page{};
    if(!VirtualQuery(reinterpret_cast<void*>(address),&page,sizeof(page))||page.State!=MEM_COMMIT||
       (page.Protect&(PAGE_NOACCESS|PAGE_GUARD))||address<uintptr_t(page.BaseAddress)||
       bytes>page.RegionSize||address-uintptr_t(page.BaseAddress)>page.RegionSize-bytes)
        return reject(error,"native_button_read_page_unavailable");
    const auto seh=copy_seh(address,out,bytes);
    if(seh){error="native_button_read_seh="+std::to_string(seh);return false;}return true;
}
bool actual_entry(const Binding& b,std::string& error) {
    if(GetCurrentThreadId()!=b.game_thread)return reject(error,"native_button_actual_OS_mainthread_differs");
    if(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))!=b.image)
        return reject(error,"native_button_actual_process_image_differs");
    std::array<uint8_t,button_activate_original.size()> code{};
    if(!read(b.image+button_activate_rva,code.data(),code.size(),error))return false;
    if(code!=button_activate_original)return reject(error,"native_button_complete_Original_bytes_differ");
    uintptr_t addresspoint=0,item=0,proxy=0,target=0,type=0,type_proxy=0,type_target=0,own_item_proxy=0,own_type_proxy=0;
    if(!read(b.button,&addresspoint,sizeof(addresspoint),error))return false;
    if(addresspoint!=b.image+button_addresspoint_rva)
        return reject(error,"native_button_actual_class_differs");
    if(!read(b.button+0x2d0,&item,sizeof(item),error)||!read(b.button+0x2d8,&proxy,sizeof(proxy),error)||
       !read(b.item_proxy,&target,sizeof(target),error)||!read(b.item+0x68,&type,sizeof(type),error)||
       !read(b.item+0x70,&type_proxy,sizeof(type_proxy),error)||!read(b.type_proxy,&type_target,sizeof(type_target),error)||
       !read(b.item+0x28,&own_item_proxy,sizeof(own_item_proxy),error)||!read(b.type+0x40,&own_type_proxy,sizeof(own_type_proxy),error))return false;
    if(item!=b.item||proxy!=b.item_proxy||target!=b.item+0x20||type!=b.type||
       type_proxy!=b.type_proxy||type_target!=b.type+0x38||own_item_proxy!=b.item_proxy||own_type_proxy!=b.type_proxy)
        return reject(error,"native_button_actual_receiver_Weak_identity_differs");
    return true;
}
#endif
}
bool validate_binding(const Request& request,const Binding& b,std::string& error) {
    if(!uuid(request.token)||!uuid(request.snapshot_uuid)||!request.epoch||!request.generation||!request.revision||!scale(request.scale))
        return reject(error,"native_button_request_identity_invalid");
    if(!b.image||b.image>UINTPTR_MAX-0x2000000||!b.world||!b.panel||!b.slot||!b.button||!b.item||!b.type||
       !b.item_proxy||!b.type_proxy||!b.game_thread||b.item>UINTPTR_MAX-0xc8||b.type>UINTPTR_MAX-0x160||
       b.button>UINTPTR_MAX-0x408)
        return reject(error,"native_button_private_binding_span_invalid");
    if(b.epoch!=request.epoch||b.generation!=request.generation||b.revision!=request.revision||b.scale!=request.scale)
        return reject(error,"native_button_request_world_revision_scale_differs");
    if(b.item_proxy_target!=b.item+0x20||b.type_proxy_target!=b.type+0x38)
        return reject(error,"native_button_WeakTarget_subobject_differs");
    if(b.role!=Role::Host&&b.role!=Role::Client)return reject(error,"native_button_role_invalid");
    if(!b.button_enabled||b.native_item_state!=1)
        return reject(error,"native_button_requires_enabled_inactive_item_for_Activate_route");
    error.clear();return true;
}
bool invoke(const Request& request,RootServices& services,Receipt& receipt,std::string& error) {
    receipt={};
    auto persist=[&] {
        receipt.cause=error;
        std::string cause;
        if(!services.record_actual_attempt(receipt,cause)) {
            if(cause.empty())cause="Root job receipt persistence failed without cause";
            error+="; native_button_actual_receipt_failed: "+cause;
            if(receipt.native_attempted)receipt.phase=Phase::Unknown;
            receipt.cause=error;return false;
        }
        return true;
    };
    try {
        if(!services.reserve_actual_job(request,receipt.actual_attempt,error)) {
            if(error.empty())error="native_button_Root_job_reservation_failed_without_cause";return false;
        }
        if(!receipt.actual_attempt.sequence||!same_request(request,receipt.actual_attempt.request)) {
            error="native_button_actual_Root_job_identity_differs";persist();return false;
        }
        Binding binding;
        if(!services.read_actual_button(request,binding,error)||!validate_binding(request,binding,error)) {
            if(error.empty())error="native_button_actual_button_read_failed_without_cause";persist();return false;
        }
        if(receipt.actual_attempt.binding!=binding) {error="native_button_actual_job_button_binding_differs";persist();return false;}
        std::string digest;
        if(!services.read_actual_executable_sha256(digest,error)||digest!=executable_sha256) {
            if(error.empty())error="native_button_actual_executable_receipt_differs";persist();return false;
        }
#ifdef _WIN32
        if(!actual_entry(binding,error)){persist();return false;}
        Binding fresh;
        if(!services.read_actual_button(request,fresh,error)||fresh!=binding||!validate_binding(request,fresh,error)) {
            if(error.empty())error="native_button_actual_binding_changed_before_native_call";persist();return false;
        }
        receipt.invocation_reserved=true;receipt.phase=Phase::PreparedNativeInvocation;
        if(!persist())return false;
        // Root receipt storage must not dispatch native callbacks or alter the binding.
        if(!services.read_actual_button(request,fresh,error)||fresh!=binding||!actual_entry(fresh,error)) {
            receipt.phase=Phase::RejectedNoNativeEffect;
            if(error.empty())error="native_button_actual_binding_changed_after_attempt_record";persist();return false;
        }
        receipt.phase=Phase::Unknown;
        const auto code=call_seh(reinterpret_cast<ButtonActivate>(binding.image+button_activate_rva),reinterpret_cast<void*>(binding.button),&receipt.native_attempted);
        receipt.seh_code=code;
        if(code){error="native_button_original_SEH="+std::to_string(code);persist();return false;}
        receipt.native_returned=true;receipt.phase=Phase::NativeReturnedAwaitingAuthority;error.clear();
        return persist();
#else
        error="native_button_Windows_actual_entry_required";persist();return false;
#endif
    }catch(const std::exception& original) {
        error="native_button_original_exception: "+std::string(original.what());
    }catch(...) {error="native_button_original_nonstandard_exception";}
    if(receipt.native_attempted)receipt.phase=Phase::Unknown;
    try {persist();}
    catch(const std::exception& cause){error+="; native_button_receipt_exception: "+std::string(cause.what());receipt.cause=error;}
    catch(...){error+="; native_button_receipt_nonstandard_exception";receipt.cause=error;}
    return false;
}
}
