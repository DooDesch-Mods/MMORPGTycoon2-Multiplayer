#include "mt2mp/net/snapshot_world_path.h"
#include <iostream>
#include <stdexcept>
#include <vector>

namespace paths=mt2mp::net::snapshot_world_path;
const std::string uuid="50000000-0000-4000-8000-000000000001";
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
template<class Call> void rejected(Call call,const char* reason){try{call();}catch(const std::runtime_error&){return;}throw std::runtime_error(reason);}
int main()try{
    const std::string name="World of Stories";
    check(paths::from_manifest({{"world_name",name}})==name,"manifest changed native world folder");
    check(paths::from_manifest(nlohmann::json::object())=="MPTestSession","legacy manifest fallback changed");
    check(paths::capture(name,uuid)=="user/save/World of Stories/snapshot-"+uuid+".vrz","capture virtual folder differs");
    check(paths::load(name,uuid)=="save/World of Stories/snapshot-"+uuid+".vrz","load virtual folder differs");
    const auto write=std::filesystem::absolute("owned-profile");
    const auto expected=write/"save"/name/("snapshot-"+uuid+".vrz");
    check(paths::physical(write,name,uuid)==expected,"physical file escaped original world folder");
    const std::string unicode="Welt der Wälder";
    check(paths::valid_folder(unicode),"valid UTF-8 native folder rejected");
    check(paths::physical(write,unicode,uuid).parent_path().filename()==paths::utf8_path(unicode),"UTF-8 folder changed");
    check(paths::utf8(paths::physical(write,unicode,uuid)).find(unicode)!=std::string::npos,"UTF-8 receipt changed folder");
    for(const auto& bad:std::vector<std::string>{"",".","..","../private","a/b","a\\b","C:owned","/absolute","\\\\server\\share","world\"name","x\ny","x\ry","x\ty",std::string("x\0y",3),"name.","name ","CON","nul.vrz","LPT9","COM1.txt",std::string(201,'x')}){
        check(!paths::valid_folder(bad),"unsafe folder accepted");
        rejected([&]{paths::from_manifest({{"world_name",bad}});},"unsafe untrusted manifest accepted");
        rejected([&]{paths::physical(write,bad,uuid);},"unsafe physical snapshot path accepted");
    }
    rejected([]{paths::from_manifest({{"world_name",7}});},"numeric world name accepted");
    rejected([]{paths::from_manifest({{"world_name",nullptr}});},"null world name accepted");
    rejected([&]{paths::load(name,"../bad");},"unsafe UUID accepted");
    check(paths::valid_folder(std::string(200,'x')),"supported native folder length rejected");
    std::cout<<"snapshot world paths: passed original folder, UTF-8, legacy fallback and invalid manifests\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
