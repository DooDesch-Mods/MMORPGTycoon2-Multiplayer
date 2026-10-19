#pragma once
#include "selected_snapshot.h"
#include <cstddef>
#include <cstring>

namespace mt2mp::game::save_companion_retirement {
namespace fs=std::filesystem;
using Json=nlohmann::json;
inline void require(bool value,const std::string& error){if(!value)throw std::runtime_error(error);}
inline void require_win32(bool value,const char* operation){if(!value)throw std::runtime_error(std::string(operation)+" Win32="+std::to_string(GetLastError()));}

// Native autosave has no Multiplayer alias observer. Keep the exact old pair
// before that mutable basename is written, so its aliases cannot label new bytes.
inline Json archive(const fs::path& write,const std::string& world,const fs::path& archive_root){
    require(write.is_absolute()&&archive_root.is_absolute()&&net::snapshot_world_path::valid_folder(world),
            "autosave companion retirement requires absolute owned roots and an exact world folder");
    const auto input=write/L"save"/net::snapshot_world_path::utf8_path(world)/L"autosave.vrz";
    const auto companion=fs::path(input.wstring()+L".monsters.json");
    private_profile::DirectoryPins pins;pins.verify(input.parent_path());
    if(!selected_snapshot::regular(companion,false))return {{"state","no_companion"},{"native_path",private_profile::utf8(input)}};
    require(!private_profile::equal(archive_root,input.parent_path())&&!private_profile::descendant(archive_root,input.parent_path()),
            "autosave companion archive must be outside the native world folder");
    auto meta=input;meta.replace_extension(L".meta.vrz");
    struct File {fs::path source;bool present=false;std::string sha;uint64_t bytes=0;};
    std::array<File,2> files{{{input},{meta}}};
    std::array<net::SnapshotFileHandle,2> source_pins;
    std::string error;
    for(size_t i=0;i<files.size();++i){auto& file=files[i];
        file.present=selected_snapshot::regular(file.source,i==0);if(!file.present)continue;
        require(source_pins[i].open(file.source,GENERIC_READ,OPEN_EXISTING,error),error);
        require(private_profile::equal(private_profile::final_path(source_pins[i].get()),file.source),
                "autosave native/meta pinned path differs");
        require(net::hash_snapshot_file(file.source,file.sha,file.bytes,error),error);
    }
    // DELETE access lets this exact pinned file be renamed without a close/reopen
    // race. Denying write/delete sharing prevents substitution during validation.
    net::SnapshotFileHandle companion_pin;
    require(companion_pin.open(companion,GENERIC_READ|DELETE,OPEN_EXISTING,error),error);
    require(private_profile::equal(private_profile::final_path(companion_pin.get()),companion),
            "autosave companion pinned path differs");
    uint64_t companion_bytes=0;
    require(net::snapshot_file_length(companion_pin.get(),companion_bytes,error),error);
    require(companion_bytes<=monster_snapshot_companion::maximum_bytes,"autosave companion byte budget exceeded");
    std::string data(size_t(companion_bytes),'\0');DWORD read=0;
    require_win32(ReadFile(companion_pin.get(),data.data(),DWORD(data.size()),&read,nullptr)!=0,"autosave companion pinned read failed");
    require(read==data.size(),"autosave companion pinned read incomplete");
    monster_snapshot_companion::Manifest manifest;
    if(!monster_snapshot_companion::decode(data,files[0].sha,files[0].bytes,manifest,error))
        throw std::runtime_error("autosave companion retirement refused: "+error);
    net::SnapshotSha256 hasher;hasher.add(reinterpret_cast<const uint8_t*>(data.data()),data.size());
    const auto companion_sha=hasher.finish();
    pins.verify(archive_root,true);
    const auto destination=archive_root/net::snapshot_world_path::utf8_path(private_profile::new_run_id());
    require_win32(CreateDirectoryW(destination.c_str(),nullptr)!=0,"autosave companion archive creation failed");
    pins.verify(destination);
    Json receipt={{"state","archived_companion_before_native_autosave"},{"native_path",private_profile::utf8(input)},
                  {"archive",private_profile::utf8(destination)},{"files",Json::array()}};
    for(const auto& file:files){
        const auto target=destination/file.source.filename();
        if(file.present){
            require(fs::copy_file(file.source,target,fs::copy_options::none),"autosave companion archive exact copy failed");
            std::string actual;uint64_t bytes=0;
            require(net::hash_snapshot_file(target,actual,bytes,error),error);
            require(actual==file.sha&&bytes==file.bytes,"autosave companion archive copy SHA/length differs");
        }
        receipt["files"].push_back({{"name",file.source.filename().string()},{"present",file.present},{"sha256",file.sha},{"bytes",file.bytes}});
    }
    const auto retired=destination/companion.filename();const auto name=retired.wstring();
    const auto name_bytes=name.size()*sizeof(wchar_t);
    const auto rename_bytes=offsetof(FILE_RENAME_INFO,FileName)+name_bytes;
    require(rename_bytes<=MAXDWORD,"autosave companion archive rename path exceeds Win32 byte budget");
    std::vector<uint64_t> storage((rename_bytes+sizeof(uint64_t)-1)/sizeof(uint64_t));
    auto* rename=reinterpret_cast<FILE_RENAME_INFO*>(storage.data());
    rename->ReplaceIfExists=FALSE;rename->RootDirectory=nullptr;rename->FileNameLength=DWORD(name_bytes);
    std::memcpy(rename->FileName,name.data(),name_bytes);
    require_win32(SetFileInformationByHandle(companion_pin.get(),FileRenameInfo,rename,DWORD(rename_bytes))!=0,
                  "autosave companion pinned retirement failed");
    require(private_profile::equal(private_profile::final_path(companion_pin.get()),retired)&&!selected_snapshot::regular(companion,false),
            "autosave companion retirement path acknowledgement differs");
    receipt["files"].push_back({{"name",companion.filename().string()},{"present",true},{"sha256",companion_sha},{"bytes",companion_bytes}});
    std::ofstream output(destination/L"receipt.json",std::ios::binary|std::ios::out);output<<receipt.dump(2)<<'\n';output.close();
    require(bool(output),"autosave companion archive receipt write failed");
    require(companion_pin.close(&error),error);
    for(auto& pin:source_pins)require(pin.close(&error),error);
    return receipt;
}
}
