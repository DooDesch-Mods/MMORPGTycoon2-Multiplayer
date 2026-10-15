#pragma once
#include "mt2mp/core/hash.h"
#include <cstdint>
#include <string>
#include <vector>
namespace mt2mp::game::tutorial_replication::message_progress {
inline constexpr size_t function_size=0xf0;
struct Fingerprints {const char* set_label;const char* set_value;};
inline constexpr Fingerprints production_fingerprints{"f79c5fd94e545cdf82bd7353fb6876c5d63c3f6258719705ed8af2effb78505e","78620e59d8987aa4d0bfb5f8a35a5840478286d1a73330db1014ce69e3c4a44e"};
template<class Memory,class Hash>bool verify_code(Memory& memory,uintptr_t image,std::string& error,const Fingerprints& expected,Hash&& hash){
    if(!image||image>UINTPTR_MAX-0x8430e0){error="tutorial progress native image invalid";return false;}
    struct Function {uintptr_t rva;const char* digest;const char* mismatch;};
    const Function functions[]={{0x840f20,expected.set_label,"tutorial progress native SetLabel body differs"},
                                {0x842ff0,expected.set_value,"tutorial progress native SetValue body differs"}};
    for(const auto& function:functions){
        std::vector<unsigned char> actual(function_size);
        if(!memory.copy(image+function.rva,actual.data(),actual.size(),error))return false;
        try{if(hash(actual)!=function.digest){error=function.mismatch;return false;}}
        catch(const std::exception& cause){error=cause.what();return false;}
    }
    error.clear();return true;
}
template<class Memory>bool verify_code(Memory& memory,uintptr_t image,std::string& error,const Fingerprints& expected=production_fingerprints){
    return verify_code(memory,image,error,expected,[](const auto& bytes){return mt2mp::sha256(bytes);});
}
}
