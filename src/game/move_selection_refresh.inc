uint32_t refresh_outline_seh(uintptr_t image,uintptr_t stage,uintptr_t target)noexcept{
    __try{
        reinterpret_cast<void(__fastcall*)(void*,void*)>(image+0x1e5540)(
            reinterpret_cast<void*>(stage),reinterpret_cast<void*>(target));
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool refresh_outline(Runtime& runtime,uintptr_t target,move_selection_refresh::Receipt& receipt,std::string& error){
    if(GetCurrentThreadId()!=runtime.thread||!target){error="move outline refresh requires bound mainthread and current target";return false;}
    struct Pin{uintptr_t rva;size_t bytes;const char* sha;};
    constexpr Pin pins[]{
        {0x1e5540,2512,"49c5bdb0ef3acf0dd49a3d0924537de9b2e5008b61328e9c963f3797a0fb44ed"},
        {0x1e4c70,896,"354416f6c7c2247be410c5425353eaaea926dd786ca2945843b4ee46606cc778"},
        {0x1e5230,784,"af7d763126cd0974268ad33720d0a87665a0071a2578304f5c8c396254f06419"},
    };
    for(const auto& pin:pins){
        std::vector<unsigned char> bytes(pin.bytes);
        if(!runtime.memory.read(runtime.image+pin.rva,bytes.data(),bytes.size(),error))return false;
        if(mt2mp::sha256(bytes)!=pin.sha){error="move outline original body differs RVA="+std::to_string(pin.rva);return false;}
    }
    uintptr_t stage=0,addresspoint=0;
    if(!runtime.memory.read(runtime.image+0x13eb8d0,&stage,sizeof stage,error))return false;
    if(!stage){error="move outline current renderer is missing";return false;}
    if(!runtime.memory.read(stage,&addresspoint,sizeof addresspoint,error))return false;
    if(addresspoint!=runtime.image+0x1745310){error="move outline current renderer class differs";return false;}
    receipt.attempted=true;receipt.exception=refresh_outline_seh(runtime.image,stage,target);
    receipt.returned=receipt.exception==0;
    if(!receipt.returned){error="move outline original AddToOutline failed SEH="+std::to_string(receipt.exception);return false;}
    return true;
}
