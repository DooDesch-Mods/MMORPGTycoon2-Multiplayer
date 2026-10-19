#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
extern "C" {
#include "trampoline.h"
#include "hde/hde64.h"
}

namespace {
unsigned boundary_checks=0;
bool require(bool value,const char* operation,const char* name){
    ++boundary_checks;
    if(!value)std::fprintf(stderr,"FAIL %s: %s\n",name,operation);
    return value;
}
struct Code {uintptr_t rva;size_t bytes;const char* sha;std::array<unsigned char,32> prefix;};
struct Memory {
    uintptr_t begin=0;size_t size=0;bool denied=false;
    bool copy(uintptr_t address,void* out,size_t count,std::string& error)const{
        if(denied||address<begin||address-begin>size||count>size-(address-begin)){
            error="fixture trampoline read denied";return false;
        }
        std::memcpy(out,reinterpret_cast<const void*>(address),count);return true;
    }
};
bool fail(std::string& error,const char* cause){error=cause;return false;}
using Check=bool(*)(Memory&,uintptr_t,uintptr_t,std::string&);
struct Case {const char* name;const Code* code;size_t copied;Check check;};
#include "dungeon_boundaries.inc"

bool exercise(const Case& row){
    std::array<unsigned char,64> target{},trampoline{},detour{};
    target.fill(0x90);std::copy(row.code->prefix.begin(),row.code->prefix.end(),target.begin());
    TRAMPOLINE native{};native.pTarget=target.data();native.pDetour=detour.data();native.pTrampoline=trampoline.data();
    if(!require(CreateTrampolineFunction(&native)!=FALSE,"actual MinHook builds trampoline",row.name))return false;
    if(!require(!native.patchAbove&&native.nIP>1,"ordinary entry patch",row.name))return false;
    const size_t cut=native.oldIPs[native.nIP-1];
    size_t decoded=0;
    while(decoded<sizeof(JMP_REL)){
        hde64s instruction{};const auto length=hde64_disasm(target.data()+decoded,&instruction);
        if(!require(length&&!(instruction.flags&F_ERROR),"actual HDE decodes whole instruction",row.name))return false;
        decoded+=length;
    }
    if(!require(cut==decoded&&cut==row.copied,"production cut equals actual MinHook and HDE cut",row.name))return false;
    if(!require(native.newIPs[native.nIP-1]==cut&&native.pRelay==trampoline.data()+cut+sizeof(JMP_ABS),
                "copied instructions and continuation have exact physical extent",row.name))return false;
    const auto entry=reinterpret_cast<uintptr_t>(target.data());
    const auto original=reinterpret_cast<uintptr_t>(trampoline.data());
    const auto image=entry-row.code->rva;
    Memory memory{original,trampoline.size()};std::string error;
    if(!require(row.check(memory,image,original,error),"production guard accepts actual MinHook trampoline",row.name))return false;
    const auto saved=trampoline;
    for(const size_t offset:std::array<size_t,3>{0,cut,cut+6}){
        trampoline=saved;trampoline[offset]^=1;error.clear();
        if(!require(!row.check(memory,image,original,error)&&!error.empty(),
                    "production guard rejects changed instruction, continuation or resume",row.name))return false;
    }
    trampoline=saved;memory.denied=true;error.clear();
    if(!require(!row.check(memory,image,original,error)&&error=="fixture trampoline read denied",
                "read failure retains cause",row.name))return false;
    memory.denied=false;
    if(row.code->rva==gizmo::ui_code.rva){
        error.clear();
        if(!require(!legacy_gizmo::check(memory,image,original,error)&&error.find("six-byte")!=std::string::npos,
                    "original production six-byte guard reproduces failure",row.name))return false;
        trampoline.fill(0);std::copy_n(target.begin(),6,trampoline.begin());
        const JMP_ABS wrong{0xff,0x25,0,static_cast<UINT64>(entry+6)};
        std::memcpy(trampoline.data()+6,&wrong,sizeof wrong);error.clear();
        if(!require(!row.check(memory,image,original,error),"corrected guard rejects fabricated six-byte trampoline",row.name))return false;
    }
    return true;
}
}
int main(){
    for(const auto& row:cases)if(!exercise(row))return 1;
    std::printf("PASS %u actual MinHook/HDE and extracted Dungeon boundary checks\n",boundary_checks);
    return 0;
}
