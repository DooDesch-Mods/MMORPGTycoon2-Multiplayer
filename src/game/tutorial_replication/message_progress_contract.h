#pragma once
#include "mt2mp/core/hash.h"
#include <cstdint>
#include <string>
#include <vector>
namespace mt2mp::game::tutorial_replication::message_progress {
inline constexpr size_t function_size=0xf0;
struct Fingerprints {const char* set_label;const char* set_value;};
inline constexpr Fingerprints production_fingerprints{"b25d975190e7aa34e1de6c1661bec22242befaa92a11f9305cfe6a0bda89b47a","a4fa08666431c3fb2db6335658f5d0f02e69313619607275371804cb59386429"};
template<class Memory,class Hash>bool verify_code(Memory& memory,uintptr_t image,std::string& error,const Fingerprints& expected,Hash&& hash){
    if(!image||image>UINTPTR_MAX-0x84b7c0){error="tutorial progress native image invalid";return false;}
    struct Function {uintptr_t rva;const char* digest;const char* mismatch;};
    const Function functions[]={{0x8495e0,expected.set_label,"tutorial progress native SetLabel body differs"},
                                {0x84b6d0,expected.set_value,"tutorial progress native SetValue body differs"}};
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
