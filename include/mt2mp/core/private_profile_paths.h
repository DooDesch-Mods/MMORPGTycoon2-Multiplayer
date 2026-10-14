#pragma once
#include <windows.h>
#include <bcrypt.h>
#include <array>
#include <filesystem>
#include <string>
#include <stdexcept>
#include <vector>

namespace mt2mp::private_profile {
namespace fs=std::filesystem;
inline void require(bool condition,const char* operation){
    if(!condition)throw std::runtime_error(std::string(operation)+" Win32="+std::to_string(GetLastError()));
}
inline std::wstring wide_utf8(const char* input){
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,input,-1,nullptr,0);
    require(n>0,"UTF-8 path conversion");std::wstring value(size_t(n),L'\0');
    require(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,input,-1,value.data(),n)==n,"UTF-8 path conversion");value.pop_back();return value;
}
inline std::string utf8(const fs::path& path){
    const auto wide=path.wstring();const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.c_str(),-1,nullptr,0,nullptr,nullptr);
    require(n>0,"UTF-16 path conversion");std::string value(size_t(n),'\0');
    require(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,wide.c_str(),-1,value.data(),n,nullptr,nullptr)==n,"UTF-16 path conversion");value.pop_back();return value;
}
inline bool equal(const fs::path& left,const fs::path& right){
    const auto a=left.lexically_normal().make_preferred().wstring(),b=right.lexically_normal().make_preferred().wstring();
    return CompareStringOrdinal(a.c_str(),-1,b.c_str(),-1,TRUE)==CSTR_EQUAL;
}
inline bool descendant(const fs::path& path,const fs::path& root){
    auto p=path.begin();for(auto r=root.begin();r!=root.end();++r,++p)if(p==path.end()||!equal(*p,*r))return false;
    return p!=path.end();
}
inline fs::path final_path(HANDLE handle){
    std::array<wchar_t,32768> value{};const DWORD n=GetFinalPathNameByHandleW(handle,value.data(),DWORD(value.size()),FILE_NAME_NORMALIZED|VOLUME_NAME_DOS);
    require(n>0&&n<value.size(),"GetFinalPathNameByHandleW");std::wstring path(value.data(),n);
    if(path.rfind(L"\\\\?\\",0)==0)path.erase(0,4);
    require(path.size()>3&&path[1]==L':'&&path[2]==L'\\',"local absolute profile path required");return fs::path(path).lexically_normal();
}
inline fs::path canonical_file(const fs::path& path){
    HANDLE file=CreateFileW(path.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,nullptr);
    require(file!=INVALID_HANDLE_VALUE,"canonical file open");
    fs::path result;
    try{result=final_path(file);}
    catch(...){if(!CloseHandle(file))OutputDebugStringA("error: canonical file cleanup CloseHandle failed\n");throw;}
    require(CloseHandle(file)!=0,"canonical file close");return result;
}
inline fs::path module_file(HMODULE module){
    std::array<wchar_t,32768> value{};const DWORD n=GetModuleFileNameW(module,value.data(),DWORD(value.size()));
    require(n>0&&n<value.size(),"GetModuleFileNameW");return canonical_file(fs::path(value.data()));
}
class DirectoryPins {
    std::vector<HANDLE> pins;
public:
    DirectoryPins()=default;DirectoryPins(const DirectoryPins&)=delete;DirectoryPins& operator=(const DirectoryPins&)=delete;
    ~DirectoryPins(){for(auto pin:pins)if(!CloseHandle(pin))OutputDebugStringA("error: private profile directory CloseHandle failed\n");}
    void verify(const fs::path& directory,bool create=false){
        require(directory.is_absolute(),"profile directory must be absolute");
        fs::path current=directory.root_path();
        for(const auto& component:directory.relative_path()){
            const auto name=component.wstring();
            require(!name.empty()&&name!=L"."&&name!=L".."&&name.find(L':')==std::wstring::npos&&name.back()!=L'.'&&name.back()!=L' ',"profile path component refused");
            current/=component;
            if(create&&!CreateDirectoryW(current.c_str(),nullptr))require(GetLastError()==ERROR_ALREADY_EXISTS,"private profile CreateDirectoryW");
            HANDLE pin=CreateFileW(current.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,nullptr);
            require(pin!=INVALID_HANDLE_VALUE,"profile directory pin open");pins.push_back(pin);
            FILE_ATTRIBUTE_TAG_INFO info{};require(GetFileInformationByHandleEx(pin,FileAttributeTagInfo,&info,sizeof(info))!=0,"profile directory attributes");
            require((info.FileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0&&(info.FileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)==0,"profile directory reparse or non-directory refused");
            require(equal(final_path(pin),current),"profile directory alias refused");
        }
    }
};
inline std::string hex(const unsigned char* bytes,size_t size){
    constexpr char digits[]="0123456789abcdef";std::string value;value.reserve(size*2);
    for(size_t i=0;i<size;++i){value+=digits[bytes[i]>>4];value+=digits[bytes[i]&15];}return value;
}
inline std::wstring installation_id(const fs::path& game){
    const auto value=game.lexically_normal().wstring();
    const int n=LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,value.c_str(),int(value.size()),nullptr,0,nullptr,nullptr,0);
    require(n>0,"installation path case mapping");std::wstring normalized(size_t(n),L'\0');
    require(LCMapStringEx(LOCALE_NAME_INVARIANT,LCMAP_LOWERCASE,value.c_str(),int(value.size()),normalized.data(),n,nullptr,nullptr,0)==n,"installation path case mapping");
    std::array<unsigned char,32> digest{};
    const auto status=BCryptHash(BCRYPT_SHA256_ALG_HANDLE,nullptr,0,reinterpret_cast<PUCHAR>(normalized.data()),ULONG(normalized.size()*sizeof(wchar_t)),digest.data(),ULONG(digest.size()));
    if(status<0)throw std::runtime_error("installation path SHA256 NTSTATUS="+std::to_string(status));
    const auto text=hex(digest.data(),digest.size());return std::wstring(text.begin(),text.end());
}
inline fs::path runtime_root(const fs::path& game){
    std::array<wchar_t,32768> value{};const DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",value.data(),DWORD(value.size()));
    require(n>0&&n<value.size(),"LOCALAPPDATA unavailable");const fs::path base(value.data());
    require(base.is_absolute(),"LOCALAPPDATA must be absolute");return base/L"MT2Multiplayer"/installation_id(game);
}
inline std::string new_run_id(){
    std::array<unsigned char,16> bytes{};const auto status=BCryptGenRandom(nullptr,bytes.data(),ULONG(bytes.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    if(status<0)throw std::runtime_error("private run ID generation NTSTATUS="+std::to_string(status));
    bytes[6]=(bytes[6]&15)|64;bytes[8]=(bytes[8]&63)|128;
    auto value=hex(bytes.data(),bytes.size());value.insert(8,"-");value.insert(13,"-");value.insert(18,"-");value.insert(23,"-");return value;
}
}
