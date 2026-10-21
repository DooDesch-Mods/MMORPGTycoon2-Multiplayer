uint32_t refresh_widgets_seh(uintptr_t image,uintptr_t target)noexcept{
    __try{
        reinterpret_cast<void(__fastcall*)(void*)>(image+0x406b50)(reinterpret_cast<void*>(target));
        return 0;
    }__except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool refresh_widgets(Runtime& runtime,uintptr_t target,move_widget_refresh::Receipt& receipt,std::string& error){
    auto call=[&](move_widget_refresh::Receipt& result,std::string& cause){
        if(GetCurrentThreadId()!=runtime.thread||!target){cause="move widget refresh requires bound mainthread and current target";return false;}
        struct Pin{uintptr_t rva;size_t bytes;const char* sha;};
        constexpr Pin pins[]{
            {0x406b50,496,"7556499756fa3e7b151af3e3ea09cf6159347c626c210fcbbc69e0c096158f43"},
            {0x414970,256,"748c880ff848531c5cd18e443228aa2cfe7eae746471cce1408a2820f48daf2f"},
            {0x414b00,400,"0a7e9c0f20dc14ab13ddd479b3c6457b7d45d9ca0590dcce009f4865913cac06"},
            {0x406880,288,"e8490b35f7759e6c73d496f53743315c98f3e83efb60e5cce52c1624a94f8fca"},
            {0x9ff8d0,112,"4e964526f56eab49b81f1169dbda5c490f86b6a5d32b3e2369a8e338e8bfbfbb"},
            {0x9a38c0,336,"514e8c3499a65001ba8f680ef8e705fabfc0b3844197c6b3ffa9c9135632c139"},
            {0x9f88e0,320,"6dcfa211f16dd66077f2ecc8f43526fcf4dce31d602a40071ba50167d96c6b28"},
            {0x9f7c60,64,"2ab6018352c74cf35f351f1282791a2ac00236bc82cfb01981cc5fb3761d7c40"},
            {0x9a36b0,48,"ec655a7f85d37cd1768c896c23adbab4e65a05289ee6dea27d0a72b3d26252fe"},
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
