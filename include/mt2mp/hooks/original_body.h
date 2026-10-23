#pragma once
#include "mt2mp/core/hash.h"
#include "mt2mp/hooks/code_body_digest.h"
#if defined(MT2MP_FRAMEWORK_PLUGIN)
#include "mt2mp/hooks/framework_backend.h"
#endif
#include <algorithm>
#include <cstring>

namespace mt2mp::hooks::framework {
namespace original_body_detail {
inline bool restore_prefix(uintptr_t entry,std::vector<unsigned char>& body,std::string& error){
#if defined(MT2MP_FRAMEWORK_PLUGIN)
    auto& backend=process_backend();MT2FW_MT2_RouteReceipt receipt{sizeof receipt};
    if(!backend.read_route(reinterpret_cast<void*>(entry),receipt)){error=backend.error();return false;}
    const auto count=std::min(body.size(),sizeof receipt.current);
    if(std::memcmp(body.data(),receipt.current,count)){error="Original body live prefix differs from authenticated route";return false;}
    std::memcpy(body.data(),receipt.pristine,count);
    // Authenticate the changed prefix; the caller still checks every original byte, including long tails.
    return backend.verify_code(entry,body.data(),count,error);
#else
    (void)entry;(void)body;error="Original body mismatch requires authenticated Framework route";return false;
#endif
}
}
inline bool verify_original_hash(uintptr_t entry,const void* observed,size_t size,const char* expected,std::string& error,CodeBodyDigest* memo=nullptr){
    if(!entry||!observed||!size||!expected||std::strlen(expected)!=64){error="Original body hash input or extent invalid";return false;}
    const auto* begin=static_cast<const unsigned char*>(observed);std::vector<unsigned char> body(begin,begin+size);
    const auto hash=[&](std::string& value){
        const auto calculate=[&](std::string& out,std::string&){out=mt2mp::sha256(body);return true;};
        return memo?memo->read(entry,body.data(),body.size(),expected,value,error,calculate):calculate(value,error);
    };
    std::string digest;if(!hash(digest))return false;
    if(digest==expected){error.clear();return true;}
    if(!original_body_detail::restore_prefix(entry,body,error))return false;
    if(!hash(digest))return false;
    if(digest!=expected){error="Authenticated original full body SHA differs";return false;}
    error.clear();return true;
}
inline bool verify_original_bytes(uintptr_t entry,const void* observed,const void* expected,size_t size,std::string& error){
    if(!entry||!observed||!expected||!size){error="Original body byte input or extent invalid";return false;}
    if(!std::memcmp(observed,expected,size)){error.clear();return true;}
    const auto* begin=static_cast<const unsigned char*>(observed);std::vector<unsigned char> body(begin,begin+size);
    if(!original_body_detail::restore_prefix(entry,body,error))return false;
    if(std::memcmp(body.data(),expected,size)){error="Authenticated original full body bytes differ";return false;}
    error.clear();return true;
}
}
