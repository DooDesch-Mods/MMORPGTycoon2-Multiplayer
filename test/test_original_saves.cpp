#include "mt2mp/core/original_saves.h"
#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>
#include <vector>

namespace fs = std::filesystem;
namespace saves = mt2mp::original_saves;
using mt2mp::private_profile::utf8;
struct Fixture {
    fs::path owner, appdata, data, account, source;
    std::string write;
    std::vector<std::pair<std::string, std::string>> volumes;
    std::vector<char*> listing;
    unsigned private_calls=0, source_calls=0, unmount_calls=0, write_reads=0, free_calls=0;
    bool fail_private=false, fail_source=false, fail_unmount=false, fail_list=false;
    bool ignore_source=false, change_write=false, wrong_point=false;
};
Fixture f;
std::atomic<bool> stopping{false}, log_root_ready{true};
fs::path data_dir, original_save_proof_root, original_save_appdata;
saves::Native original_save_io;
saves::State original_save_state;
std::mutex original_save_gate;
std::string original_save_message;
std::vector<std::pair<std::string,std::string>> messages;
void record(const char* level, const char* detail) { messages.emplace_back(level,detail); }
const char* write_dir() { ++f.write_reads; return f.write.c_str(); }
int mount(const char* path, const char* point, int append) {
    const std::string name(path ? path : ""), target(point ? point : "");
    const bool is_private=name==f.write;
    const bool is_source=name==utf8(f.source);
    if(is_private)++f.private_calls;
    if(is_source)++f.source_calls;
    if((is_private&&f.fail_private)||(is_source&&f.fail_source))return 0;
    if(is_source&&f.change_write)f.write=utf8(f.data/L"76561198000000009");
    if(is_source&&f.ignore_source)return 7;
    for(const auto& volume:f.volumes)if(volume.first==name)return 7;
    const auto entry=std::make_pair(name,is_source&&f.wrong_point?"wrong/":target+"/");
    if(append)f.volumes.push_back(entry);else f.volumes.insert(f.volumes.begin(),entry);
    return 7;
}
int unmount(const char* path) {
    ++f.unmount_calls;if(f.fail_unmount)return 0;
    for(auto it=f.volumes.begin();it!=f.volumes.end();++it)if(it->first==path){f.volumes.erase(it);return 1;}
    return 0;
}
const char* mount_point(const char* path) {
    for(const auto& volume:f.volumes)if(volume.first==path)return volume.second.c_str();
    return nullptr;
}
char** search_path() {
    if(f.fail_list)return nullptr;
    f.listing.clear();for(auto& volume:f.volumes)f.listing.push_back(volume.first.data());
    f.listing.push_back(nullptr);return f.listing.data();
}
void free_list(void* list) { if(list!=f.listing.data())throw std::runtime_error("foreign list free");++f.free_calls; }
int error_code() { return 42; }
const char* error_text(int code) { return code==42?"fixture original failure":"wrong original code"; }
#include "loader_mount_body.inc"
unsigned cases=0;
void require(bool value,const char* why) { if(!value)throw std::runtime_error(why); }
void reset(const fs::path& owner) {
    f=Fixture{};f.owner=owner;f.appdata=owner/L"roaming";f.data=owner/L"private/data";
    f.account=f.data/L"76561198000000001";
    f.source=f.appdata/L"VectorStorm/MMORPG Tycoon 2/76561198000000001/save";
    fs::create_directories(f.account);fs::create_directories(f.source);
    f.appdata=mt2mp::private_profile::canonical_file(f.appdata);f.data=mt2mp::private_profile::canonical_file(f.data);
    f.account=mt2mp::private_profile::canonical_file(f.account);f.source=mt2mp::private_profile::canonical_file(f.source);
    f.write=utf8(f.account);
    data_dir=f.data;original_save_appdata=f.appdata;original_save_proof_root.clear();
    original_save_state={};original_save_message.clear();messages.clear();stopping=false;log_root_ready=true;
    original_save_io={write_dir,mount,unmount,mount_point,search_path,free_list,error_code,error_text};
}
int user_mount() { const auto value=f.write; return private_mount(value.c_str(),"user",1); }
bool error_has(const char* text) {
    return !messages.empty()&&messages.back().first=="error"&&messages.back().second.find(text)!=std::string::npos;
}
int wmain(int argc,wchar_t** argv) {
    try {
        require(argc==2,"fixture output directory required");
        const auto owner=fs::absolute(argv[1])/std::to_wstring(GetCurrentProcessId());
        require(!fs::exists(owner),"refuse to reuse previous fixture directory");fs::create_directories(owner);
        const auto run=[&](const char* name,const auto& test){reset(owner/std::to_wstring(++cases));test();std::cout<<"PASS "<<name<<'\n';};
        run("native private mount forwards once and exposes originals",[]{
            std::ofstream original(f.source/L"manual.vrz",std::ios::binary);original<<"original fixture";original.close();
            require(user_mount()==7,"original native return changed");
            require(f.private_calls==1&&f.source_calls==1&&f.unmount_calls==0,"original or source mount count differs");
            require(f.volumes.size()==2&&f.volumes[0].first==f.write&&f.volumes[1].first==utf8(f.source),"private priority differs");
            require(original_save_state.owned_source==utf8(f.source)&&messages.back().first=="info","visible mount not acknowledged");
            std::ifstream read(f.source/L"manual.vrz",std::ios::binary);std::string contents;std::getline(read,contents);
            require(contents=="original fixture"&&f.write==utf8(f.account),"original file or private write path changed");
        });
        run("duplicate user mounts do not duplicate source",[]{user_mount();user_mount();require(f.private_calls==2&&f.source_calls==1&&messages.size()==1,"duplicate mount or duplicate log");});
        run("foreign mount bypasses source policy",[]{require(private_mount("C:/foreign","base",0)==7,"foreign return changed");require(f.write_reads==0&&f.source_calls==0,"foreign mount inspected profile");});
        run("unprepared loader forwards only",[]{log_root_ready=false;user_mount();require(f.write_reads==0&&f.source_calls==0,"unprepared profile inspected");});
        run("shutdown forwards only",[]{stopping=true;user_mount();require(f.write_reads==0&&f.source_calls==0,"shutdown source work");});
        run("native mount failure stays zero without source effects",[]{f.fail_private=true;require(user_mount()==0,"native failure hidden");require(f.private_calls==1&&!f.source_calls&&!f.write_reads,"failed native mount triggered helper");});
        run("owned original survives engine private-volume remount",[]{
            user_mount();require(unmount(f.write.c_str())==1,"fixture removal failed");f.unmount_calls=0;
            require(user_mount()==7&&f.source_calls==2&&f.unmount_calls==1,"owned reorder not exact once");
            require(f.volumes[0].first==f.write,"original source hides private saves after remount");
        });
        run("foreign wrong mountpoint remains untouched",[]{f.volumes.emplace_back(utf8(f.source),"other/");user_mount();require(error_has("existing source mount point differs")&&!f.source_calls&&!f.unmount_calls,"foreign mount changed or accepted");});
        run("foreign higher-priority source is rejected",[]{f.volumes.emplace_back(utf8(f.source),"user/save/");user_mount();require(error_has("foreign mount precedes private")&&!f.unmount_calls,"foreign priority accepted or changed");});
        run("foreign correct lower-priority source is not claimed",[]{mount(f.write.c_str(),"user",1);mount(utf8(f.source).c_str(),"user/save",1);f.source_calls=0;user_mount();require(!f.source_calls&&original_save_state.owned_source.empty()&&!error_has("foreign"),"existing valid source remounted or claimed");});
        run("missing original directory is expected and not created",[]{original_save_appdata=f.owner/L"absent";user_mount();require(!fs::exists(original_save_appdata)&&!f.source_calls&&messages.back().first=="info","missing originals created or failed");});
        run("proof defaults never read normal APPDATA",[]{original_save_proof_root=f.owner/L"proof";original_save_appdata=L"C:/Users/not-the-fixture/AppData/Roaming";user_mount();require(!f.source_calls&&messages.back().second.find("outside proof root")!=std::string::npos,"proof did not exclude real originals");});
        run("proof fixture APPDATA is accepted",[]{original_save_proof_root=f.owner;user_mount();require(f.source_calls==1&&messages.back().first=="info","owned original fixture unavailable");});
        run("new account without saves cannot retain old originals",[]{
            user_mount();const auto old_source=utf8(f.source);
            f.account=f.data/L"76561198000000002";fs::create_directories(f.account);f.write=utf8(f.account);
            user_mount();require(!mount_point(old_source.c_str())&&original_save_state.owned_source.empty()&&f.unmount_calls==1,"old account originals retained");
            require(messages.back().second.find("absent for current Steam account")!=std::string::npos,"missing new account saves not explicit");
        });
        run("changed old account mount is not unmounted as own",[]{
            user_mount();f.volumes.back().second="foreign/";
            f.account=f.data/L"76561198000000002";fs::create_directories(f.account);f.write=utf8(f.account);
            user_mount();require(error_has("previous account mount ownership changed")&&!f.unmount_calls,"changed mount ownership ignored");
        });
        run("missing APPDATA error remains visible",[]{original_save_appdata.clear();user_mount();require(error_has("APPDATA is missing")&&!f.source_calls,"missing APPDATA hidden");});
        run("relative APPDATA rejected",[]{original_save_appdata=L"relative";user_mount();require(error_has("APPDATA is missing")&&!f.source_calls,"relative APPDATA accepted");});
        run("non-account private path is ignored",[]{f.write=utf8(f.data/L"not-a-steamid");user_mount();require(!f.source_calls&&messages.empty(),"non-account volume exposed originals");});
        run("nested foreign user volume is ignored",[]{const auto nested=utf8(f.account/L"nested");private_mount(nested.c_str(),"user",1);require(!f.source_calls&&messages.empty(),"foreign user path exposed originals");});
        run("source mount original cause retained",[]{f.fail_source=true;require(user_mount()==7,"successful native private mount changed");require(error_has("read mount failed code=42 cause=fixture original failure"),"source failure original cause lost");});
        run("successful mount return without membership rejected",[]{f.ignore_source=true;user_mount();require(error_has("source absent from search path"),"success return substituted for membership");});
        run("source mountpoint readback is mandatory",[]{f.wrong_point=true;user_mount();require(error_has("mounted source readback differs"),"wrong successful mountpoint accepted");});
        run("search path native error retained",[]{f.fail_list=true;user_mount();require(error_has("search path read failed code=42 cause=fixture original failure"),"search path failure cause lost");});
        run("reorder unmount native error retained",[]{user_mount();unmount(f.write.c_str());f.fail_unmount=true;user_mount();require(error_has("reorder unmount failed code=42 cause=fixture original failure"),"reorder failure hidden");});
        run("read mount must retain actual private write root",[]{f.change_write=true;user_mount();require(error_has("write directory changed"),"write root change accepted");});
        run("error logging is bounded and recovers",[]{f.fail_source=true;user_mount();user_mount();require(messages.size()==1,"identical failure spammed");f.fail_source=false;user_mount();require(messages.size()==2&&messages.back().first=="info","source recovery not logged");});
        run("all source search lists retired",[]{user_mount();require(f.free_calls==1,"search list not freed once");user_mount();require(f.free_calls==2,"duplicate check list not freed");});
        std::cout<<"PASS "<<cases<<" actual-helper and extracted actual-loader scenarios; no game or real AppData access\n";return 0;
    } catch(const std::exception& error) {
        std::cerr<<"error: case="<<cases<<" cause="<<error.what()<<" private_calls="<<f.private_calls<<" source_calls="<<f.source_calls<<'\n';
        for(const auto& message:messages)std::cerr<<message.first<<": "<<message.second<<'\n';
        return 1;
    }
}
