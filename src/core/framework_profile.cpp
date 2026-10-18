#include "mt2mp/core/framework_profile.h"
#include "mt2mp/core/private_profile_paths.h"
#include "mt2mp/net/snapshot_buffer.h"
#include <array>
#include <limits>

namespace mt2mp::framework_profile {
using json=nlohmann::json;
namespace fs=std::filesystem;
namespace {
fs::path borrowed_path(const char* input) {
    if(!input)throw std::runtime_error("Framework path is null");
    size_t length=0;
    while(length<32768&&input[length])++length;
    if(!length||length==32768)throw std::runtime_error("Framework path length invalid");
    const auto path=fs::path(private_profile::wide_utf8(input));
    if(!path.is_absolute())throw std::runtime_error("Framework path must be absolute");
    for(const auto& part:path)if(part==L".."||part==L".")throw std::runtime_error("Framework path contains traversal");
    if(!private_profile::equal(private_profile::canonical_file(path),path))
        throw std::runtime_error("Framework path resolves outside its declared location");
    return path.lexically_normal();
}
bool lower_hex(const std::string& value,size_t length) {
    if(value.size()!=length)return false;
    for(const char c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
}
bool uuid(const std::string& value) {
    if(value.size()!=36)return false;
    std::string digits;
    for(size_t i=0;i<value.size();++i){
        if(i==8||i==13||i==18||i==23){if(value[i]!='-')return false;}
        else digits+=value[i];
    }
    return lower_hex(digits,32);
}
uint64_t positive_integer(const json& value,const char* field,uint64_t maximum) {
    if(!value.is_number_integer()||value.is_boolean()||(value.is_number_integer()&&!value.is_number_unsigned()&&value.get<int64_t>()<1))
        throw std::runtime_error(std::string("Framework local test integer invalid: ")+field);
    const auto result=value.get<uint64_t>();
    if(!result||result>maximum)throw std::runtime_error(std::string("Framework local test integer out of range: ")+field);
    return result;
}
}
Paths paths(const MT2FW_MT2_ServiceV1& api) {
    if(api.size<sizeof(api)||api.version!=MT2FW_MT2_SERVICE_VERSION||!api.context)
        throw std::runtime_error("Framework native service identity invalid");
    Paths p{borrowed_path(api.game_directory),borrowed_path(api.profile_directory),borrowed_path(api.game_data_directory),
        borrowed_path(api.mod_directory),borrowed_path(api.mod_data_directory),{}, {}};
    p.runtime=p.profile.parent_path();p.diagnostics=p.profile/L"diagnostics/multiplayer";
    // The Framework service admits either isolated or original Steam data.
    if(!private_profile::equal(p.mod,p.profile/L"mods"/plugin_id)||
       !private_profile::equal(p.data,p.profile/L"data"/plugin_id)||
       private_profile::equal(p.game,p.profile)||private_profile::equal(p.game,p.game_data)||
       private_profile::equal(p.profile,p.game_data))
        throw std::runtime_error("Framework path ownership differs");
    private_profile::DirectoryPins pins;
    for(const auto& directory:{p.game,p.profile,p.game_data,p.mod,p.data})pins.verify(directory);
    return p;
}
LocalTest local_test(const Paths& p,const json& value) {
    if(!value.is_object()||value.size()!=9||!value.at("schema_version").is_number_integer()||value.at("schema_version")!=1)
        throw std::runtime_error("Framework local test configuration schema differs");
    if(!private_profile::equal(p.game_data,p.profile/L"game-data"))
        throw std::runtime_error("Framework local test requires isolated game data");
    LocalTest result;
    result.run_id=value.at("run_id").get<std::string>();
    if(!uuid(result.run_id)||value.at("owner")!=json{{"kind","MPTest"},{"run_id",result.run_id}})
        throw std::runtime_error("Framework local test owner differs");
    if(!private_profile::equal(p.profile,p.runtime/L"profile")||!private_profile::equal(p.game,p.runtime/L"game"))
        throw std::runtime_error("Framework local test requires exact sibling game/profile layout");
    const auto root=borrowed_path(value.at("runtime_root").get_ref<const std::string&>().c_str());
    if(!private_profile::equal(root,p.runtime))throw std::runtime_error("Framework local test runtime root differs");
    result.peer=positive_integer(value.at("peer_id"),"peer_id",4);
    result.session=positive_integer(value.at("session_id"),"session_id",std::numeric_limits<uint64_t>::max());
    result.control_port=static_cast<uint16_t>(positive_integer(value.at("control_port"),"control_port",65535));
    result.token=value.at("session_token").get<std::string>();
    if(!lower_hex(result.token,32))throw std::runtime_error("Framework local test session token must contain32 lowercase hex bytes");
    const auto transport=value.at("transport").get<std::string>();
    if(transport!="loopback"&&transport!="steam")throw std::runtime_error("Framework local test transport differs");
    result.loopback=transport=="loopback";
    return result;
}
void configure(Config& cfg,const Paths& p,const LocalTest* test) {
    cfg.game_dir=private_profile::utf8(p.game);cfg.user_data_dir=private_profile::utf8(p.game_data);
    cfg.original_steam_dll=private_profile::utf8(p.game/L"steam_api64.dll");
    cfg.log_file=private_profile::utf8(p.data/L"mt2mp.log");
    cfg.installed_profile_verified=false;cfg.framework_profile_verified=true;
    cfg.framework_profile_directory=private_profile::utf8(p.profile);
    cfg.runtime_root=private_profile::utf8(test?p.runtime:p.data);
    cfg.run_id=test?test->run_id:private_profile::new_run_id();
    cfg.diagnostic_mailbox_enabled=test!=nullptr;cfg.local_test=test&&test->loopback;
    cfg.peer_id=test?test->peer:0;cfg.session_id=test?test->session:0;cfg.is_host=test&&test->peer==1;
    cfg.session_token=test?test->token:std::string{};cfg.sidecar_control_port=test?test->control_port:47001;
}
json module_receipt(const Paths& p,HMODULE plugin) {
    const std::map<std::string,std::pair<fs::path,HMODULE>> modules={
        {"MT2.exe",{p.game/L"MT2.exe",GetModuleHandleW(nullptr)}},
        {"steam_api64.dll",{p.game/L"steam_api64.dll",GetModuleHandleW((p.game/L"steam_api64.dll").c_str())}},
        {"WSOCK32.dll",{p.game/L"WSOCK32.dll",GetModuleHandleW((p.game/L"WSOCK32.dll").c_str())}},
        {"MT2Framework.dll",{p.game/L"MT2Framework.dll",GetModuleHandleW((p.game/L"MT2Framework.dll").c_str())}},
        {"MT2MultiplayerPlugin.dll",{p.mod/L"MT2MultiplayerPlugin.dll",plugin}}};
    json result=json::object();
    for(const auto& [name,entry]:modules){
        if(!entry.second)throw std::runtime_error("Framework loaded module missing: "+name);
        const auto actual=private_profile::module_file(entry.second);
        if(!private_profile::equal(actual,entry.first))throw std::runtime_error("Framework loaded module path differs: "+name);
        std::string digest,error;uint64_t bytes=0;
        if(!net::hash_snapshot_file(actual,digest,bytes,error)||!bytes)
            throw std::runtime_error("Framework module hash failed: "+name+" cause="+error);
        result[name]=digest;
    }
    return result;
}
json instance_receipt(const Paths& p,const LocalTest& test,const json& modules) {
    if(modules.size()!=5)throw std::runtime_error("Framework instance requires exact five-module receipt");
    for(const char* name:{"MT2.exe","steam_api64.dll","WSOCK32.dll","MT2Framework.dll","MT2MultiplayerPlugin.dll"})
        if(!modules.contains(name)||!modules.at(name).is_string()||!lower_hex(modules.at(name).get<std::string>(),64))
            throw std::runtime_error(std::string("Framework instance module digest invalid: ")+name);
    FILETIME created{},exited{},kernel{},user{};
    if(!GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user))
        throw std::runtime_error("Framework process start query failed Win32="+std::to_string(GetLastError()));
    return {{"schema_version",2},{"runtime_kind","framework-plugin"},{"run_id",test.run_id},
        {"owner",{{"kind","MPTest"},{"run_id",test.run_id}}},{"pid",GetCurrentProcessId()},
        {"process_start_time",std::to_string((uint64_t(created.dwHighDateTime)<<32)|created.dwLowDateTime)},
        {"runtime_root",private_profile::utf8(p.runtime)},{"framework_profile",private_profile::utf8(p.profile)},
        {"executable_path",private_profile::utf8(p.game/L"MT2.exe")},{"installed_binary_sha256",modules},
        {"role","game"},{"peer_id",test.peer}};
}
}
