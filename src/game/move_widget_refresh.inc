uint32_t refresh_widgets_seh(uintptr_t image,uintptr_t target)noexcept{
    __try{
        reinterpret_cast<void(__fastcall*)(void*)>(image+0x40b710)(reinterpret_cast<void*>(target));
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool refresh_widgets(Runtime& runtime,uintptr_t target,move_widget_refresh::Receipt& receipt,std::string& error){
    auto call=[&](move_widget_refresh::Receipt& result,std::string& cause){
        if(GetCurrentThreadId()!=runtime.thread||!target){cause="move widget refresh requires bound mainthread and current target";return false;}
        struct Pin{uintptr_t rva;size_t bytes;const char* sha;};
        constexpr Pin pins[]{
            {0x40b710,496,"d76bb991f58ef63f0f0c45d3af3d97db9f653a30368b48aec3ced5794d30b55d"},
            {0x419530,256,"748c880ff848531c5cd18e443228aa2cfe7eae746471cce1408a2820f48daf2f"},
            {0x4196c0,400,"ba18f3a585155927ee45bf25ed3cc1c2ab8a87bcaa5d288d585595fce4bc063f"},
            {0x40b440,288,"4a170cfcddd14cf213f03ccca523c6c6561f010166351e87a7a3d35a112040f8"},
            {0xa0a490,112,"b285c9e711211732ebcb32d5f035c9968feaa4334e4bc463fcbfacef2e05a569"},
            {0x9ad3b0,336,"514e8c3499a65001ba8f680ef8e705fabfc0b3844197c6b3ffa9c9135632c139"},
            {0xa02d30,320,"6dcfa211f16dd66077f2ecc8f43526fcf4dce31d602a40071ba50167d96c6b28"},
            {0xa020b0,64,"2ab6018352c74cf35f351f1282791a2ac00236bc82cfb01981cc5fb3761d7c40"},
            {0x9ad1a0,48,"ec655a7f85d37cd1768c896c23adbab4e65a05289ee6dea27d0a72b3d26252fe"},
        };
        for(const auto& pin:pins){
            std::vector<unsigned char> bytes(pin.bytes);
            if(!runtime.memory.read(runtime.image+pin.rva,bytes.data(),bytes.size(),cause))return false;
            if(mt2mp::sha256(bytes)!=pin.sha){cause="move widget original body differs RVA="+std::to_string(pin.rva);return false;}
        }
        result.attempted=true;result.exception=refresh_widgets_seh(runtime.image,target);
        result.returned=result.exception==0;
        if(!result.returned){cause="move widget original _UpdateWidgets failed SEH="+std::to_string(result.exception);return false;}
        return true;
    };
    return move_widget_refresh::refresh(runtime.image,target,receipt,
        [&](uintptr_t address,void* output,size_t bytes,std::string& cause){return runtime.memory.read(address,output,bytes,cause);},
        call,error);
}
