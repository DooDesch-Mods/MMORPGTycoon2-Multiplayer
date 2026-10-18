#include "mt2mp/hooks/minhook_backend.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <stdexcept>
#include <thread>
#include <vector>

using mt2mp::hooks::framework::Backend;
namespace {
unsigned checks=0;
void require(bool value,const char* expression,int line) {
    ++checks;if(!value)throw std::runtime_error(std::string(expression)+" at line "+std::to_string(line));
}
#define CHECK(value) require(!!(value),#value,__LINE__)
constexpr uintptr_t image=0x140000000;
constexpr uintptr_t first=0x54ce0,second=0x54cf0,shared=0x463ce0;
constexpr char first_name[]="mp.purchase-subscription-price.54ce0";
constexpr char second_name[]="mp.purchase-subscription-price.54cf0";
constexpr char shared_name[]="game.update";
struct Target { const char* name; uintptr_t rva; bool shared; };
constexpr Target catalog[]{
#include "framework_catalog.inc"
};
void* address(uintptr_t rva){return reinterpret_cast<void*>(image+rva);}
void* detour(){return reinterpret_cast<void*>(uintptr_t{0x70000100});}
template<size_t N> void copy(char (&out)[N],const char* text) {
    const auto length=std::strlen(text);CHECK(length<N);std::memset(out,0,N);std::memcpy(out,text,length);
}
struct Fake {
    std::map<std::string,MT2FW_MT2_RouteReceipt> routes;
    std::vector<std::string> calls,logs;
    void** published=nullptr;
    bool publication_seen=false,allow_reclaim=true,fail_prepare=false,fail_enable=false,fail_remove=false,fail_read=false,fail_verify=false;
    bool wrong_prepare_entry=false,wrong_prepare_hash=false,wrong_prepare_owner=false,zero_original=false,clean_prepare_failure=false;
    uint32_t verified_offset=0,verified_length=0;
    const void* verified_bytes=nullptr;
    std::string verified_name;
    static MT2FW_MT2_RouteReceipt initial(uintptr_t rva) {
        MT2FW_MT2_RouteReceipt value{};value.size=sizeof value;value.rva=rva;value.entry=image+rva;
        copy(value.game_sha256,"bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6");return value;
    }
    Fake() {
        for(const auto& target:catalog){auto value=initial(target.rva);
            if(target.shared){value.state=MT2FW_MT2_ROUTE_ENABLED;value.generation=7;value.detour=0x8800;value.trampoline=0x9900;copy(value.owner,"framework");}
            routes[target.name]=value;
        }
    }
    MT2FW_MT2_RoutesV1 api() {
        MT2FW_MT2_RoutesV1 value{};value.size=sizeof value;value.version=MT2FW_MT2_ROUTES_VERSION;value.context=this;
        value.hook_prepare=prepare;value.hook_enable=enable;value.hook_remove=remove;value.route_read=read;value.code_verify=verify;return value;
    }
    static void log(void* context,const char* operation,const std::string& cause) {
        CHECK(!cause.empty());static_cast<Fake*>(context)->logs.push_back(std::string(operation)+":"+cause);
    }
    static bool reclaim(void* context,uintptr_t target,std::string& cause) {
        auto& self=*static_cast<Fake*>(context);self.calls.push_back("reclaim:"+std::to_string(target));
        if(!self.allow_reclaim){cause="original is still published";return false;}cause.clear();return true;
    }
    bool bind(Backend& backend) {
        return backend.bind(api(),image,"doodesch.multiplayer",GetCurrentThreadId(),reclaim,log,this);
    }
    static MT2FW_MT2_Result MT2FW_CALL prepare(void* context,const char* name,void* replacement,MT2FW_MT2_RouteReceipt* out) {
        auto& self=*static_cast<Fake*>(context);self.calls.push_back(std::string("prepare:")+name);
        if(self.clean_prepare_failure){*out=self.routes.at(name);copy(out->cause,"prepare failed before ownership");return MT2FW_MT2_FAILED;}
        auto& value=self.routes.at(name);value.state=MT2FW_MT2_ROUTE_PREPARED;value.generation=42;
        value.detour=reinterpret_cast<uintptr_t>(replacement);value.trampoline=0x7f000000+value.rva;
        copy(value.owner,"doodesch.multiplayer");
        if(self.fail_prepare){value.state=MT2FW_MT2_ROUTE_UNKNOWN;copy(value.cause,"prepare mutation outcome unknown");}
        *out=value;
        if(self.wrong_prepare_entry)++out->entry;
        if(self.wrong_prepare_hash)out->game_sha256[0]='0';
        if(self.wrong_prepare_owner)copy(out->owner,"another.plugin");
        if(self.zero_original)out->trampoline=0;
        return self.fail_prepare?MT2FW_MT2_FAILED:MT2FW_MT2_OK;
    }
    static MT2FW_MT2_Result MT2FW_CALL enable(void* context,const char* name,MT2FW_MT2_RouteReceipt* out) {
        auto& self=*static_cast<Fake*>(context);self.calls.push_back(std::string("enable:")+name);
        auto& value=self.routes.at(name);
        self.publication_seen=self.published&&*self.published==reinterpret_cast<void*>(value.trampoline);
        value.state=self.fail_enable?MT2FW_MT2_ROUTE_UNKNOWN:MT2FW_MT2_ROUTE_ENABLED;
        if(self.fail_enable)copy(value.cause,"enable mutation outcome unknown");
        *out=value;return self.fail_enable?MT2FW_MT2_FAILED:MT2FW_MT2_OK;
    }
    static MT2FW_MT2_Result MT2FW_CALL remove(void* context,const char* name,MT2FW_MT2_RouteReceipt* out) {
        auto& self=*static_cast<Fake*>(context);self.calls.push_back(std::string("remove:")+name);auto& value=self.routes.at(name);
        if(self.fail_remove){value.state=MT2FW_MT2_ROUTE_UNKNOWN;copy(value.cause,"remove mutation outcome unknown");*out=value;return MT2FW_MT2_FAILED;}
        value.state=MT2FW_MT2_ROUTE_REMOVED;value.owner[0]=0;value.trampoline=0;value.detour=0;*out=value;return MT2FW_MT2_OK;
    }
    static MT2FW_MT2_Result MT2FW_CALL read(void* context,const char* name,MT2FW_MT2_RouteReceipt* out) {
        auto& self=*static_cast<Fake*>(context);self.calls.push_back(std::string("read:")+name);*out=self.routes.at(name);
        if(self.fail_read){copy(out->cause,"read failed");return MT2FW_MT2_FAILED;}return MT2FW_MT2_OK;
    }
    static MT2FW_MT2_Result MT2FW_CALL verify(void* context,const char* name,uint32_t offset,const void* expected,uint32_t length,MT2FW_MT2_RouteReceipt* out) {
        auto& self=*static_cast<Fake*>(context);self.calls.push_back(std::string("verify:")+name);
        self.verified_name=name;self.verified_offset=offset;self.verified_length=length;self.verified_bytes=expected;
        *out=self.routes.at(name);if(self.fail_verify){copy(out->cause,"pristine or live bytes differ");return MT2FW_MT2_FAILED;}return MT2FW_MT2_OK;
    }
    size_t count(const char* prefix)const {
        return static_cast<size_t>(std::count_if(calls.begin(),calls.end(),[&](const auto& call){return call.starts_with(prefix);}));
    }
};
void publication_and_cleanup() {
    Fake fake;Backend backend;CHECK(fake.bind(backend));CHECK(backend.initialize()==MH_OK);
    void* original=nullptr;fake.published=&original;
    CHECK(backend.create(address(first),detour(),&original)==MH_OK);
    CHECK(original==reinterpret_cast<void*>(0x7f000000+first));CHECK(fake.calls.size()==1);CHECK(fake.count("enable:")==0);
    CHECK(backend.cleanup()==MH_UNKNOWN);CHECK(backend.retained()==1);CHECK(fake.count("remove:")==0);
    CHECK(backend.enable(address(first))==MH_OK);CHECK(fake.publication_seen);
    CHECK(fake.calls==std::vector<std::string>({std::string("prepare:")+first_name,std::string("read:")+first_name,std::string("enable:")+first_name}));
    CHECK(backend.enable(address(first))==MH_ERROR_ENABLED);CHECK(fake.count("enable:")==1);
    const auto before=fake.calls.size();CHECK(backend.disable(address(first))==MH_ERROR_UNSUPPORTED_FUNCTION);
    CHECK(fake.calls.size()==before+1);CHECK(fake.calls.back()==std::string("read:")+first_name);CHECK(backend.retained()==1);CHECK(backend.error().find("Retaining disable requires the ownership service")!=std::string::npos);
    fake.allow_reclaim=false;CHECK(backend.remove(address(first))==MH_UNKNOWN);CHECK(fake.count("remove:")==0);CHECK(backend.retained()==1);
    fake.allow_reclaim=true;CHECK(backend.remove(address(first))==MH_OK);CHECK(backend.retained()==0);CHECK(fake.count("remove:")==1);
    CHECK(backend.cleanup()==MH_OK);CHECK(backend.remove(address(first))==MH_ERROR_NOT_CREATED);CHECK(fake.count("remove:")==1);
    CHECK(backend.create(address(first),detour(),&original)==MH_ERROR_ALREADY_CREATED);CHECK(fake.count("prepare:")==1);
}
void owner_local_all() {
    Fake fake;Backend backend;CHECK(fake.bind(backend));void* a=nullptr;void* b=nullptr;
    CHECK(backend.create(address(first),detour(),&a)==MH_OK);CHECK(backend.create(address(second),detour(),&b)==MH_OK);
    CHECK(backend.enable(MH_ALL_HOOKS)==MH_OK);CHECK(fake.count("enable:")==2);
    CHECK(fake.routes.at(shared_name).generation==7);CHECK(fake.routes.at(shared_name).state==MT2FW_MT2_ROUTE_ENABLED);
    CHECK(backend.disable(MH_ALL_HOOKS)==MH_ERROR_UNSUPPORTED_FUNCTION);CHECK(fake.count("remove:")==0);
    CHECK(backend.remove(MH_ALL_HOOKS)==MH_ERROR_NOT_CREATED);CHECK(backend.remove(address(shared))==MH_ERROR_NOT_CREATED);
    CHECK(backend.enable(address(shared))==MH_ERROR_NOT_CREATED);CHECK(fake.count("remove:")==0);
    CHECK(backend.enable(MH_ALL_HOOKS)==MH_OK);CHECK(fake.count("enable:")==2);
}
void no_replay() {
    for(unsigned mode=0;mode<6;++mode){
        Fake fake;Backend backend;CHECK(fake.bind(backend));void* original=reinterpret_cast<void*>(uintptr_t{0x1234});
        fake.fail_prepare=mode==0;fake.wrong_prepare_entry=mode==1;fake.wrong_prepare_hash=mode==2;fake.wrong_prepare_owner=mode==3;fake.zero_original=mode==4;
        fake.clean_prepare_failure=mode==5;
        CHECK(backend.create(address(first),detour(),&original)==MH_UNKNOWN);CHECK(original==reinterpret_cast<void*>(uintptr_t{0x1234}));
        CHECK(backend.retained()==1);CHECK(backend.create(address(first),detour(),&original)==MH_ERROR_ALREADY_CREATED);
        CHECK(backend.enable(address(first))==MH_UNKNOWN);CHECK(backend.remove(address(first))==MH_UNKNOWN);CHECK(backend.cleanup()==MH_UNKNOWN);
        CHECK(fake.count("prepare:")==1);CHECK(fake.count("enable:")==0);CHECK(fake.count("remove:")==0);
    }
    for(unsigned mode=0;mode<4;++mode){
        Fake fake;Backend backend;CHECK(fake.bind(backend));void* original=nullptr;CHECK(backend.create(address(first),detour(),&original)==MH_OK);
        if(mode==0)fake.fail_enable=true;
        if(mode==1)copy(fake.routes.at(first_name).owner,"another.plugin");
        if(mode==2)++fake.routes.at(first_name).generation;
        if(mode==3)fake.fail_read=true;
        CHECK(backend.enable(address(first))==MH_UNKNOWN);CHECK(backend.enable(address(first))==MH_UNKNOWN);
        CHECK(backend.remove(address(first))==MH_UNKNOWN);CHECK(backend.cleanup()==MH_UNKNOWN);
        CHECK(fake.count("enable:")==static_cast<size_t>(mode==0));CHECK(fake.count("remove:")==0);CHECK(backend.retained()==1);
    }
    Fake fake;Backend backend;CHECK(fake.bind(backend));void* original=nullptr;CHECK(backend.create(address(first),detour(),&original)==MH_OK);
    fake.fail_remove=true;CHECK(backend.remove(address(first))==MH_UNKNOWN);CHECK(backend.remove(address(first))==MH_UNKNOWN);
    CHECK(fake.count("remove:")==1);CHECK(backend.retained()==1);CHECK(backend.cleanup()==MH_UNKNOWN);
}
void proofs_and_rejections() {
    Fake fake;Backend backend;CHECK(fake.bind(backend));void* original=nullptr;std::string error;unsigned char bytes[8]{};
    CHECK(backend.create(address(shared),detour(),&original)==MH_ERROR_UNSUPPORTED_FUNCTION);CHECK(original==nullptr);CHECK(fake.calls.empty());
    CHECK(backend.create(address(0x1c52a00),detour(),&original)==MH_ERROR_UNSUPPORTED_FUNCTION);CHECK(fake.calls.empty());
    CHECK(backend.create(reinterpret_cast<void*>(0x20),detour(),&original)==MH_ERROR_UNSUPPORTED_FUNCTION);CHECK(fake.calls.empty());
    CHECK(backend.create(address(first),nullptr,&original)==MH_ERROR_NOT_EXECUTABLE);CHECK(fake.calls.empty());
    CHECK(backend.create(address(first),detour(),nullptr)==MH_ERROR_NOT_EXECUTABLE);CHECK(fake.calls.empty());
    for(const auto& target:catalog)if(target.shared){
        CHECK(backend.create(address(target.rva),detour(),&original)==MH_ERROR_UNSUPPORTED_FUNCTION);
        CHECK(backend.verify_code(image+target.rva,bytes,sizeof bytes,error));
    }
    CHECK(fake.count("prepare:")==0);
    MT2FW_MT2_RouteReceipt out{};CHECK(backend.read_route(address(shared),out));CHECK(out.owner[0]=='f');
    CHECK(backend.verify_code(image+shared+5,bytes,sizeof bytes,error));CHECK(error.empty());CHECK(fake.verified_name==shared_name);
    CHECK(fake.verified_offset==5&&fake.verified_length==sizeof bytes&&fake.verified_bytes==bytes);
    const auto before=fake.count("verify:");CHECK(!backend.verify_code(0x20,bytes,sizeof bytes,error));CHECK(!error.empty());
    CHECK(!backend.verify_code(image+shared,nullptr,sizeof bytes,error));CHECK(!backend.verify_code(image+shared,bytes,0,error));
    CHECK(!backend.verify_code(image+shared,bytes,4097,error));CHECK(!backend.verify_code((std::numeric_limits<uintptr_t>::max)(),bytes,sizeof bytes,error));
    CHECK(fake.count("verify:")==before);
    fake.fail_verify=true;CHECK(!backend.verify_code(image+shared,bytes,sizeof bytes,error));CHECK(error.find("pristine or live bytes differ")!=std::string::npos);
    fake.fail_verify=false;copy(fake.routes.at(shared_name).owner,"another.plugin");CHECK(!backend.verify_code(image+shared,bytes,sizeof bytes,error));
    CHECK(!backend.read_route(address(shared),out));CHECK(fake.count("prepare:")==0);CHECK(fake.count("enable:")==0);CHECK(fake.count("remove:")==0);
    CHECK(backend.verify_code(image+first,bytes,sizeof bytes,error));
    CHECK(backend.create(address(first),detour(),&original)==MH_OK);CHECK(backend.verify_code(image+first,bytes,sizeof bytes,error));
    CHECK(backend.enable(address(first))==MH_OK);CHECK(backend.verify_code(image+first,bytes,sizeof bytes,error));
}
void monster_named_admission() {
    constexpr uintptr_t monster=0x5bf3c0;
    constexpr char name[]="mp.monster-release-capture.5bf3c0";
    const auto item=std::find_if(std::begin(catalog),std::end(catalog),[](const auto& row){return row.rva==monster;});
    CHECK(item!=std::end(catalog));CHECK(std::strcmp(item->name,name)==0);CHECK(!item->shared);
    CHECK(std::count_if(std::begin(catalog),std::end(catalog),[](const auto& row){return row.rva==monster;})==1);
    Fake fake;Backend backend;CHECK(fake.bind(backend));void* original=nullptr;fake.published=&original;
    CHECK(backend.create(address(monster),detour(),&original)==MH_OK);
    CHECK(original==reinterpret_cast<void*>(0x7f000000+monster));
    CHECK(fake.calls==std::vector<std::string>({std::string("prepare:")+name}));
    CHECK(backend.enable(address(monster))==MH_OK);CHECK(fake.publication_seen);
    CHECK(fake.calls==std::vector<std::string>({std::string("prepare:")+name,std::string("read:")+name,std::string("enable:")+name}));
    MT2FW_MT2_RouteReceipt receipt{};CHECK(backend.read_route(address(monster),receipt));
    CHECK(receipt.state==MT2FW_MT2_ROUTE_ENABLED&&receipt.rva==monster&&receipt.entry==image+monster);
    CHECK(std::strcmp(receipt.owner,"doodesch.multiplayer")==0&&receipt.generation==42);
    CHECK(receipt.detour==reinterpret_cast<uintptr_t>(detour())&&receipt.trampoline==reinterpret_cast<uintptr_t>(original));
    const auto before=fake.calls.size();void* untouched=reinterpret_cast<void*>(uintptr_t{0x1234});
    CHECK(backend.create(address(monster+1),detour(),&untouched)==MH_ERROR_UNSUPPORTED_FUNCTION);
    CHECK(untouched==reinterpret_cast<void*>(uintptr_t{0x1234})&&fake.calls.size()==before);
    CHECK(backend.error().find("Target is not in the pinned Framework EXE catalog")!=std::string::npos);
    CHECK(!fake.logs.empty()&&fake.logs.back().find(backend.error())!=std::string::npos);
    Fake broken;Backend rejected;CHECK(broken.bind(rejected));broken.clean_prepare_failure=true;
    CHECK(rejected.create(address(monster),detour(),&untouched)==MH_UNKNOWN);
    CHECK(rejected.error().find(std::string("prepare failed or receipt differs: ")+name)!=std::string::npos);
    CHECK(rejected.error().find("prepare failed before ownership")!=std::string::npos);
    CHECK(rejected.enable(address(monster))==MH_UNKNOWN&&broken.count("prepare:")==1&&broken.count("enable:")==0);
}
void admission_and_shim() {
    Backend missing;CHECK(missing.initialize()==MH_ERROR_NOT_INITIALIZED);CHECK(!missing.error().empty());
    Fake invalid;auto api=invalid.api();api.code_verify=nullptr;
    CHECK(!missing.bind(api,image,"doodesch.multiplayer",GetCurrentThreadId(),Fake::reclaim,Fake::log,&invalid));
    Fake fake;Backend backend;CHECK(fake.bind(backend));CHECK(!fake.bind(backend));
    auto result=MH_OK;std::thread worker([&]{result=backend.initialize();});worker.join();CHECK(result==MH_ERROR_NOT_INITIALIZED);CHECK(fake.calls.empty());
    Fake global;auto& process=mt2mp::hooks::framework::process_backend();CHECK(global.bind(process));
    CHECK(MH_Initialize()==MH_OK);void* original=nullptr;global.published=&original;
    CHECK(MH_CreateHook(address(first),detour(),&original)==MH_OK);CHECK(MH_EnableHook(MH_ALL_HOOKS)==MH_OK);CHECK(global.publication_seen);
    CHECK(MH_DisableHook(MH_ALL_HOOKS)==MH_ERROR_UNSUPPORTED_FUNCTION);CHECK(MH_Uninitialize()==MH_UNKNOWN);
    CHECK(std::strstr(MH_StatusToString(MH_OK),"plugin-owned")!=nullptr);
    unsigned char expected=0;std::string error;CHECK(mt2mp::hooks::framework::verify_code(image+shared,&expected,1,error));
    CHECK(MH_RemoveHook(address(first))==MH_OK);CHECK(MH_Uninitialize()==MH_OK);CHECK(global.count("remove:")==1);
}
}
int main() {
    try{publication_and_cleanup();owner_local_all();no_replay();proofs_and_rejections();monster_named_admission();admission_and_shim();
        std::printf("framework_hook_backend: %u checks passed\n",checks);return 0;}
    catch(const std::exception& error){std::fprintf(stderr,"framework_hook_backend failed: %s\n",error.what());return 1;}
}
