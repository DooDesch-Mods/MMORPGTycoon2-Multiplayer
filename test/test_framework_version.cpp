#include "framework_version.h"
#include <array>
#include <iostream>
#include <stdexcept>

using mt2mp::framework_version::fixed_resource_at_least;
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int wmain(int argc,wchar_t** argv) try {
    std::string error;
    require(argc==2,"resource-less fixture path is required");
    require(mt2mp::framework_version::loaded_module_at_least(GetModuleHandleW(nullptr),error),"loaded minimum VERSIONINFO rejected");
    require(!mt2mp::framework_version::loaded_module_at_least(nullptr,error)&&error.find(MT2MP_FRAMEWORK_REQUIRED)==0,"missing module accepted");
    const auto empty=LoadLibraryW(argv[1]);require(empty!=nullptr,"resource-less fixture did not load");
    const bool empty_accepted=mt2mp::framework_version::loaded_module_at_least(empty,error);
    const bool empty_freed=FreeLibrary(empty)!=0;
    require(empty_freed&&!empty_accepted&&error.find("absent")!=std::string::npos,"loaded resource-less module accepted");
    // VS_FIXEDFILEINFO version words: ms = major<<16|minor, ls = patch<<16|build.
    constexpr uint32_t ms=uint32_t(MT2MP_FRAMEWORK_MAJOR)<<16|MT2MP_FRAMEWORK_MINOR,ls=uint32_t(MT2MP_FRAMEWORK_PATCH)<<16;
    static_assert(MT2MP_FRAMEWORK_MINOR>0,"the older-version cases lower the minor version");
    std::array<unsigned char,92> data{};
    const auto word=[&](size_t offset,uint16_t value){std::memcpy(data.data()+offset,&value,sizeof(value));};
    const auto dword=[&](size_t offset,uint32_t value){std::memcpy(data.data()+offset,&value,sizeof(value));};
    constexpr char16_t key[]=u"VS_VERSION_INFO";
    word(0,92);word(2,52);word(4,0);std::memcpy(data.data()+6,key,sizeof(key));
    dword(40,0xfeef04bd);dword(44,0x00010000);dword(48,ms);dword(52,ls);dword(56,ms);dword(60,ls);
    require(fixed_resource_at_least(data.data(),data.size(),error),"valid fixed minimum rejected");
    dword(48,ms-1);require(!fixed_resource_at_least(data.data(),data.size(),error)&&error.find("too old")!=std::string::npos,"older file version accepted");
    dword(48,ms);dword(56,ms-1);require(!fixed_resource_at_least(data.data(),data.size(),error),"older product version accepted");
    dword(56,ms+1);require(fixed_resource_at_least(data.data(),data.size(),error),"newer product version rejected");
    require(!fixed_resource_at_least(data.data(),data.size()-1,error),"truncated resource accepted");
    word(0,91);require(!fixed_resource_at_least(data.data(),data.size(),error),"short resource length accepted");word(0,92);
    word(2,51);require(!fixed_resource_at_least(data.data(),data.size(),error),"wrong fixed length accepted");word(2,52);
    data[6]^=1;require(!fixed_resource_at_least(data.data(),data.size(),error),"wrong resource key accepted");data[6]^=1;
    dword(40,0);require(!fixed_resource_at_least(data.data(),data.size(),error),"wrong fixed signature accepted");
    std::cout<<"PASS loaded module resource and minimum version validation\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
