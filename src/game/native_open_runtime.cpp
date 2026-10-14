#include "native_open_runtime.h"
#include "native_open_witness.h"
#include "mt2mp/hooks/hook_engine.h"
#include "mt2mp/core/logger.h"
#include <windows.h>
#include <atomic>
#include <array>
#include <limits>
#include <mutex>
#include <stdexcept>

namespace mt2mp::game::native_open_runtime {
namespace witness=building_mixed_identity::opened_file;
namespace {
witness::FileCtor original_file=nullptr;
witness::StreamCtor original_stream=nullptr,original_reader=nullptr;
using Worker=void(*)(void*);
Worker original_worker=nullptr;
std::atomic<uint64_t> active_generation{0};
uint64_t next_generation=0;
uint32_t created_hooks=0;
struct Observation {std::string operation,cause;uint64_t repeats=0;};
std::array<Observation,8> observations;
std::mutex observation_gate;

template<class Call> void original_once(const char* operation,Call&& call) {
    try { call(); }
    catch(const std::exception& exception) {
        witness::original_failed(operation,exception.what());
        LOG_ERROR("[NativeOpen] Native original failed operation={} cause={}; exception continues",operation,exception.what());
        throw;
    } catch(...) {
        witness::original_failed(operation,"non-standard native original exception");
        LOG_ERROR("[NativeOpen] Native original failed operation={} cause=non-standard exception; exception continues",operation);
        throw;
    }
}
void observation(const char* operation,bool accepted,const std::string& error) noexcept try {
    std::lock_guard lock(observation_gate);
    auto* entry=&observations.front();
    for(auto& candidate:observations) {
        if(candidate.operation==operation||candidate.operation.empty()){entry=&candidate;break;}
    }
    entry->operation=operation;
    if(accepted) {
        if(!entry->cause.empty())LOG_INFO("[NativeOpen] Observer recovered operation={} previous_cause={} repeats={}",operation,entry->cause,entry->repeats);
        entry->cause.clear();entry->repeats=0;return;
    }
    if(error.empty())return;
    if(entry->cause==error){++entry->repeats;return;}
    const auto repeats=entry->repeats;entry->cause=error;entry->repeats=1;
    if(error.starts_with("info:"))LOG_INFO("[NativeOpen] Observer skipped operation={} cause={} previous_repeats={}; original continues",operation,error,repeats);
    else LOG_WARN("[NativeOpen] Observer rejected operation={} cause={} previous_repeats={}; original continues",operation,error,repeats);
} catch(const std::exception& exception) {
    OutputDebugStringA("[NativeOpen] error operation=observer_log cause=");
    OutputDebugStringA(exception.what());
    OutputDebugStringA("; native original continues\n");
} catch(...) {
    OutputDebugStringA("[NativeOpen] error operation=observer_log cause=non-standard exception; native original continues\n");
}
void file_ctor(void* file,const void* name,int32_t mode) {
    std::string error;
    const bool accepted=witness::file_before(file,name,mode,error);
    observation("file_before",accepted,error);
    original_once("file",[&]{original_file(file,name,mode);});
    if(accepted)observation("file_after",witness::file_after(file,error),error);
}
void stream_ctor(void* stream,void* file) {
    std::string error;
    const bool accepted=witness::stream_before(stream,file,error);
    observation("stream_before",accepted,error);
    original_once("stream",[&]{original_stream(stream,file);});
    if(accepted)observation("stream_after",witness::stream_after(stream,file,error),error);
}
void reader_ctor(void* reader,void* stream) {
    std::string error;
    const bool accepted=witness::reader_before(reader,stream,error);
    observation("reader_before",accepted,error);
    original_once("reader",[&]{original_reader(reader,stream);});
    if(accepted)observation("reader_after",witness::reader_after(reader,stream,error),error);
}
void worker_run(void* task) {
    const auto generation=active_generation.load();
    std::string error;
    const bool accepted=witness::worker_before(task,generation,error);
    observation("worker_before",accepted,error);
    original_once("worker",[&]{original_worker(task);});
    if(accepted)observation("worker_after",witness::worker_after(task,generation,error),error);
}
}
bool initialize(const std::filesystem::path& root,uintptr_t image,std::string& error) {
    if(!witness::initialize(root,error))return false;
    struct Entry {uintptr_t rva;void* detour;void** original;uint32_t bit;};
    const Entry entries[]={
        {0x95ec00,reinterpret_cast<void*>(&file_ctor),reinterpret_cast<void**>(&original_file),1},
        {0xa02d10,reinterpret_cast<void*>(&stream_ctor),reinterpret_cast<void**>(&original_stream),2},
        {0x96bbd0,reinterpret_cast<void*>(&reader_ctor),reinterpret_cast<void**>(&original_reader),4},
        {0xbd7a80,reinterpret_cast<void*>(&worker_run),reinterpret_cast<void**>(&original_worker),8},
    };
    for(const auto& entry:entries) {
        if(!hooks::create_hook(reinterpret_cast<void*>(image+entry.rva),entry.detour,entry.original)) {
            error="native open hook creation failed RVA="+std::to_string(entry.rva)+" mask="+std::to_string(created_hooks);
            witness::hooks_stopped(error);return false;
        }
        created_hooks|=entry.bit;
    }
    return true;
}
bool hooks_enabled(std::string& error) {
    const bool confirmed=witness::confirm_hooks(created_hooks,error);
    if(confirmed)LOG_INFO("[NativeOpen] Hooks enabled mask={} file stream reader worker",created_hooks);
    return confirmed;
}
bool prepare(const std::filesystem::path& root,const std::string& uuid,const std::string& sha,
             uint64_t bytes,uint64_t& generation,std::string& error,const std::string& world_name) {
    using GetWriteDir=const char*(*)();
    const auto module=GetModuleHandleW(L"libphysfs.dll");
    const auto getter=module?reinterpret_cast<GetWriteDir>(GetProcAddress(module,"PHYSFS_getWriteDir")):nullptr;
    const auto* actual=getter?getter():nullptr;
    if(!actual||!*actual){error="actual PhysFS write directory unavailable";return false;}
    auto write=std::filesystem::weakly_canonical(actual);
    if(write.filename().empty())write=write.parent_path();
    const auto parent=std::filesystem::weakly_canonical(root/"data");
    if(write.parent_path()!=parent){error="actual PhysFS write directory outside own data/SteamID";return false;}
    const auto steam_id=write.filename().string();
    if(next_generation==std::numeric_limits<uint64_t>::max()){error="native open generation exhausted";return false;}
    const auto next=++next_generation;
    if(!witness::begin(root,steam_id,uuid,sha,bytes,next,error,world_name))return false;
    generation=next;active_generation.store(next);
    LOG_INFO("[NativeOpen] File pinned uuid={} generation={} bytes={}",uuid,next,bytes);
    return true;
}
bool before_request(uint64_t generation,std::string& error) {return witness::note_request_attempt(generation,error);}
bool retire_unstarted(uint64_t generation,std::string& error) {return witness::retire_unstarted(generation,error);}
bool finish(uint64_t generation,bool& pending,std::string& error) {
    pending=false;
    const auto retired=witness::retire(generation,error);
    if(retired==witness::Retirement::Pending){pending=true;return true;}
    if(retired==witness::Retirement::Rejected)return false;
    const auto before=witness::status();
    if(!before.world_witness_bound&&!witness::seal_loaded_world(generation,error))return false;
    return witness::validate_completed_origin(generation,error);
}
bool read_current_world(witness::CurrentWorld& result,std::string& error) {
    result={};
    const auto generation=active_generation.load();
    if(!witness::read_completed_world(generation,result,error))return false;
    if(active_generation.load()!=generation) {result={};error="current_world_read_runtime_generation_changed";return false;}
    return true;
}
nlohmann::json status_json() {
    const auto s=witness::status();
    return {{"state",s.state},{"uuid",s.uuid},{"sha256",s.sha256},{"generation",s.generation},
        {"bytes",s.bytes},{"hash_bytes",s.hash_bytes},{"initialized",s.initialized},{"hook_mask",s.hook_mask},
        {"world_epoch",s.world_epoch},{"origin_world_epoch",s.origin_world_epoch},
        {"file_calls",s.file_calls},{"file_returns",s.file_returns},{"stream_calls",s.stream_calls},{"stream_returns",s.stream_returns},
        {"reader_calls",s.reader_calls},{"reader_returns",s.reader_returns},{"worker_calls",s.worker_calls},{"worker_returns",s.worker_returns},
        {"foreign_files",s.foreign_files},{"foreign_streams",s.foreign_streams},{"foreign_readers",s.foreign_readers},
        {"thread",s.thread},{"game_thread",s.game_thread},{"terminal_mode_state",s.terminal_mode_state},
        {"native_request_attempted",s.native_request_attempted},{"native_task_retired",s.native_task_retired},
        {"held",s.held},{"origin_confirmed",s.origin_confirmed},{"reader_confirmed",s.reader_confirmed},
        {"completed_origin",s.completed_origin},{"world_witness_bound",s.world_witness_bound},{"current_origin_available",s.current_origin_available},
        {"unarmed_task_runs",s.unarmed_task_runs},{"world_checks",s.world_checks},
        {"pin_close_calls",s.pin_close_calls},{"pin_close_confirmed",s.pin_close_confirmed},
        {"original_failures",s.original_failures},{"original_error",s.original_error},{"error",s.error},
        {"retirement_wait",s.retirement_wait},{"retirement_observations",s.retirement_observations},
        {"retirement_wait_changes",s.retirement_wait_changes},{"retirement_recoveries",s.retirement_recoveries},
        {"callback_error_truncated",s.callback_error_truncated},{"full_coop_proven",false}};
}
void shutdown(const std::string& cause) {witness::hooks_stopped(cause);}
}
