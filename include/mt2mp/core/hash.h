#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <vector>
#include <string>
#include <stdexcept>
#pragma comment(lib,"bcrypt.lib")
namespace mt2mp {
inline std::string sha256(const std::vector<unsigned char>& bytes) {
    BCRYPT_ALG_HANDLE algorithm=nullptr; BCRYPT_HASH_HANDLE hash=nullptr;
    NTSTATUS code=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0);
    DWORD size=0,received=0;
    if(code>=0) code=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&received,0);
    std::vector<unsigned char> state(size);
    if(code>=0) code=BCryptCreateHash(algorithm,&hash,state.data(),size,nullptr,0,0);
    if(code>=0&&!bytes.empty()) code=BCryptHashData(hash,const_cast<PUCHAR>(bytes.data()),ULONG(bytes.size()),0);
    unsigned char digest[32]{};
    if(code>=0) code=BCryptFinishHash(hash,digest,sizeof(digest),0);
    if(hash) BCryptDestroyHash(hash);
    if(algorithm) BCryptCloseAlgorithmProvider(algorithm,0);
    if(code<0) throw std::runtime_error("SHA256 BCrypt failure: "+std::to_string(code));
    std::string result; constexpr char digits[]="0123456789abcdef";
    for(auto byte:digest) { result+=digits[byte>>4]; result+=digits[byte&15]; }
    return result;
}
}
