#include <Windows.h>
#include <MinHook.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <string>

struct BoundaryCase {const char* name;std::array<unsigned char,32> prefix;size_t copied,jump,resume,size,offset;};
#include "native_ui_boundaries.inc"
void detour(){}
bool check(const BoundaryCase& test,std::string& error){
    auto* target=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
    if(!target){error="VirtualAlloc owned test function failed error="+std::to_string(GetLastError());return false;}
    std::memset(target,0x90,4096);std::memcpy(target,test.prefix.data(),test.prefix.size());target[64]=0xc3;
    void* original=nullptr;const auto created=MH_CreateHook(target,reinterpret_cast<void*>(&detour),&original);
    if(created!=MH_OK)error="MH_CreateHook disabled failed status="+std::to_string(created);
    else {
        const auto* trampoline=static_cast<const unsigned char*>(original);uintptr_t resume=0;
        constexpr std::array<unsigned char,6> continuation{0xff,0x25,0,0,0,0};
        if(!original||test.size>64||test.offset+8>test.size||test.copied>test.prefix.size()||test.jump+6>test.size)error="extracted production guard span invalid";
        else {
            std::memcpy(&resume,trampoline+test.offset,8);
            if(!std::equal(test.prefix.begin(),test.prefix.end(),target))error="disabled hook modified original test prefix";
            else if(!std::equal(test.prefix.begin(),test.prefix.begin()+test.copied,trampoline)||
                    !std::equal(continuation.begin(),continuation.end(),trampoline+test.jump)||resume!=uintptr_t(target)+test.resume)
                error="actual MinHook trampoline differs from production guard copied="+std::to_string(test.copied)+" jump="+std::to_string(test.jump)+" resume="+std::to_string(test.resume);
        }
        const auto removed=MH_RemoveHook(target);if(removed!=MH_OK)error+="; MH_RemoveHook disabled failed status="+std::to_string(removed);
    }
    if(!VirtualFree(target,0,MEM_RELEASE))error+="; VirtualFree test function failed error="+std::to_string(GetLastError());
    if(!error.empty())error=std::string(test.name)+": "+error;return error.empty();
}
int main(){
    const auto initialized=MH_Initialize();if(initialized!=MH_OK){std::cerr<<"MH_Initialize failed status="<<initialized<<'\n';return 1;}
    size_t passed=0;bool failed=false;
    for(const auto& test:cases){std::string error;if(check(test,error)){++passed;std::cout<<"PASS "<<test.name<<" copied="<<test.copied<<'\n';}else{failed=true;std::cerr<<error<<'\n';}}
#ifdef MT2MP_NATIVE_UI_LEGACY_CASE
    {std::string error;if(check(legacy_palette_case,error)||error.find("actual MinHook trampoline differs")==std::string::npos){failed=true;std::cerr<<"Retained old Palette guard was not rejected: "<<error<<'\n';}else std::cout<<"PASS retained old Palette guard rejected: "<<error<<'\n';}
#endif
    const auto uninitialized=MH_Uninitialize();if(uninitialized!=MH_OK){failed=true;std::cerr<<"MH_Uninitialize failed status="<<uninitialized<<'\n';}
    std::cout<<passed<<"/"<<cases.size()<<" actual disabled MinHook/native UI guard comparisons passed; target functions never executed\n";return failed?1:0;
}
