#include "release_boundary_frames.h"
#include <Windows.h>
#include <array>
namespace mt2mp::game::release_boundary {
namespace {
struct Unwind {CONTEXT context{};DWORD64 image=0;void* data=nullptr;DWORD64 establisher=0;PRUNTIME_FUNCTION function=nullptr;};
uint32_t step(Unwind& frame)noexcept{
    __try{frame.function=RtlLookupFunctionEntry(frame.context.Rip,&frame.image,nullptr);
        if(frame.function)RtlVirtualUnwind(UNW_FLAG_NHANDLER,frame.image,frame.context.Rip,frame.function,&frame.context,&frame.data,&frame.establisher,nullptr);
        else{frame.context.Rip=*reinterpret_cast<const DWORD64*>(frame.context.Rsp);frame.context.Rsp+=8;}
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool current_native_frames(uintptr_t image,size_t image_bytes,std::vector<uintptr_t>& out,std::string& e){
    out.clear();if(!image||!image_bytes||image>UINTPTR_MAX-image_bytes){e="Release unwind native image extent invalid";return false;}
    ULONG_PTR low=0,high=0;GetCurrentThreadStackLimits(&low,&high);Unwind frame;RtlCaptureContext(&frame.context);
    for(size_t n=0;n<64&&frame.context.Rip;++n){
        if(frame.context.Rsp<low||frame.context.Rsp>high||high-frame.context.Rsp<8){e="Release unwind left actual thread stack bounds";return false;}
        if(frame.context.Rip>=image&&frame.context.Rip-image<image_bytes){out.push_back(uintptr_t(frame.context.Rip-image));if(out.size()==8){e.clear();return true;}}
        const auto before=frame.context.Rsp;const auto exception=step(frame);
        if(exception||frame.context.Rsp<=before){e="Release actual metadata unwind failed SEH="+std::to_string(exception);return false;}
    }
    if(out.empty()){e="Release actual metadata unwind contains no native game frame";return false;}e.clear();return true;
}
}
