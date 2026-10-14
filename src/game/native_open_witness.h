#pragma once
#include <cstdint>
#include <string>
#include <filesystem>

namespace mt2mp::game::building_mixed_identity::opened_file {
using FileCtor = void(*)(void*,const void*,int32_t);
using StreamCtor = void(*)(void*,void*);
using Lookup = const char*(*)(const char*);
constexpr uint32_t required_hooks=15;
struct Status {
    std::string state="idle",uuid,sha256,error,original_error;
    uint64_t generation=0,bytes=0,hash_bytes=0;
    uint64_t world_epoch=0,origin_world_epoch=0;
    uint32_t thread=0;
    uint64_t file_calls=0,file_returns=0,stream_calls=0,stream_returns=0,reader_calls=0,reader_returns=0;
    uint64_t foreign_files=0,foreign_streams=0,foreign_readers=0,original_failures=0;
    uint64_t pin_close_calls=0,pin_close_confirmed=0;
    uint64_t worker_calls=0,worker_returns=0;
    uint32_t game_thread=0;
    int32_t terminal_mode_state=-1;
    bool native_request_attempted=false,native_task_retired=false;
    bool completed_origin=false,current_origin_available=false,world_witness_bound=false;
    uint64_t unarmed_task_runs=0,world_checks=0;
    uint64_t retirement_observations=0,retirement_wait_changes=0,retirement_recoveries=0;
    std::string retirement_wait;
    bool held=false,origin_confirmed=false,reader_confirmed=false,full_coop_proven=false;
    bool callback_error_truncated=false;
    bool initialized=false;
    uint32_t hook_mask=0;
};
bool initialize(const std::filesystem::path& runtime_root,std::string& error);
bool confirm_hooks(uint32_t installed_mask,std::string& error);
void hooks_stopped(const std::string& cause);
bool begin(const std::filesystem::path& runtime_root,const std::string& steam_id,
           const std::string& uuid,const std::string& sha,uint64_t bytes,uint64_t generation,std::string& error);
// These guards observe the caller's original invocation. A rejection must not skip the original.
bool file_before(void* file,const void* gcc_filename,int32_t mode,std::string& error) noexcept;
bool file_after(void* file,std::string& error) noexcept;
bool stream_before(void* stream,void* file,std::string& error) noexcept;
bool stream_after(void* stream,void* file,std::string& error) noexcept;
bool reader_before(void* reader,void* stream,std::string& error) noexcept;
bool reader_after(void* reader,void* stream,std::string& error) noexcept;
bool validate_reader(void* reader,uint64_t generation,std::string& error) noexcept;
bool declared_children(void* reader,uint64_t generation,int32_t& count,std::string& error) noexcept;
enum class ArrayKind { Building, DungeonInstance };
bool array_record(void* reader,uint64_t generation,ArrayKind kind,int32_t& count,std::string& error) noexcept;
bool note_request_attempt(uint64_t generation,std::string& error);
bool worker_before(void* init_task,uint64_t generation,std::string& error) noexcept;
bool worker_after(void* init_task,uint64_t generation,std::string& error) noexcept;
void original_failed(const std::string& operation,const std::string& cause) noexcept;
enum class Retirement { Pending, Closed, Rejected };
// A same-task full Run return, cleared context and native Complete flag are required.
Retirement retire(uint64_t generation,std::string& error);
bool retire_unstarted(uint64_t generation,std::string& error);
bool seal_loaded_world(uint64_t generation,std::string& error);
bool validate_completed_origin(uint64_t generation,std::string& error);
// These receivers remain private to the current process and bound Mainthread.
struct CurrentWorld {
    void* game=nullptr;
    void* mode=nullptr;
    void* state=nullptr;
    void* world=nullptr;
    void* map=nullptr;
    uint64_t generation=0,world_epoch=0,bytes=0;
    std::string uuid,sha256;
};
bool read_completed_world(uint64_t generation,CurrentWorld&,std::string& error);
Status status();
#ifdef MT2MP_MIXED_OPEN_TEST
void testing_backend(uintptr_t image,Lookup real_dir,Lookup mount_point);
void testing_mode(void* mode);
void testing_world(void* game,void* world,void* map);
bool testing_same_file(const std::filesystem::path& candidate,std::string& error);
void testing_callback_failure(const char* cause);
bool testing_entry_guard(uintptr_t rva,const unsigned char* bytes,size_t count,std::string& error);
#endif
}
