#include <iostream>
#include <functional>
#include "mt2mp/core/framework_open_paths.h"
namespace mt2mp {Config& Config::instance(){static Config value;return value;}}
namespace f=mt2mp::framework_open_paths;
namespace p=mt2mp::private_profile;
uint64_t assertions=0,attributes_count=0;
std::vector<std::wstring> attribute_paths;
DWORD fixture_attributes=FILE_ATTRIBUTE_DIRECTORY,fixture_error=ERROR_SUCCESS;
DWORD WINAPI fake_attributes(LPCWSTR value){attribute_paths.emplace_back(value);SetLastError(fixture_error);return fixture_attributes;}
DWORD WINAPI actual_attributes(LPCWSTR value){++attributes_count;return GetFileAttributesW(value);}
void need(bool value,const char* cause){++assertions;if(!value)throw std::runtime_error(cause);}
void baseline(const f::Binding& binding,const f::fs::path& expected,const char* raw,f::AttributeReader& attributes){
    const auto actual=f::actual_write(binding,raw,attributes);
    if(expected.empty()||!p::equal(actual,expected))throw std::runtime_error("Framework native open actual write root changed during bound origin");
}
struct Outcome {std::string cause;std::vector<std::wstring> paths;bool operator==(const Outcome&)const=default;};
template<class F> Outcome outcome(F action){attribute_paths.clear();std::string cause;try{action();}catch(const std::exception& e){cause=e.what();}return {cause,attribute_paths};}
int main(int argc,char** argv){try{
    need(argc==1,"usage: test_framework_write_origin");
    const auto owner=f::fs::current_path()/(L"framework_write_fixture_"+std::to_wstring(GetCurrentProcessId())+L"_"+std::to_wstring(GetTickCount64()));
    need(f::fs::create_directory(owner),"fresh Framework path fixture creation failed");
    f::fs::create_directories(owner/L"game");f::fs::create_directories(owner/L"profile/game-data/76561198013473990");
    auto& config=mt2mp::Config::instance();config.framework_profile_verified=true;config.installed_profile_verified=false;config.local_test=true;config.diagnostic_mailbox_enabled=true;
    config.runtime_root=p::utf8(owner);config.game_dir=p::utf8(owner/L"game");config.framework_profile_directory=p::utf8(owner/L"profile");config.user_data_dir=p::utf8(owner/L"profile/game-data");
    const auto binding=f::bind(owner);const auto expected=binding.data/L"76561198013473990";
    auto raw=p::utf8(expected);std::replace(raw.begin(),raw.end(),'\\','/');raw+="/";
    const auto original_config=config;f::WriteOrigin cache;unsigned cases=0;
    auto check=[&](const std::string& label,const char* value,const f::fs::path& bound){
        const auto old=outcome([&]{baseline(binding,bound,value,fake_attributes);});
        const auto fresh=outcome([&]{cache.validate(binding,bound,value,fake_attributes);});
        if(old!=fresh)throw std::runtime_error("differential mismatch: "+label+" old="+old.cause+" candidate="+fresh.cause);
        ++cases;return fresh;
    };
    need(check("initial",raw.c_str(),expected).cause.empty(),"initial valid path rejected");
    need(check("warm exact",raw.c_str(),expected).paths.size()==1,"warm account must query fresh attributes");
    for(auto flags:{DWORD(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT),DWORD(FILE_ATTRIBUTE_NORMAL),DWORD(INVALID_FILE_ATTRIBUTES)}){
        fixture_attributes=flags;fixture_error=ERROR_ACCESS_DENIED;need(!check("changed attributes",raw.c_str(),expected).cause.empty(),"changed attributes accepted");
    }
    fixture_attributes=FILE_ATTRIBUTE_DIRECTORY;fixture_error=0;need(check("attribute recovery",raw.c_str(),expected).cause.empty(),"attribute recovery rejected");
    for(const auto& mutation:std::vector<std::function<void()>>{
        [&]{config.framework_profile_verified=false;},[&]{config.installed_profile_verified=true;},[&]{config.runtime_root+="x";},[&]{config.game_dir+="x";},
        [&]{config.framework_profile_directory+="x";},[&]{config.user_data_dir+="x";},[&]{config.diagnostic_mailbox_enabled=false;},[&]{config.local_test=false;}}){
        mutation();need(!check("changed Config",raw.c_str(),expected).cause.empty(),"changed Config accepted");config=original_config;
    }
    need(check("Config recovery",raw.c_str(),expected).cause.empty(),"Config recovery rejected");
    auto slash=raw;std::replace(slash.begin(),slash.end(),'/','\\');
    auto lower=raw;std::transform(lower.begin(),lower.end(),lower.begin(),[](unsigned char c){return char(std::tolower(c));});
    const auto no_slash=p::utf8(expected);
    for(const auto& variant:{slash,lower,no_slash,raw+"/",raw}){
        need(check("equivalent spelling fallback",variant.c_str(),expected).cause.empty(),"equivalent spelling rejected");
        need(check("equivalent spelling warm",variant.c_str(),expected).cause.empty(),"equivalent spelling warm rejected");
    }
    need(!check("null",nullptr,expected).cause.empty(),"null accepted");need(!check("empty","",expected).cause.empty(),"empty accepted");
    for(const auto& invalid:std::vector<std::string>{"relative",raw+"../76561198013473990",raw+"./",p::utf8(binding.data/"abc"),p::utf8(binding.data/"123456789012345678901"),p::utf8(binding.data/"123"/"456"),std::string("F:/bad\xff")})
        need(!check("invalid spelling",invalid.c_str(),expected).cause.empty(),"invalid path accepted");
    need(!check("wrong account",p::utf8(binding.data/"123").c_str(),expected).cause.empty(),"different account accepted");
    need(!check("empty configured root",raw.c_str(),{}).cause.empty(),"empty configured root accepted");
    need(!check("changed configured root",raw.c_str(),binding.data/"123").cause.empty(),"changed configured root accepted");
    need(check("configured root restored",raw.c_str(),expected).cause.empty(),"restored root rejected");
    const auto base=p::utf8(binding.data);need(check("base path",base.c_str(),binding.data).paths.empty(),"base must preserve no account attribute query");
    need(check("base warm",base.c_str(),binding.data).paths.empty(),"base warm must preserve no attribute query");
    need(!check("base mismatched account root",base.c_str(),expected).cause.empty(),"base accepted against account root");
    cache.clear();need(check("clear/new account",raw.c_str(),expected).cause.empty(),"new bound account rejected");
    auto changed_binding=binding;changed_binding.data/=L"changed";
    auto a=outcome([&]{baseline(changed_binding,expected,raw.c_str(),fake_attributes);});auto b=outcome([&]{cache.validate(changed_binding,expected,raw.c_str(),fake_attributes);});need(a==b&&!b.cause.empty(),"changed parsed Binding reused proof");++cases;
    const auto module_absent=GetModuleHandleW(L"libphysfs.dll")==nullptr;
    need(module_absent,"PhysFS unexpectedly loaded before isolated fixture");
    if(module_absent){
        auto missing=outcome([&]{cache.live_validate(binding,expected,fake_attributes);});need(missing.cause=="actual PhysFS write directory unavailable","missing PhysFS module error changed");++cases;
        const auto dll=f::fs::absolute(argv[0]).parent_path()/L"libphysfs.dll";
        const auto module=LoadLibraryW(dll.c_str());need(module!=nullptr,"private PhysFS test fixture load failed");
        const auto setter=reinterpret_cast<void(*)(const char*)>(GetProcAddress(module,"fixture_set_path"));
        const auto count=reinterpret_cast<uint64_t(*)()>(GetProcAddress(module,"fixture_get_calls"));
        need(setter&&count,"private PhysFS fixture exports missing");
        setter(raw.c_str());cache.live_validate(binding,expected,fake_attributes);cache.live_validate(binding,expected,fake_attributes);
        need(count()==2,"warm proof skipped fresh PhysFS query");
        const auto other=p::utf8(binding.data/"123");setter(other.c_str());
        auto changed=outcome([&]{cache.live_validate(binding,expected,fake_attributes);});
        need(changed.cause=="Framework native open actual write root changed during bound origin"&&count()==3,"changed PhysFS query reused warm path");
        setter(nullptr);auto null=outcome([&]{cache.live_validate(binding,expected,fake_attributes);});
        need(null.cause=="actual PhysFS write directory unavailable"&&count()==4,"null PhysFS query reused warm path");
        need(FreeLibrary(module)!=0,"private PhysFS test fixture unload failed");
        auto unloaded=outcome([&]{cache.live_validate(binding,expected,fake_attributes);});
        need(unloaded.cause=="actual PhysFS write directory unavailable","unloaded module reused stale getter");cases+=5;
    }
    cache.clear();attributes_count=0;
    for(unsigned i=0;i<3;++i)cache.validate(binding,expected,raw.c_str(),actual_attributes);
    need(attributes_count==3,"real account attributes were reused");
    std::cout<<"PASS: Framework write origin: "<<cases<<" path/module cases, "<<assertions<<" assertions; fresh Config/PhysFS/attributes, fallback and errors preserved\n";return 0;
}catch(const std::exception& e){std::cerr<<"error: Framework write origin: "<<e.what()<<'\n';return 1;}}
