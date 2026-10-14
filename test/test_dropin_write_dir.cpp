#include "mt2mp/core/private_profile_paths.h"
#include <winioctl.h>
#include <atomic>
#include <memory>
#include <iostream>
#include <vector>
#include <cstring>

std::filesystem::path data_dir;
std::unique_ptr<mt2mp::private_profile::DirectoryPins> write_pins;
std::atomic<unsigned> isolation{0};
std::vector<std::string> records;
std::string forwarded;
unsigned original_calls=0;
int original_result=1;
void ensure_profile(){}
void record(const char*,const char* value,DWORD=0){records.emplace_back(value);}
int original_write_dir(const char* value){++original_calls;forwarded=value?value:"<null>";return original_result;}
#include "private_write_dir_body.inc"

void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void reset(){records.clear();original_calls=0;isolation.store(0);original_result=1;}
void accepted(const std::string& value){
    reset();require(private_write_dir(value.c_str())==1,"valid trailing directory refused");
    require(original_calls==1&&forwarded==value,"original PhysFS argument changed");
    require(isolation.load()==2,"PhysFS acknowledgement missing");
}
void rejected(const std::string& value,const char* cause){
    reset();require(private_write_dir(value.c_str())==0,"unsafe directory accepted");
    require(original_calls==0&&isolation.load()==0,"rejected path reached original PhysFS");
    require(!records.empty()&&records.back().find(cause)!=std::string::npos,"original rejection cause lost");
}
void junction(const std::filesystem::path& link,const std::filesystem::path& target){
    require(std::filesystem::create_directory(link),"junction fixture directory exists");
    struct MountPoint {
        DWORD tag;WORD bytes,reserved;WORD substitute_offset,substitute_bytes,print_offset,print_bytes;
        wchar_t path[4096];
    } value{};
    static_assert(offsetof(MountPoint,path)==16);
    const auto substitute=L"\\??\\"+target.wstring();
    require(substitute.size()+2<std::size(value.path),"junction fixture path is too long");
    value.tag=IO_REPARSE_TAG_MOUNT_POINT;
    value.substitute_bytes=WORD(substitute.size()*sizeof(wchar_t));
    value.print_offset=WORD(value.substitute_bytes+sizeof(wchar_t));
    value.bytes=WORD(8+value.substitute_bytes+2*sizeof(wchar_t));
    std::memcpy(value.path,substitute.data(),value.substitute_bytes);
    const HANDLE file=CreateFileW(link.c_str(),GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,nullptr);
    require(file!=INVALID_HANDLE_VALUE,"junction fixture open failed");
    DWORD written=0;const BOOL result=DeviceIoControl(file,FSCTL_SET_REPARSE_POINT,&value,DWORD(value.bytes)+8,nullptr,0,&written,nullptr);
    const DWORD cause=GetLastError();
    require(CloseHandle(file)!=0,"junction fixture close failed");
    if(!result)throw std::runtime_error("junction fixture creation failed Win32="+std::to_string(cause));
}
int wmain(int argc,wchar_t** argv){
    try{
        require(argc<=2,"usage: test_dropin_write_dir [new-empty-owned-directory]");
        const auto owner=argc==2?std::filesystem::absolute(argv[1]):std::filesystem::current_path()/(L"write_dir_fixture_"+std::to_wstring(GetCurrentProcessId())+L"_"+std::to_wstring(GetTickCount64()));
        if(argc==1)require(std::filesystem::create_directory(owner),"fresh fixture creation failed");
        require(std::filesystem::is_directory(owner)&&std::filesystem::is_empty(owner),"fixture must be a new empty owned directory");
        data_dir=owner/L"data";
        std::filesystem::create_directories(data_dir/L"child");std::filesystem::create_directory(owner/L"data-sibling");
        const auto data=mt2mp::private_profile::utf8(data_dir);
        accepted(data+"/");accepted(data+"\\");accepted(data+"///");accepted(data+"/child/");
        rejected(mt2mp::private_profile::utf8(owner/L"data-sibling")+"/","outside confirmed private data root");
        rejected(data+"/../data/","profile path component refused");
        rejected(data+"/./child/","profile path component refused");
        junction(data_dir/L"alias",data_dir/L"child");
        rejected(data+"/alias/","profile directory reparse or non-directory refused");
        reset();original_result=0;require(private_write_dir((data+"/").c_str())==0,"original PhysFS refusal hidden");
        require(original_calls==1&&isolation.load()==0&&!records.empty()&&records.back()=="original PHYSFS_setWriteDir refused private directory","original PhysFS refusal cause changed");
        reset();require(private_write_dir(nullptr)==1&&forwarded=="<null>","null original write directory behavior changed");
        write_pins.reset();
        std::cout<<"PASS: actual caller body; root/child terminal separators, sibling/raw-parent/raw-dot/reparse rejection, original refusal and null passthrough\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"error: write directory fixture: "<<error.what()<<'\n';return 1;}
}
