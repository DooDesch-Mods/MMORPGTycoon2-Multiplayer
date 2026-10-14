#include "native_refund.h"
#include "codeguards.h"
#include <Windows.h>
#include <cstring>
#include <exception>
#include <vector>
namespace mt2mp::game::native_refund {
__declspec(noinline) uint32_t invoke_actual(uintptr_t object,uintptr_t getter,int32_t& value) {
    __try{value=reinterpret_cast<int32_t(__fastcall*)(const void*)>(getter)(reinterpret_cast<const void*>(object));return 0;}
    __except(GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool read(build_actions::private_native::Memory& memory,uintptr_t image,uint32_t thread,uintptr_t object,
          uint64_t uid,uintptr_t addresspoint,Receipt& out,std::string& error,Invoke invoke) {
    out={};error.clear();
    if(thread!=GetCurrentThreadId()){error="native refund requires actual Mainthread";return false;}
    if(!image||image>UINTPTR_MAX-0x16d9640||!object||object>UINTPTR_MAX-0x270||!uid||!invoke){error="native refund image or object/UID binding invalid";return false;}
    const bool building=addresspoint==image+0x16d93c0,scenery=addresspoint==image+0x16c1390;
    if(!building&&!scenery){error="native refund actual Prop class unsupported";return false;}
    uintptr_t ap=0,getter=0,manager=0;uint64_t actual_uid=0;
    if(!memory.copy(object,&ap,sizeof ap,error)||!memory.copy(object+0x18,&actual_uid,sizeof actual_uid,error))return false;
    if(ap!=addresspoint||actual_uid!=uid){error="native refund actual class or UID differs";return false;}
    if(!memory.copy(ap+0x278,&getter,sizeof getter,error))return false;
    out.getter=getter;
    if(getter!=image+(building?0xd314f0:0xcf30e0)){error="native refund actual virtual getter differs";return false;}
    for(const auto& code:codes) {
        if(scenery&&code.rva!=0xcf30e0)continue;
        if(building&&code.rva==0xcf30e0)continue;
        std::vector<unsigned char> bytes(code.size);
        if(!memory.copy(image+code.rva,bytes.data(),bytes.size(),error))return false;
        if(std::memcmp(bytes.data(),code.bytes,code.size)){error="native refund original getter/callee code differs RVA="+std::to_string(code.rva);return false;}
    }
    if(building) {
        if(!memory.copy(image+0x13df780,&manager,sizeof manager,error))return false;
        if(!manager){error="native refund BuildingModelManager cache unavailable; initialization skipped";return false;}
    } else {
        int32_t category=-1;if(!memory.copy(object+0x268,&category,sizeof category,error))return false;
        if(category<0||category>=47){error="native refund Scenery category invalid; native assert skipped";return false;}
    }
    out.attempted=true;
    try{out.seh=invoke(object,getter,out.value);}
    catch(const std::exception& cause){error=std::string("native refund getter exception: ")+cause.what();return false;}
    catch(...){error="native refund getter nonstandard exception";return false;}
    if(out.seh){error="native refund getter SEH="+std::to_string(out.seh);return false;}
    out.returned=true;
    uintptr_t after_ap=0,after_getter=0,after_manager=0;uint64_t after_uid=0;
    if(!memory.copy(object,&after_ap,sizeof after_ap,error)||!memory.copy(object+0x18,&after_uid,sizeof after_uid,error)||
       !memory.copy(addresspoint+0x278,&after_getter,sizeof after_getter,error))return false;
    if(after_ap!=ap||after_uid!=uid||after_getter!=getter){error="native refund Prop identity changed after getter";return false;}
    if(building&&(!memory.copy(image+0x13df780,&after_manager,sizeof after_manager,error)||after_manager!=manager)) {
        if(error.empty())error="native refund BuildingModelManager changed after getter";return false;
    }
    out.confirmed=true;return true;
}
}
