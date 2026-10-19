#pragma once
#include "monster_snapshot_companion.h"
#include "mt2mp/core/private_profile_paths.h"
#include "mt2mp/net/snapshot_world_path.h"
#include "mt2mp/net/snapshot_buffer.h"
#include <fstream>

namespace mt2mp::game::selected_snapshot {
namespace fs=std::filesystem;
using Json=nlohmann::json;
inline bool regular(const fs::path& file,bool required) {
    const auto attributes=GetFileAttributesW(file.c_str());
    if(attributes==INVALID_FILE_ATTRIBUTES){
        const auto cause=GetLastError();
        if(!required&&(cause==ERROR_FILE_NOT_FOUND||cause==ERROR_PATH_NOT_FOUND))return false;
        throw std::runtime_error("selected snapshot file unavailable Win32="+std::to_string(cause));
    }
    if(attributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))
        throw std::runtime_error("selected snapshot reparse or non-file refused");
    return true;
}
inline std::pair<std::string,std::string> source(const std::string& selected) {
    if(!selected.starts_with("save/"))throw std::runtime_error("selected snapshot must be in save/world/file.vrz");
    const auto split=selected.find('/',5);
    if(split==std::string::npos)throw std::runtime_error("selected snapshot world/file absent");
    auto world=selected.substr(5,split-5),name=selected.substr(split+1);
    if(!net::snapshot_world_path::valid_folder(world)||!net::snapshot_world_path::valid_folder(name)||!name.ends_with(".vrz")||name.ends_with(".meta.vrz"))
        throw std::runtime_error("selected snapshot world/file component invalid");
    return {world,name};
}
template<class Hash>Json copy(const fs::path& write,const std::string& selected,const std::string& id,uint64_t maximum,Hash&& hash) {
    const auto [world,name]=source(selected);net::snapshot_world_path::require(world,id);
    const auto input=write/"save"/net::snapshot_world_path::utf8_path(world)/net::snapshot_world_path::utf8_path(name);
    const auto output=net::snapshot_world_path::physical(write,world,id);
    private_profile::DirectoryPins pins;pins.verify(input.parent_path());
    if(!write.is_absolute()||fs::weakly_canonical(input)==fs::weakly_canonical(output))
        throw std::runtime_error("selected snapshot source must differ from new owned snapshot");
    struct File {fs::path input,output;bool present=false;std::string sha;uint64_t bytes=0;};
    auto meta_in=input;meta_in.replace_extension(".meta.vrz");auto meta_out=output;meta_out.replace_extension(".meta.vrz");
    std::array<File,3> files{{{input,output},{meta_in,meta_out},{fs::path(input.wstring()+L".monsters.json"),fs::path(output.wstring()+L".monsters.json")}}};
    std::array<net::SnapshotFileHandle,3> source_pins;
    std::string error;
    for(size_t i=0;i<files.size();++i){auto& f=files[i];
        if(regular(f.output,false))throw std::runtime_error("selected snapshot destination already exists; no overwrite");
        f.present=regular(f.input,i==0);if(!f.present)continue;
        if(!source_pins[i].open(f.input,GENERIC_READ,OPEN_EXISTING,error))throw std::runtime_error("selected snapshot source pin failed: "+error);
        if(!hash(f.input,f.sha,f.bytes,error))throw std::runtime_error("selected snapshot source hash failed: "+error);
        if(!f.bytes||f.bytes>maximum||(i==2&&f.bytes>monster_snapshot_companion::maximum_bytes))
            throw std::runtime_error("selected snapshot source exceeds existing byte contract");
    }
    if(files[2].present){
        std::ifstream stream(files[2].input,std::ios::binary);std::string text(size_t(files[2].bytes),'\0');
        stream.read(text.data(),std::streamsize(text.size()));monster_snapshot_companion::Manifest companion;
        if(!stream||!monster_snapshot_companion::decode(text,files[0].sha,files[0].bytes,companion,error))
            throw std::runtime_error("selected snapshot companion rejected: "+error);
    }
    // A new UUID is published only after all copied files and original inputs still match.
    for(auto& f:files)if(f.present&&!fs::copy_file(f.input,f.output,fs::copy_options::none))
        throw std::runtime_error("selected snapshot exact copy failed; load not attempted");
    for(const auto& f:files){
        if(regular(f.input,false)!=f.present)throw std::runtime_error("selected snapshot companion/meta presence changed during copy");
        if(!f.present)continue;
        for(const auto& path:{f.input,f.output}){std::string digest;uint64_t bytes=0;
            regular(path,true);
            if(!hash(path,digest,bytes,error))throw std::runtime_error("selected snapshot copy readback failed: "+error);
            if(digest!=f.sha||bytes!=f.bytes)throw std::runtime_error("selected snapshot source/copy SHA or bytes changed");
        }
    }
    for(auto& pin:source_pins)if(!pin.close(&error))throw std::runtime_error("selected snapshot source unpin failed: "+error);
    return {{"snapshot_id",id},{"world_name",world},{"path",net::snapshot_world_path::utf8(output)},
        {"bytes",files[0].bytes},{"sha256",files[0].sha},{"selected_file",selected},
        {"selected_meta_present",files[1].present},{"selected_meta_sha256",files[1].sha},
        {"selected_monster_companion_present",files[2].present},{"selected_monster_companion_sha256",files[2].sha}};
}
}
