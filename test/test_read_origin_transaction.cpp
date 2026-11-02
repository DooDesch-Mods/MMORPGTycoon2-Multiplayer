#include "mt2mp/core/framework_open_paths.h"
#include "mt2mp/net/snapshot_sync.h"
#include "native_open_witness.h"
#include "region_rng_snapshot.h"
#include "live_host_state/memory_query_timing.h"
#include <native_steps_draft.h>
#include <atomic>
#include <functional>
#include <iostream>
#include <mutex>
#include <optional>
#include <set>
#include <thread>
#include <type_traits>

namespace mt2mp {Config& Config::instance(){static Config config;return config;}}
namespace t=mt2mp::game::live_host_state::apply_timing;
namespace f=mt2mp::framework_open_paths;
namespace p=mt2mp::private_profile;
unsigned assertions=0;
void need(bool value,const char* cause){++assertions;if(!value)throw std::runtime_error(cause);}
std::vector<std::string> events;
std::function<void()> metadata_action,copy_action,attribute_action;
DWORD attribute_override=0;
bool throw_copy=false;

namespace mt2mp::game::building_mixed_identity::opened_file {
namespace {
std::mutex gate;
Status current;
uintptr_t image=0;
void* native_mode=nullptr;
void* mode_override=nullptr;
std::filesystem::path configured_root,configured_write,physical;
f::Binding configured_paths;
f::WriteOrigin configured_write_origin;
uint64_t world_epoch=0;
enum class InitPhase {Uninitialized,Ready,Failed};
InitPhase init_phase=InitPhase::Ready;
std::string init_error;
uint32_t installed_hooks=required_hooks;
std::atomic<bool> callback_failure=false;
std::atomic_flag emergency_gate=ATOMIC_FLAG_INIT;
std::array<char,384> emergency_cause{};
bool emergency_truncated=false;
struct WorldWitness {void* game=nullptr;void* mode=nullptr;void* state=nullptr;void* world=nullptr;void* map=nullptr;bool operator==(const WorldWitness&)const=default;};
WorldWitness loaded_world;
DWORD WINAPI observed_account_attributes(LPCWSTR value){
    events.push_back("attributes");
    if(attribute_action)attribute_action();
    return attribute_override?attribute_override:GetFileAttributesW(value);
}
#include "read_origin_binding.inc"
DWORD copy(const void*,void*,size_t);
#include "read_origin_helpers.inc"
struct FieldRead {const void* address;size_t bytes;std::vector<unsigned char> value;bool operator==(const FieldRead&)const=default;};
std::vector<FieldRead> reads;
DWORD copy(const void* from,void* to,size_t bytes){
    events.push_back("field");
    if(copy_action)copy_action();
    if(throw_copy)throw std::runtime_error("exact field exception");
    const auto result=native_copy(from,to,bytes);
    reads.push_back({from,bytes,{}});
    if(!result)reads.back().value.assign(static_cast<unsigned char*>(to),static_cast<unsigned char*>(to)+bytes);
    return result;
}
}
#include "read_origin_native.inc"
}
namespace opened=mt2mp::game::building_mixed_identity::opened_file;
namespace mt2mp::game::native_open_runtime {
namespace witness=building_mixed_identity::opened_file;
std::atomic<uint64_t> active_generation=37;
#include "read_origin_runtime.inc"
}
namespace mt2mp::game::normal_load_identity {
bool read_load_phase(const std::string&,const std::string&,bool& confirmed,std::string&){confirmed=true;return true;}
}
namespace mt2mp::game::monster_saved_identity_runtime {
bool read_prepared_region_rng(const std::string&,const std::string&,uint64_t,uint32_t,uint32_t,region_rng_snapshot::Proof& out,std::string&){
    if(metadata_action)metadata_action();
    out={};out.mode=1;out.sha256=std::string(64,'a');return true;
}
}
namespace mt2mp::net::session_wire {
uint64_t session=17,local=1;bool pending=false,online=true;
std::array<uint64_t,5> members{0,101,102,0,0};
uint64_t session_id(){return session;}
uint64_t local_peer(){return local;}
bool connected(){return online;}
bool membership_pending(){return pending;}
uint8_t peer_mask(){return 3;}
bool read_binding(std::array<uint64_t,5>& out,uint64_t& lobby,uint64_t& peer,std::string&,bool){out=members;lobby=19;peer=local;return true;}
}
namespace mt2mp::net {
std::optional<OrderedSnapshotIdentity> ordered_snapshot;
uint32_t ordered_thread=0;
bool ordered_original_cohort=true,ordered_load_complete=true,ordered_load_requested=true;
std::string ordered_failure;
#include "read_origin_snapshot.inc"
}
namespace mt2mp::game::ordered_session {
#include "read_origin_parent.inc"
}
namespace proof=mt2mp::game::ordered_session;
template<class T>void put(void* object,size_t offset,T value){std::memcpy(static_cast<unsigned char*>(object)+offset,&value,sizeof value);}
unsigned char* objects=nullptr;
f::fs::path owner,account;
std::optional<mt2mp::Config> original_config;
void(*set_path)(const char*)=nullptr;
std::string raw,other_raw;
void reset(){
    using namespace opened;
    current={};current.state="ready";current.generation=37;current.game_thread=GetCurrentThreadId();
    current.completed_origin=current.current_origin_available=current.world_witness_bound=true;
    current.initialized=true;current.hook_mask=required_hooks;current.native_request_attempted=true;
    current.origin_world_epoch=world_epoch=41;current.uuid="fixture-world";current.sha256=std::string(64,'a');current.bytes=current.hash_bytes=1234;
    init_phase=InitPhase::Ready;installed_hooks=required_hooks;callback_failure=false;
    native_mode=objects;loaded_world={objects+0x300,objects,objects+0x600,objects+0x900,objects+0xc00};
    put(reinterpret_cast<void*>(image),0x13df320,native_mode);
    put(reinterpret_cast<void*>(image),0x13df8f0,loaded_world.game);put(reinterpret_cast<void*>(image),0x13df9c0,loaded_world.world);put(reinterpret_cast<void*>(image),0x13df8c0,loaded_world.map);
    put(objects,0,image+0x16e6730);put(objects,0x2c4,int32_t(4));put(objects,0x58,loaded_world.state);
    put(objects+0x300,0,image+0x1743740);put(objects+0x300,0x28,native_mode);
    put(objects+0x600,0,image+0x16de920);put(objects+0x600,0x160,loaded_world.world);
    put(objects+0x900,0,image+0x174aef0);put(objects+0x900,0x158,loaded_world.map);
    put(objects+0xc00,0,image+0x1742860);put(objects+0xe00,0,image+0x1742860);
    mt2mp::Config::instance()=*original_config;set_path(raw.c_str());configured_write_origin.clear();
    metadata_action={};copy_action={};attribute_action={};attribute_override=0;throw_copy=false;
    events.clear();reads.clear();t::original_call_revision=0;
    mt2mp::game::native_open_runtime::active_generation=37;
    using namespace mt2mp::net;
    session_wire::session=17;session_wire::local=1;session_wire::members={0,101,102,0,0};session_wire::pending=false;session_wire::online=true;
    ordered_snapshot=OrderedSnapshotIdentity{};auto& identity=*ordered_snapshot;
    identity.members=session_wire::members;identity.session=17;identity.lobby=19;identity.local_peer=1;identity.peer_mask=3;
    identity.snapshot_uuid=current.uuid;identity.sha256=current.sha256;identity.bytes=current.bytes;
    identity.native_generation=37;identity.seed_present=identity.file_verified=identity.native_prepared=true;identity.seed=7;identity.region_rng_mode=1;
    ordered_thread=GetCurrentThreadId();ordered_failure.clear();ordered_original_cohort=true;ordered_load_complete=true;ordered_load_requested=true;
}
struct Outcome {bool ok;MT2FW_MT2_StepProofReply result;uint32_t role;std::string error;};
Outcome run(bool candidate=true,bool recovery=false){
    Outcome out{};
    out.ok=candidate?(recovery?proof::read_native_proof_impl<true>(out.result,out.role,out.error):proof::read_native_proof_impl<false>(out.result,out.role,out.error)):
        proof::baseline_read_native_proof_impl<false>(out.result,out.role,out.error);
    need(t::page_read_scope==nullptr,"parent leaked PageReadScope");return out;
}
void rejected(const std::function<void()>& mutation,const char* expected){
    reset();metadata_action=mutation;const auto out=run();
    if(out.ok||out.error.find(expected)==std::string::npos)throw std::runtime_error("wrong rejected cause: expected="+std::string(expected)+" actual="+out.error);
    ++assertions;
}
int main(int argc,char** argv){try{
    static_assert(!std::is_copy_constructible_v<opened::ReadOriginTransaction>&&!std::is_move_constructible_v<opened::ReadOriginTransaction>);
    need(argc==1,"fixture takes no arguments");
    owner=f::fs::current_path()/("read_origin_fixture_"+std::to_string(GetCurrentProcessId()));
    need(f::fs::create_directory(owner),"fresh fixture directory required");
    f::fs::create_directories(owner/"game");account=owner/"profile/game-data/76561198013473990";f::fs::create_directories(account);f::fs::create_directories(account.parent_path()/"123");
    auto& config=mt2mp::Config::instance();config.framework_profile_verified=true;config.local_test=true;config.diagnostic_mailbox_enabled=true;
    config.runtime_root=p::utf8(owner);config.game_dir=p::utf8(owner/"game");config.framework_profile_directory=p::utf8(owner/"profile");config.user_data_dir=p::utf8(owner/"profile/game-data");
    original_config=config;opened::configured_root=owner;opened::configured_paths=f::bind(owner);opened::configured_write=account;opened::physical=account/"fixture.sav";
    const auto module=LoadLibraryW((f::fs::absolute(argv[0]).parent_path()/"libphysfs.dll").c_str());need(module!=nullptr,"PhysFS fixture load failed");
    set_path=reinterpret_cast<decltype(set_path)>(GetProcAddress(module,"fixture_set_path"));need(set_path!=nullptr,"PhysFS setter missing");raw=p::utf8(account);other_raw=p::utf8(account.parent_path()/"123");
    struct Memory {void* image=nullptr;void* objects=nullptr;~Memory(){for(auto value:{image,objects})if(value&&!VirtualFree(value,0,MEM_RELEASE))std::cerr<<"error: fixture memory release "<<GetLastError()<<'\n';}} memory;
    memory.image=VirtualAlloc(nullptr,0x13e0000,MEM_RESERVE,PAGE_NOACCESS);need(memory.image!=nullptr,"reserve image");opened::image=reinterpret_cast<uintptr_t>(memory.image);
    need(VirtualAlloc(reinterpret_cast<void*>(opened::image+0x13df000),4096,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"commit image singleton page");
    memory.objects=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);need(memory.objects!=nullptr,"allocate objects");objects=static_cast<unsigned char*>(memory.objects);
    reset();const auto baseline=run(false);const auto baseline_reads=opened::reads;
    need(baseline.ok&&std::count(events.begin(),events.end(),"attributes")==4&&opened::reads.size()==56,"actual baseline parent must validate four times and copy 56 fields");
    reset();t::totals={};Outcome candidate{};
    {t::Scope take(t::Part::Take);candidate=run();}
    need(candidate.ok&&std::memcmp(&baseline.result,&candidate.result,sizeof candidate.result)==0&&baseline.role==candidate.role,"actual parent proof result differs");
    need(opened::reads==baseline_reads&&opened::current.world_checks==4,"all four live passes and all 56 copied bytes remain identical");
    need(std::count(events.begin(),events.end(),"attributes")==2&&events.front()=="attributes"&&events.back()=="attributes","entry and successful finish require fresh account attributes");
    need(t::totals[size_t(t::Part::NativeOpenFrameworkBinding)].count==2&&
         t::totals[size_t(t::Part::NativeOpenAccountAttributes)].count==2&&
         t::totals[size_t(t::Part::NativeOpenWorldFields)].count==4,"actual timing scopes keep two binding/attribute reads and four field passes");
    reset();need(run().ok&&run().ok&&std::count(events.begin(),events.end(),"attributes")==4,"next independent parent must validate afresh");
    reset();bool nested=false;metadata_action=[&]{if(nested)return;nested=true;need(run().ok,"nested independent proof");nested=false;};
    need(run().ok&&opened::reads.size()==112&&std::count(events.begin(),events.end(),"attributes")==4,"nested parents cannot borrow each other's origin proof");
    reset();attribute_override=FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY;
    need(!run().ok&&opened::reads.empty(),"fresh entry validation rejects reparse before any field copy");
    reset();opened::CurrentWorld world;std::string error;need(mt2mp::game::native_open_runtime::read_current_world(world,error)&&mt2mp::game::native_open_runtime::read_current_world(world,error)&&std::count(events.begin(),events.end(),"attributes")==4,"ordinary public reader keeps two fresh validations each");
    reset();opened::current.native_request_attempted=false;opened::current.completed_origin=false;opened::current.world_witness_bound=false;opened::current.state="pinned";opened::current.held=true;
    need(run().ok&&events.empty(),"preload parent must never bind completed-world origin");
    reset();opened::current.completed_origin=false;need(run().ok&&events.empty(),"loading parent must never bind completed-world origin");
    reset();mt2mp::net::session_wire::members[2]=103;need(!run().ok,"ordinary membership drift rejected");need(run(true,true).ok,"recovery-local keeps original-cohort membership behavior");
    rejected([]{t::PageReadScope native(t::PageBoundary::OriginalCall);},"native_boundary_changed");
    rejected([]{t::original_call_revision=UINT64_MAX;},"native_boundary_changed");
    rejected([]{++opened::current.game_thread;},"wrong_thread");
    rejected([]{++opened::current.generation;},"generation_mismatch");
    rejected([]{++opened::world_epoch;},"origin_unavailable");
    rejected([]{opened::configured_root/="changed";},"owner_changed");opened::configured_root=owner;
    rejected([]{mt2mp::Config::instance().user_data_dir+="changed";},"verified profile binding changed");
    rejected([]{set_path(nullptr);},"PhysFS write directory unavailable");
    rejected([]{set_path(other_raw.c_str());},"actual write root changed");
    rejected([]{attribute_override=FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT;},"reparse or non-directory");
    rejected([]{need(f::fs::remove(account),"remove owned account");},"account directory unavailable");need(f::fs::create_directory(account),"restore owned account");
    rejected([]{need(f::fs::remove(account),"remove owned account");HANDLE file=CreateFileW(account.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);need(file!=INVALID_HANDLE_VALUE,"replace owned directory with file");need(CloseHandle(file)!=0,"close owned replacement file");},"reparse or non-directory");need(f::fs::remove(account)&&f::fs::create_directory(account),"restore directory");
    reset();copy_action=[]{if(opened::reads.size()==14)put(objects+0x900,0x158,static_cast<void*>(objects+0xe00));};need(!run().ok,"changed reciprocal map is copied and rejected");
    reset();copy_action=[]{if(opened::reads.size()==27)opened::callback_failure=true;};need(!run().ok,"callback failure remains visible");
    reset();metadata_action=[]{throw std::runtime_error("exact metadata exception");};bool caught=false;try{run();}catch(const std::exception& cause){caught=std::string(cause.what())=="exact metadata exception";}need(caught&&t::page_read_scope==nullptr,"metadata exception propagates and discards transaction");
    reset();throw_copy=true;caught=false;try{run();}catch(const std::exception& cause){caught=std::string(cause.what())=="exact field exception";}need(caught&&t::page_read_scope==nullptr,"copy exception propagates and restores page scope");reset();need(run().ok,"next proof after exception starts cleanly");
    reset();opened::ReadOriginTransaction token;need(mt2mp::game::native_open_runtime::read_current_world(world,error,token),"admit direct transaction");bool foreign=true;std::thread worker([&]{std::string cause;foreign=token.finish(cause);});worker.join();need(!foreign&&!token.finish(error),"foreign finish consumes token");
    reset();opened::ReadOriginTransaction once;need(once.finish(error)&&!once.finish(error)&&!mt2mp::game::native_open_runtime::read_current_world(world,error,once),"finished no-world token cannot be reused");
    for(const auto& mutation:std::vector<std::function<void()>>{
        []{++opened::current.game_thread;},[]{++opened::current.generation;},[]{++opened::world_epoch;},
        []{t::PageReadScope native(t::PageBoundary::OriginalCall);},[]{t::original_call_revision=UINT64_MAX;},
        []{mt2mp::Config::instance().runtime_root+="changed";},[]{set_path(nullptr);},
        []{attribute_override=FILE_ATTRIBUTE_NORMAL;}}){
        reset();opened::ReadOriginTransaction bound;
        need(mt2mp::game::native_open_runtime::read_current_world(world,error,bound),"admit before finish mutation");
        mutation();need(!bound.finish(error)&&!bound.finish(error),"finish rejects changed identity/origin and cannot be replayed");
    }
    reset();attribute_action=[]{if(std::count(events.begin(),events.end(),"attributes")==2)t::original_call_revision++;};need(!run().ok,"finish rechecks revision after live validator");
    reset();opened::ReadOriginTransaction unloading;
    need(mt2mp::game::native_open_runtime::read_current_world(world,error,unloading),"admit before PhysFS unload");
    need(FreeLibrary(module)!=0,"unload owned PhysFS fixture");
    need(!unloading.finish(error)&&error=="actual PhysFS write directory unavailable","finish must resolve live PhysFS module again");
    std::cout<<"PASS: read-origin transaction: "<<assertions<<" assertions; actual parent 4 -> 2 fresh attribute checks, 56 unchanged field copies; no global or TLS proof\n";return 0;
}catch(const std::exception& cause){std::cerr<<"error: read-origin transaction: "<<cause.what()<<'\n';return 1;}}
