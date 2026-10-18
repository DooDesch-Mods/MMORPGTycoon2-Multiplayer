#pragma once
#ifdef MT2MP_FRAMEWORK_PLUGIN
#include "mt2mp/core/config.h"
#include "mt2mp/core/private_profile_paths.h"
#include "mt2mp/net/snapshot_world_path.h"
#include <filesystem>
#include <algorithm>
#include <stdexcept>

namespace mt2mp::framework_open_paths {
namespace fs=std::filesystem;
struct Binding {
    fs::path runtime,game,profile,data;
    std::string raw_runtime,raw_game,raw_profile,raw_data;
    bool diagnostic_mailbox=false,local_test=false;
};
inline fs::path absolute(const std::string& value) {
    if(value.empty())throw std::runtime_error("Framework open path is empty");
    auto path=fs::path(private_profile::wide_utf8(value.c_str()));
    if(!path.is_absolute())throw std::runtime_error("Framework open path must be absolute");
    for(const auto& part:path)if(part==L"."||part==L"..")throw std::runtime_error("Framework open path contains traversal");
    while(path.has_relative_path()&&path.filename().empty())path=path.parent_path();
    return path.lexically_normal();
}
inline Binding bind(const fs::path& runtime) {
    const auto& cfg=Config::instance();
    if(!cfg.framework_profile_verified||cfg.installed_profile_verified)
        throw std::runtime_error("Framework open requires one verified SDK profile");
    Binding b{absolute(cfg.runtime_root),absolute(cfg.game_dir),absolute(cfg.framework_profile_directory),absolute(cfg.user_data_dir),
              cfg.runtime_root,cfg.game_dir,cfg.framework_profile_directory,cfg.user_data_dir,
              cfg.diagnostic_mailbox_enabled,cfg.local_test};
    if(!runtime.is_absolute()||!private_profile::equal(runtime,b.runtime))
        throw std::runtime_error("Framework open caller runtime differs from verified SDK binding");
    if(b.diagnostic_mailbox) {
        if(!private_profile::equal(b.game,b.runtime/L"game")||!private_profile::equal(b.profile,b.runtime/L"profile")||
           !private_profile::equal(b.data,b.profile/L"game-data"))
            throw std::runtime_error("Framework diagnostic open requires exact sibling game/profile and isolated game-data");
    }else if(b.local_test||!private_profile::equal(b.runtime,b.profile/L"data/doodesch.multiplayer")||
             private_profile::equal(b.game,b.profile)||private_profile::equal(b.game,b.data)||private_profile::equal(b.profile,b.data)) {
        throw std::runtime_error("Framework production open mod storage differs from verified SDK layout");
    }
    private_profile::DirectoryPins pins;
    for(const auto& path:{b.runtime,b.game,b.profile,b.data})pins.verify(path);
    return b;
}
inline void unchanged(const Binding& b) {
    const auto& cfg=Config::instance();
    if(!cfg.framework_profile_verified||cfg.installed_profile_verified||cfg.runtime_root!=b.raw_runtime||
       cfg.game_dir!=b.raw_game||cfg.framework_profile_directory!=b.raw_profile||cfg.user_data_dir!=b.raw_data||
       cfg.diagnostic_mailbox_enabled!=b.diagnostic_mailbox||cfg.local_test!=b.local_test)
        throw std::runtime_error("Framework open verified profile binding changed");
}
inline fs::path actual_write(const Binding& b,const char* actual) {
    unchanged(b);
    if(!actual||!*actual)throw std::runtime_error("actual PhysFS write directory unavailable");
    const auto path=absolute(actual);
    if(private_profile::equal(path,b.data))return path;
    const auto account=path.filename().wstring();
    if(!private_profile::equal(path.parent_path(),b.data)||account.empty()||account.size()>20||
       !std::all_of(account.begin(),account.end(),[](wchar_t c){return c>=L'0'&&c<=L'9';}))
        throw std::runtime_error("actual PhysFS write directory requires Framework base or one decimal account child");
    const auto attributes=GetFileAttributesW(path.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES)
        throw std::runtime_error("actual Framework account directory unavailable Win32="+std::to_string(GetLastError()));
    if(!(attributes&FILE_ATTRIBUTE_DIRECTORY)||(attributes&FILE_ATTRIBUTE_REPARSE_POINT))
        throw std::runtime_error("actual Framework account directory reparse or non-directory refused");
    return path;
}
inline fs::path live_write(const Binding& b) {
    using Getter=const char*(*)();
    const auto module=GetModuleHandleW(L"libphysfs.dll");
    const auto getter=module?reinterpret_cast<Getter>(GetProcAddress(module,"PHYSFS_getWriteDir")):nullptr;
    return actual_write(b,getter?getter():nullptr);
}
inline fs::path verified_write_root(const fs::path& runtime,const char* actual) {
    return actual_write(bind(runtime),actual);
}
inline fs::path snapshot(const fs::path& runtime,const char* actual,const std::string& world,const std::string& uuid,bool create) {
    const auto b=bind(runtime);
    const auto path=net::snapshot_world_path::physical(actual_write(b,actual),world,uuid);
    private_profile::DirectoryPins pins;
    pins.verify(path.parent_path(),create);
    for(auto file:{path,fs::path(path.wstring()+L".incoming")}) {
        const auto attributes=GetFileAttributesW(file.c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES) {
            const auto cause=GetLastError();
            if(cause!=ERROR_FILE_NOT_FOUND&&cause!=ERROR_PATH_NOT_FOUND)
                throw std::runtime_error("Framework snapshot attributes unavailable Win32="+std::to_string(cause));
        }else if(attributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))
            throw std::runtime_error("Framework snapshot file reparse or directory refused");
    }
    return path;
}
}
#endif
