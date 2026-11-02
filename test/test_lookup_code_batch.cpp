#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
namespace mt2mp::native_uid {inline constexpr std::string_view required_exe_sha="fixture-exe";}
namespace mt2mp::game::live_host_state::apply_timing {inline uint64_t original_call_revision=0;}
namespace fixture {
uint32_t thread_id=7;uint32_t GetCurrentThreadId(){return thread_id;}
using FindUid=void*(*)(uint64_t);using DynamicCast=void*(*)(void*,const void*,const void*,ptrdiff_t);
struct LookupApi {FindUid find=nullptr;DynamicCast cast=nullptr;};
struct Memory {virtual bool copy(uintptr_t,void*,size_t,std::string&)=0;};
inline std::map<uintptr_t,uint8_t> bytes;inline int reads=0;
struct ProcessMemory:Memory {bool copy(uintptr_t address,void* out,size_t size,std::string& e)override{++reads;auto* dest=static_cast<uint8_t*>(out);for(size_t i=0;i<size;++i){auto found=bytes.find(address+i);if(found==bytes.end()){e="missing code";return false;}dest[i]=found->second;}return true;}};
bool fail(std::string& e,const char* cause){if(e.empty())e=cause;return false;}
#include "lookup_raw.inc"
struct Proof {bool failed=false;bool healthy(std::string&)const{return !failed;}bool reject(std::string&){failed=true;return false;}};
struct RootSceneryReadBatch {
    struct State {Proof proof;uintptr_t lookup_image=0;uint32_t lookup_thread=0;uint64_t lookup_revision=0;LookupApi lookup_api;
        bool verify_origin(std::string&){return true;}bool finish_body(std::string&){return true;}
#include "lookup_batch_methods.inc"
    };
    static thread_local RootSceneryReadBatch* current_;std::shared_ptr<State> state_=std::make_shared<State>();
    static bool resolve_current_lookup(Memory&,uintptr_t,std::string_view,LookupApi&,std::string&);
};
thread_local RootSceneryReadBatch* RootSceneryReadBatch::current_=nullptr;
#include "lookup_batch_dispatch.inc"
void need(bool value,const char* cause){if(!value)throw std::runtime_error(cause);}
void install(uintptr_t image){const std::vector<uint8_t> a={0x53,0x45,0x31,0xc0,0x48,0x85,0xc9,0x74,0x31,0x48,0x83,0x3d,0xa7,0x95,0x5b,0x01},b={0x41,0x54,0x55,0x57,0x56,0x53,0x48,0x83,0xec,0x70,0x48,0x89,0xcb,0x48,0x89,0xd5};for(size_t i=0;i<a.size();++i)bytes[image+0x47ce20+i]=a[i];for(size_t i=0;i<b.size();++i)bytes[image+0x12654f0+i]=b[i];}
}
int main(){try{using namespace fixture;constexpr uintptr_t image=0x10000000;install(image);ProcessMemory memory;LookupApi api;std::string e;
    RootSceneryReadBatch batch;RootSceneryReadBatch::current_=&batch;reads=0;
    for(int i=0;i<8;++i)need(resolve_lookup(memory,image,mt2mp::native_uid::required_exe_sha,api,e),"batched lookup failed");
    need(reads==2,"lookup verifier repeated per row");need(batch.state_->finish(e)&&reads==4,"finish did not freshly revalidate both prologues");
    RootSceneryReadBatch changed;RootSceneryReadBatch::current_=&changed;e.clear();need(resolve_lookup(memory,image,mt2mp::native_uid::required_exe_sha,api,e),"changed-code setup failed");bytes[image+0x47ce20]^=1;need(!changed.state_->finish(e),"changed code accepted at finish");install(image);
    RootSceneryReadBatch owner;RootSceneryReadBatch::current_=&owner;e.clear();need(resolve_lookup(memory,image,mt2mp::native_uid::required_exe_sha,api,e),"owner setup failed");++thread_id;need(!resolve_lookup(memory,image,mt2mp::native_uid::required_exe_sha,api,e),"changed thread accepted");--thread_id;
    RootSceneryReadBatch revision;RootSceneryReadBatch::current_=&revision;e.clear();need(resolve_lookup(memory,image,mt2mp::native_uid::required_exe_sha,api,e),"revision setup failed");++mt2mp::game::live_host_state::apply_timing::original_call_revision;need(!revision.state_->finish(e),"native boundary change accepted");
    RootSceneryReadBatch changed_image;RootSceneryReadBatch::current_=&changed_image;e.clear();need(resolve_lookup(memory,image,mt2mp::native_uid::required_exe_sha,api,e),"image setup failed");need(!resolve_lookup(memory,image+0x1000,mt2mp::native_uid::required_exe_sha,api,e)&&!api.find,"changed image accepted");
    RootSceneryReadBatch changed_hash;RootSceneryReadBatch::current_=&changed_hash;e.clear();need(resolve_lookup(memory,image,mt2mp::native_uid::required_exe_sha,api,e),"hash setup failed");need(!resolve_lookup(memory,image,"different",api,e)&&!api.find,"changed hash accepted");
    RootSceneryReadBatch missing;RootSceneryReadBatch::current_=&missing;e.clear();need(resolve_lookup(memory,image,mt2mp::native_uid::required_exe_sha,api,e),"read failure setup failed");bytes.erase(image+0x12654f0);need(!missing.state_->finish(e),"final code read failure accepted");install(image);
    RootSceneryReadBatch initial;RootSceneryReadBatch::current_=&initial;bytes.erase(image+0x47ce20);e.clear();need(!resolve_lookup(memory,image,mt2mp::native_uid::required_exe_sha,api,e),"initial read failure accepted");install(image);e.clear();need(!resolve_lookup(memory,image,mt2mp::native_uid::required_exe_sha,api,e),"failed proof was reused");
    RootSceneryReadBatch exhausted;RootSceneryReadBatch::current_=&exhausted;mt2mp::game::live_host_state::apply_timing::original_call_revision=UINT64_MAX;e.clear();need(!resolve_lookup(memory,image,mt2mp::native_uid::required_exe_sha,api,e),"exhausted revision accepted");mt2mp::game::live_host_state::apply_timing::original_call_revision=0;
    RootSceneryReadBatch outer,inner;inner.state_=outer.state_;RootSceneryReadBatch::current_=&outer;reads=0;e.clear();need(resolve_lookup(memory,image,mt2mp::native_uid::required_exe_sha,api,e),"outer lookup failed");RootSceneryReadBatch::current_=&inner;need(resolve_lookup(memory,image,mt2mp::native_uid::required_exe_sha,api,e)&&reads==2,"shared nested proof did not reuse lookup");need(inner.state_->finish(e)&&reads==4,"nested finish skipped fresh proof");RootSceneryReadBatch::current_=&outer;need(outer.state_->finish(e)&&reads==6,"outer finish skipped fresh proof");
    RootSceneryReadBatch::current_=nullptr;std::cout<<"actual lookup batch proof passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
