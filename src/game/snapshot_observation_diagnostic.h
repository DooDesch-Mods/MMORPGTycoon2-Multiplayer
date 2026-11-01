#pragma once
#include "mt2mp/core/hash.h"
#include "vendor/nlohmann/json.hpp"
#include <filesystem>
#include <fstream>

namespace mt2mp::game::snapshot_observation_diagnostic {
using Json=nlohmann::json;
inline Json write(const std::filesystem::path& directory,const Json& owner,const Json& projection){
    Json receipt=owner;
    receipt["schema"]="mt2-save-covered-artifact-v1";receipt["path"]="";receipt["bytes"]=0;
    receipt["written"]=false;receipt["error"]="";
    try{
        const auto nonce=owner.at("nonce").get<std::string>();
        if(nonce.size()!=36||nonce.find_first_not_of("0123456789abcdef-")!=std::string::npos||
           !directory.is_absolute()||!owner.at("session").get<uint64_t>()||!owner.at("peer").get<uint64_t>())
            throw std::runtime_error("Save covered diagnostic owner/path is invalid");
        const auto bytes=projection.dump();
        if(bytes.size()>ULONG_MAX)throw std::runtime_error("Save covered diagnostic exceeds hash capacity");
        const auto sha=mt2mp::sha256({bytes.begin(),bytes.end()});
        if(sha!=owner.at("sha256").get<std::string>())throw std::runtime_error("Save covered diagnostic differs from sealed witness");
        std::filesystem::create_directories(directory);
        const auto path=directory/("save-world-"+nonce+"-"+std::to_string(owner.at("session").get<uint64_t>())+"-"+
            std::to_string(owner.at("peer").get<uint64_t>())+".json");
        if(std::filesystem::exists(path))throw std::runtime_error("Save covered diagnostic already exists; no overwrite");
        std::ofstream stream(path,std::ios::binary|std::ios::trunc);
        stream.write(bytes.data(),std::streamsize(bytes.size()));stream.close();
        if(!stream)throw std::runtime_error("Save covered diagnostic write failed");
        std::ifstream readback(path,std::ios::binary);const std::string actual{std::istreambuf_iterator<char>(readback),{}};
        if(readback.bad()||actual!=bytes)throw std::runtime_error("Save covered diagnostic readback differs");
        const auto utf8=path.u8string();receipt["path"]=std::string(utf8.begin(),utf8.end());
        receipt["bytes"]=bytes.size();receipt["written"]=true;
    }catch(const std::exception& cause){receipt["error"]=cause.what();}
    return receipt;
}
}
