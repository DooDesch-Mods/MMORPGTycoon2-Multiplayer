struct ActivityNativeCalls {
    std::array<unsigned,3> attempted{},returned{};DWORD exception=0;
    bool unknown=false;std::string cause;
};
ActivityNativeCalls activity_calls;
bool activity_faulted=false;std::string activity_fault_cause;
struct alignas(8) NativeLocStorage {unsigned char bytes[0x38]{};};
struct alignas(8) NativeArgStorage {unsigned char bytes[0x68]{};};
static_assert(sizeof(NativeLocStorage)==0x38&&sizeof(NativeArgStorage)==0x68);
DWORD activity_get(uintptr_t image,void* out,uintptr_t developer){
    ++activity_calls.attempted[0];
    __try{reinterpret_cast<void*(*)(void*,void*)>(image+0x3a72d0)(out,reinterpret_cast<void*>(developer));++activity_calls.returned[0];return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){activity_calls.exception=GetExceptionCode();activity_calls.unknown=true;return activity_calls.exception;}
}
DWORD activity_name(uintptr_t image,void* out,uintptr_t project){
    ++activity_calls.attempted[0];
    __try{reinterpret_cast<void*(*)(void*,void*)>(image+0x2642a0)(out,reinterpret_cast<void*>(project));++activity_calls.returned[0];return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){activity_calls.exception=GetExceptionCode();activity_calls.unknown=true;return activity_calls.exception;}
}
DWORD activity_arg(uintptr_t image,void* out,const void* name,const void* value){
    ++activity_calls.attempted[1];
    __try{reinterpret_cast<void(*)(void*,const void*,const void*)>(image+0xc6c620)(out,name,value);++activity_calls.returned[1];return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){activity_calls.exception=GetExceptionCode();activity_calls.unknown=true;return activity_calls.exception;}
}
DWORD activity_format(uintptr_t image,void* out,const char* key,const void* arg){
    ++activity_calls.attempted[1];
    __try{reinterpret_cast<void*(*)(void*,const char*,const void*)>(image+0xa94360)(out,key,arg);++activity_calls.returned[1];return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){activity_calls.exception=GetExceptionCode();activity_calls.unknown=true;return activity_calls.exception;}
}
DWORD activity_destroy(uintptr_t image,uintptr_t rva,void* value){
    ++activity_calls.attempted[2];
    __try{reinterpret_cast<void(*)(void*)>(image+rva)(value);++activity_calls.returned[2];return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){activity_calls.exception=GetExceptionCode();activity_calls.unknown=true;return activity_calls.exception;}
}
DWORD activity_count(uintptr_t image,uintptr_t rva,uintptr_t lounge,int32_t& count){
    ++activity_calls.attempted[0];
    __try{count=reinterpret_cast<int32_t(*)(void*)>(image+rva)(reinterpret_cast<void*>(lounge));++activity_calls.returned[0];return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){activity_calls.exception=GetExceptionCode();activity_calls.unknown=true;return activity_calls.exception;}
}
bool activity_error(DWORD code,const char* operation,std::string& e){return !code||fail(e,std::string("Staff native ")+operation+" exception="+std::to_string(code));}
bool activity_release(uintptr_t image,NativeLocStorage& value,std::string& e){
    const auto vector_code=activity_destroy(image,0x11b2b00,value.bytes+0x20);
    // The original caller destroys the vector first, then its owning key.
    // If vector destruction faults, its heap state is unknown; do not retry.
    if(!activity_error(vector_code,"LocString vector destruction",e))return false;
    return activity_error(activity_destroy(image,0x11bd1e0,value.bytes),"LocString key destruction",e);
}
bool activity_code(uintptr_t image,std::string& e){
    constexpr tutorial_replication::start_intent::Body bodies[]={
        {0x25ead0,0x30,"e4dfe933c75c30580beb9af9d9b2f452a14f7eecc884f18a0f04403694ee2342"},
        {0x25ed30,0x10,"be3bfed589ca5c1e017fde51c58ea449969df8a5de1d1bc570b1ce28040332e5"},
        {0x261290,0x70,"f02798dc1cb8c4dd48e11ee96d36c734d094628bc928fab5b29b0e6302bd93fd"},
        {0x2611d0,0x10,"a2e81e07d85eb54e9d31229cbef2db47c97b3a90c5ffafce5311ea3a4137675e"},
        {0x3a72d0,0x5a0,"f75bc152fdab08bb87bbfa3c550b3d698bafd8e18d6b144ea736a3585f3f02f6"},
        {0x2642a0,0x510,"0b10dbcecf6fce21cea41662a4787f490894f6061b1073b2010beda59591558a"},
        {0xc6c620,0x150,"1405f3da1ddaafa39b05530c3c562bf439e7813cc763935ad9731dd1095eff90"},
        {0xa94360,0x120,"6376258870bccef03faf08a6651149ce5bc065e1665f819fbae62a9bbf16d521"},
        {0xc6c9b0,0x60,"79286ae0d1b13d0b256e0340f6c15f3a886a2783b69d0eba22604e395caee9a2"},
        {0x11b2b00,0xb0,"e6155e49c38d380ed513ce15ead8505762a5e50d6f1bde2e9985ec668fb95524"},
        {0x11bd1e0,0x20,"4a8cc79ce002b866698caded8eaea996814366638d34ce5cdf3ccb0394dc95e5"}
    };
    for(const auto& entry:bodies){std::vector<unsigned char> bytes(entry.size);
        if(!memory.copy(image+entry.rva,bytes.data(),bytes.size(),e))return false;
        if(mt2mp::sha256(bytes)!=entry.sha)return fail(e,"Staff native activity body differs RVA="+std::to_string(entry.rva));}
    return true;
}
