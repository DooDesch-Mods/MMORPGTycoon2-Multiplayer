#include "mt2mp/core/original_saves.h"
#include <fstream>
#include <iostream>
#include <map>

namespace fs=std::filesystem;
namespace saves=mt2mp::original_saves;
using mt2mp::private_profile::utf8;
fs::path appdata,data,write_path,source;
std::string write_text,source_text;
std::map<std::string,std::string> mounts;
std::vector<std::string> order;
std::vector<char*> listing;
unsigned mounted=0;
void require(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
const char* get_write(){return write_text.c_str();}
int mount_source(const char* path,const char* point,int append){
    require(append==1,"source was not appended");
    require(fs::is_directory(write_path/L"save/World of Stories"),"source became visible before private write parent existed");
    mounts[path]=std::string(point)+"/";order.push_back(path);++mounted;return 1;
}
int unmount(const char*){throw std::runtime_error("unexpected unmount");}
const char* mount_point(const char* path){const auto it=mounts.find(path);return it==mounts.end()?nullptr:it->second.c_str();}
char** search(){listing.clear();for(auto& entry:order)listing.push_back(entry.data());listing.push_back(nullptr);return listing.data();}
void free_list(void* value){require(value==listing.data(),"wrong native list retired");}
int error_code(){return 42;}
const char* error_text(int){return "fixture cause";}
const saves::Native io{get_write,mount_source,unmount,mount_point,search,free_list,error_code,error_text};
std::string contents(const fs::path& file){std::ifstream stream(file,std::ios::binary);return {std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>()};}
void reset(const fs::path& base){
    appdata=base/L"original";data=base/L"private/data";write_path=data/L"76561198000000001";
    source=appdata/L"VectorStorm/MMORPG Tycoon 2/76561198000000001/save";
    fs::create_directories(write_path);fs::create_directories(source/L"World of Stories");
    appdata=mt2mp::private_profile::canonical_file(appdata);data=mt2mp::private_profile::canonical_file(data);
    write_path=mt2mp::private_profile::canonical_file(write_path);source=mt2mp::private_profile::canonical_file(source);
    write_text=utf8(write_path);source_text=utf8(source);mounts={{write_text,"user/"}};order={write_text};mounted=0;
    std::ofstream original(source/L"World of Stories/autosave.vrz",std::ios::binary);original<<"retained original bytes";
}
void maintain(){saves::State state;std::string error;require(saves::maintain(state,io,data,{},appdata,write_text.c_str(),"user",error)==saves::Result::Visible,error.c_str());}
int wmain(int argc,wchar_t** argv){
    try{
        require(argc==2,"owned fixture root required");const auto owner=fs::absolute(argv[1])/mt2mp::private_profile::wide_utf8(mt2mp::private_profile::new_run_id().c_str());
        require(!fs::exists(owner),"refuse previous fixture reuse");
        reset(owner/L"fresh");
        require(!fs::exists(write_path/L"save/World of Stories"),"missing-parent fixture is not missing");
        maintain();require(mounted==1,"source mount did not occur once");
        const auto temporary=write_path/L"tmp/save/World of Stories/rules.vrt.1";
        fs::create_directories(temporary.parent_path());{std::ofstream rules(temporary);rules<<"native-style generated rules";}
        fs::rename(temporary,write_path/L"save/World of Stories/rules.vrt");
        require(contents(write_path/L"save/World of Stories/rules.vrt")=="native-style generated rules","private cache rename failed");
        require(!fs::exists(write_path/L"save/World of Stories/autosave.vrz"),"original save was copied");
        require(!fs::exists(source/L"World of Stories/rules.vrt")&&contents(source/L"World of Stories/autosave.vrz")=="retained original bytes","original files changed");
        std::cout<<"PASS missing private world parent exists before mount; actual private tmp-to-rules rename succeeds; no original file copies\n";
        reset(owner/L"existing");fs::create_directories(write_path/L"save/World of Stories");
        {std::ofstream existing(write_path/L"save/World of Stories/rules.vrt");existing<<"keep private rules";}
        fs::create_directories(source/L"Überwald");maintain();
        require(contents(write_path/L"save/World of Stories/rules.vrt")=="keep private rules"&&fs::is_directory(write_path/L"save/Überwald"),"existing private data changed or another world parent missed");
        std::cout<<"PASS existing private files retained and multiple original world directories seeded\n";
        reset(owner/L"collision");fs::create_directories(write_path/L"save");
        {std::ofstream collision(write_path/L"save/World of Stories");collision<<"retain collision";}
        bool rejected=false;try{maintain();}catch(const std::exception&){rejected=true;}
        require(rejected&&mounted==0&&contents(write_path/L"save/World of Stories")=="retain collision","private parent collision was hidden, overwritten or exposed");
        require(contents(source/L"World of Stories/autosave.vrz")=="retained original bytes","failed seed changed original");
        std::cout<<"PASS non-directory private parent rejects before original read mount without overwrite\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<"error: private world parent: "<<error.what()<<'\n';return 1;}
}
