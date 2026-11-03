#define main retained_origin_fixture_main
#include "test_read_origin_transaction.cpp"
#undef main
#include "typed_action/action_session.h"
#include "mt2mp/core/hash.h"

std::function<void()> progress_action;
namespace mt2mp::net {
bool snapshot_failed=false,snapshot_world_valid=true,ordered_load_acknowledged=true;
bool receiving=false,load_pending=false;
std::string outbound_id,last_loaded;
uint64_t incoming_epoch=0;
bool snapshot_host(){return true;}
bool valid_id(const std::string& value){return !value.empty();}
nlohmann::json snapshot_progress(){
    if(progress_action)progress_action();
    return {{"phase","active"},{"world_authority_valid",snapshot_world_valid&&!snapshot_failed}};
}
#include "ordered_local_epoch.inc"
}
namespace mt2mp::action_session {
#include "ordered_local_parent.inc"
}
namespace action=mt2mp::action_session;
struct LocalOutcome {bool ok=false;action::Context context{};std::string cause;};
void local_reset(){
    reset();progress_action={};
    opened::configured_root=owner;opened::configured_write=account;opened::physical=account/"fixture.sav";
    mt2mp::net::snapshot_failed=false;mt2mp::net::snapshot_world_valid=true;
    mt2mp::net::ordered_load_acknowledged=true;
}
LocalOutcome local_run(bool candidate=true){
    LocalOutcome result;
    result.ok=candidate?action::ordered_local_context(result.context,result.cause):
        action::baseline_ordered_local_context(result.context,result.cause);
    need(t::page_read_scope==nullptr,"local parent leaked PageReadScope");return result;
}
int main(int argc,char** argv){try{
    need(argc==1,"fixture takes no arguments");
    owner=f::fs::current_path()/("ordered_local_fixture_"+std::to_string(GetCurrentProcessId()));
    need(f::fs::create_directory(owner),"fresh fixture directory required");
    f::fs::create_directories(owner/"game");account=owner/"profile/game-data/76561198013473990";
    f::fs::create_directories(account);f::fs::create_directories(account.parent_path()/"123");
    auto& config=mt2mp::Config::instance();config.framework_profile_verified=true;config.local_test=true;config.diagnostic_mailbox_enabled=true;
    config.runtime_root=p::utf8(owner);config.game_dir=p::utf8(owner/"game");config.framework_profile_directory=p::utf8(owner/"profile");config.user_data_dir=p::utf8(owner/"profile/game-data");
    original_config=config;opened::configured_root=owner;opened::configured_paths=f::bind(owner);opened::configured_write=account;opened::physical=account/"fixture.sav";
    const auto module=LoadLibraryW((f::fs::absolute(argv[0]).parent_path()/"libphysfs.dll").c_str());need(module!=nullptr,"PhysFS fixture load failed");
    set_path=reinterpret_cast<decltype(set_path)>(GetProcAddress(module,"fixture_set_path"));need(set_path!=nullptr,"PhysFS setter missing");raw=p::utf8(account);other_raw=p::utf8(account.parent_path()/"123");
    struct Memory {void* image=nullptr;void* objects=nullptr;~Memory(){for(auto value:{image,objects})if(value&&!VirtualFree(value,0,MEM_RELEASE))std::cerr<<"error: fixture memory release "<<GetLastError()<<'\n';}} memory;
    memory.image=VirtualAlloc(nullptr,0x13e0000,MEM_RESERVE,PAGE_NOACCESS);need(memory.image!=nullptr,"reserve image");opened::image=reinterpret_cast<uintptr_t>(memory.image);
    need(VirtualAlloc(reinterpret_cast<void*>(opened::image+0x13df000),4096,MEM_COMMIT,PAGE_READWRITE)!=nullptr,"commit image singleton page");
    memory.objects=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);need(memory.objects!=nullptr,"allocate objects");objects=static_cast<unsigned char*>(memory.objects);
    local_reset();const auto baseline=local_run(false);const auto baseline_reads=opened::reads;
    need(baseline.ok&&opened::reads.size()==56&&std::count(events.begin(),events.end(),"attributes")==4,"baseline local parent requires four origin validations and 56 field copies");
    local_reset();t::totals={};LocalOutcome candidate;
    {t::Scope scope(t::Part::Take);candidate=local_run();}
    need(candidate.ok&&candidate.context==baseline.context,"local Context changed");
    need(opened::reads==baseline_reads&&opened::current.world_checks==4,"local parent must preserve all four fresh world passes and 56 field copies");
    need(std::count(events.begin(),events.end(),"attributes")==2&&events.front()=="attributes"&&events.back()=="attributes","local parent entry and finish must validate origin");
    need(t::totals[size_t(t::Part::NativeOpenFrameworkBinding)].count==2&&t::totals[size_t(t::Part::NativeOpenWorldFields)].count==4,"local parent counted work differs");
    local_reset();need(local_run().ok&&local_run().ok&&std::count(events.begin(),events.end(),"attributes")==4,"independent contexts validate independently");
    local_reset();bool nested=false;progress_action=[&]{if(nested)return;nested=true;need(local_run().ok,"nested independent context");nested=false;};
    need(local_run().ok&&opened::reads.size()==112&&std::count(events.begin(),events.end(),"attributes")==4,"nested contexts must not borrow transactions");
    local_reset();progress_action=[]{const auto result=run();need(result.ok,"independent proof inside context");};
    need(local_run().ok&&opened::reads.size()==112&&std::count(events.begin(),events.end(),"attributes")==4,"proof and context lifetimes stay independent");
    local_reset();need(mt2mp::net::current_world_epoch()!=0&&mt2mp::net::current_world_epoch()!=0&&std::count(events.begin(),events.end(),"attributes")==4,"public epoch keeps fresh independent validations");
    const std::vector<std::function<void()>> changes={
        []{++opened::current.game_thread;},[]{++opened::current.generation;},[]{++opened::world_epoch;},
        []{++mt2mp::game::native_open_runtime::active_generation;},[]{t::PageReadScope native(t::PageBoundary::OriginalCall);},
        []{t::original_call_revision=UINT64_MAX;},[]{opened::configured_root/="changed";},
        []{opened::configured_write/="changed";},[]{opened::physical/="changed";},
        []{mt2mp::Config::instance().runtime_root+="changed";},[]{set_path(nullptr);},[]{set_path(other_raw.c_str());},
        []{attribute_override=FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT;},
        []{opened::callback_failure=true;},[]{++opened::current.origin_world_epoch;},[]{opened::installed_hooks=0;},
        []{opened::current.state="failed";},[]{opened::current.original_error="native failure";},
        []{opened::current.current_origin_available=false;},[]{mt2mp::net::session_wire::members[2]=103;},
        []{mt2mp::net::session_wire::online=false;},[]{mt2mp::net::snapshot_failed=true;},
        []{mt2mp::net::snapshot_world_valid=false;},[]{mt2mp::net::ordered_load_acknowledged=false;},
        []{put(objects+0x900,0x158,static_cast<void*>(objects+0xe00));}};
    for(const auto& mutation:changes){
        local_reset();opened::configured_root=owner;progress_action=mutation;
        const auto result=local_run();need(!result.ok&&!result.cause.empty()&&result.context==action::Context{},"changed parent state must fail before returning Context");
    }
    opened::configured_root=owner;
    local_reset();progress_action=[]{need(f::fs::remove(account),"remove owned account directory");};need(!local_run().ok,"removed account rejected at checked finish");need(f::fs::create_directory(account),"restore account directory");
    local_reset();progress_action=[]{need(f::fs::remove(account),"remove owned account directory");HANDLE file=CreateFileW(account.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);need(file!=INVALID_HANDLE_VALUE,"replace owned directory with file");need(CloseHandle(file)!=0,"close owned replacement file");};need(!local_run().ok,"replaced account rejected at checked finish");need(f::fs::remove(account)&&f::fs::create_directory(account),"restore account directory");
    local_reset();copy_action=[]{if(opened::reads.size()==14)put(objects+0x900,0x158,static_cast<void*>(objects+0xe00));};need(!local_run().ok,"first reciprocal map change rejected");
    local_reset();copy_action=[]{if(opened::reads.size()==42)put(objects+0x900,0x158,static_cast<void*>(objects+0xe00));};need(!local_run().ok,"second reciprocal map change rejected");
    local_reset();attribute_action=[]{if(std::count(events.begin(),events.end(),"attributes")==2)++t::original_call_revision;};need(!local_run().ok,"finish validates revision after origin validator");
    local_reset();progress_action=[]{throw std::runtime_error("exact progress exception");};bool caught=false;try{local_run();}catch(const std::exception& error){caught=std::string(error.what())=="exact progress exception";}need(caught&&t::page_read_scope==nullptr,"progress exception preserved");
    local_reset();throw_copy=true;caught=false;try{local_run();}catch(const std::exception& error){caught=std::string(error.what())=="exact field exception";}need(caught&&t::page_read_scope==nullptr,"field exception preserved");
    local_reset();need(local_run().ok,"parent recovers with a new independent transaction after exception");
    local_reset();opened::current.completed_origin=false;need(!local_run().ok&&opened::reads.empty(),"loading world remains unavailable without native copies");
    local_reset();bool foreign=true;std::thread thread([&]{foreign=local_run().ok;});thread.join();need(!foreign,"wrong thread rejected");
    local_reset();progress_action=[&]{need(FreeLibrary(module)!=0,"unload owned PhysFS fixture");};need(!local_run().ok,"module unload rejected at finish");
    std::cout<<"PASS: ordered local context: "<<assertions<<" assertions; 4 -> 2 origin validations, 56 identical field copies; independent callers remain fresh\n";
    return 0;
}catch(const std::exception& cause){std::cerr<<"error: ordered local context: "<<cause.what()<<'\n';return 1;}}
