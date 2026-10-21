uint32_t refresh_outline_seh(uintptr_t image,uintptr_t stage,uintptr_t target)noexcept{
    __try{
        reinterpret_cast<void(__fastcall*)(void*,void*)>(image+0x1e3180)(
            reinterpret_cast<void*>(stage),reinterpret_cast<void*>(target));
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool refresh_outline(Runtime& runtime,uintptr_t target,move_selection_refresh::Receipt& receipt,std::string& error){
    if(GetCurrentThreadId()!=runtime.thread||!target){error="move outline refresh requires bound mainthread and current target";return false;}
    struct Pin{uintptr_t rva;size_t bytes;const char* sha;};
    constexpr Pin pins[]{
        {0x1e3180,2512,"c7c853a95380a4be895f95ae84968c2a018830afbd7c4504c6f8904adcce3741"},
        {0x1e28b0,896,"4c0f884a0e5c75783cbfa85bfe43153b8d647151b6ad3add366b042e8886fcf8"},
        {0x1e2e70,784,"85e73ac841d421bb89ec6f80c813143ad23ec344a93d3531cf97c57704477adf"},
    };
    for(const auto& pin:pins){
        std::vector<unsigned char> bytes(pin.bytes);
        if(!runtime.memory.read(runtime.image+pin.rva,bytes.data(),bytes.size(),error))return false;
        if(mt2mp::sha256(bytes)!=pin.sha){error="move outline original body differs RVA="+std::to_string(pin.rva);return false;}
    }
    uintptr_t stage=0,addresspoint=0;
    if(!runtime.memory.read(runtime.image+0x13df830,&stage,sizeof stage,error))return false;
    if(!stage){error="move outline current renderer is missing";return false;}
    if(!runtime.memory.read(stage,&addresspoint,sizeof addresspoint,error))return false;
    if(addresspoint!=runtime.image+0x173ef70){error="move outline current renderer class differs";return false;}
    receipt.attempted=true;receipt.exception=refresh_outline_seh(runtime.image,stage,target);
    receipt.returned=receipt.exception==0;
    if(!receipt.returned){error="move outline original AddToOutline failed SEH="+std::to_string(receipt.exception);return false;}
    return true;
}
