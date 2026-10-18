#include "native_open_witness.h"
#include "mt2mp/net/snapshot_world_path.h"
#ifdef MT2MP_FRAMEWORK_PLUGIN
#include "mt2mp/core/framework_open_paths.h"
#endif
#ifndef MT2MP_MIXED_OPEN_TEST
#include "mt2mp/core/installed_profile.h"
#endif
#include "mt2mp/game/native_engine.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "live_host_state/memory_query_timing.h"
#include <bcrypt.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>
#include <utility>
#include <cstdio>

namespace mt2mp::game::building_mixed_identity::opened_file {
namespace {
std::mutex gate;
Status current;
HANDLE held=INVALID_HANDLE_VALUE;
uintptr_t image=0;
Lookup real_dir=nullptr,mount_point=nullptr;
std::filesystem::path configured_root,physical;
#ifdef MT2MP_FRAMEWORK_PLUGIN
framework_open_paths::Binding configured_paths;
std::filesystem::path configured_write;
DWORD WINAPI timed_account_attributes(LPCWSTR path) {
    const auto incoming=GetLastError();
    DWORD attributes=0,error=0;
    {
        mt2mp::game::live_host_state::apply_timing::Scope timing(
            mt2mp::game::live_host_state::apply_timing::Part::NativeOpenAccountAttributes);
        SetLastError(incoming);
        attributes=GetFileAttributesW(path);
        error=GetLastError();
    }
    SetLastError(error);
    return attributes;
}
bool framework_binding(std::string& error,framework_open_paths::AttributeReader& read_attributes=GetFileAttributesW) {
    try {
        const auto actual=framework_open_paths::live_write(configured_paths,read_attributes);
        if(configured_write.empty()||!private_profile::equal(actual,configured_write)) {
            error="Framework native open actual write root changed during bound origin";return false;
        }
        return true;
    }
    catch(const std::exception& problem) {error=problem.what();return false;}
}
#endif
std::string virtual_name;
void* native_file=nullptr;void* native_stream=nullptr;void* native_reader=nullptr;
void* native_task=nullptr;void* native_mode=nullptr;
void* mode_override=nullptr;
bool worker_pending=false;
struct WorldWitness {void* game=nullptr;void* mode=nullptr;void* state=nullptr;void* world=nullptr;void* map=nullptr;bool operator==(const WorldWitness&)const=default;};
WorldWitness loaded_world;
void* game_override=nullptr;void* world_override=nullptr;void* map_override=nullptr;
bool file_pending=false,stream_pending=false,reader_pending=false;
uint64_t retired_generation=0;
uint64_t world_epoch=0;
bool tested=false;
enum class InitPhase {Uninitialized,Ready,Failed};
InitPhase init_phase=InitPhase::Uninitialized;
std::string init_error;
uint32_t installed_hooks=0;
constexpr char stream_ctor_entry[]="\x56\x53\x48\x83\xec\x28\x48\x8d\x05\xa3\xe8\xd2\x00";
constexpr char reader_ctor_entry[]="\x55\x57\x56\x53\x48\x83\xec\x28";
std::atomic<bool> callback_failure=false;
std::atomic_flag emergency_gate=ATOMIC_FLAG_INIT;
std::array<char,384> emergency_cause{};
bool emergency_truncated=false;
bool emergency(const char* cause) noexcept {
    while(emergency_gate.test_and_set(std::memory_order_acquire)){}
    if(!callback_failure.load()){
        size_t n=0;while(n+1<emergency_cause.size()&&cause[n]){emergency_cause[n]=cause[n];++n;}
        emergency_cause[n]='\0';emergency_truncated=cause[n]!='\0';callback_failure.store(true);
    }
    emergency_gate.clear(std::memory_order_release);
    OutputDebugStringA("[MixedOpen] error operation=callback cause=");OutputDebugStringA(cause);
    OutputDebugStringA("; witness unconfirmed, native original unchanged\n");return false;
}
bool fail(std::string& error,const std::string& cause) {
    error=cause;
    if(current.state!="failed") {
        current.error=cause;current.state="failed";current.origin_confirmed=false;current.reader_confirmed=false;
        current.current_origin_available=false;
        const auto line="[MixedOpen] error operation=observe uuid="+current.uuid+" cause="+cause+"; native original unchanged\n";
        OutputDebugStringA(line.c_str());
    }
    return false;
}
bool init_fail(std::string& error,const std::string& cause) {
    init_phase=InitPhase::Failed;init_error=cause;installed_hooks=0;
    current.current_origin_available=false;return fail(error,cause);
}
bool readable(const void* pointer,size_t bytes) {
    MEMORY_BASIC_INFORMATION m{};
    if(!pointer||!bytes||!mt2mp::game::live_host_state::apply_timing::virtual_query(pointer,&m,sizeof(m),mt2mp::game::live_host_state::apply_timing::Part::QueryOpenWitness)||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const auto p=reinterpret_cast<uintptr_t>(pointer),base=reinterpret_cast<uintptr_t>(m.BaseAddress);
    return p>=base&&bytes<=m.RegionSize&&p-base<=m.RegionSize-bytes;
}
DWORD copy(const void* from,void* to,size_t bytes) {
    __try {std::memcpy(to,from,bytes);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
template<class T> bool read(const void* object,size_t offset,T& value,std::string& error) {
    const auto p=reinterpret_cast<uintptr_t>(object);
    if(!object||p>UINTPTR_MAX-offset||!readable(reinterpret_cast<void*>(p+offset),sizeof(T)))
        return fail(error,"field_unreadable offset="+std::to_string(offset));
    const auto ex=copy(reinterpret_cast<void*>(p+offset),&value,sizeof(T));
    return ex?fail(error,"field_exception offset="+std::to_string(offset)+" code="+std::to_string(ex)):true;
}
bool vt(void* object,uintptr_t rva,std::string& error) {
    uintptr_t actual=0;
    return read(object,0,actual,error)&&(actual==image+rva||fail(error,"vtable_mismatch required="+std::to_string(rva)));
}
bool gcc_string(const void* string,std::string& result,std::string& error) {
    void* p=nullptr;size_t n=0;
    if(!read(string,0,p,error)||!read(string,8,n,error))return false;
    if(!n||n>32767||!readable(p,n+1))return fail(error,"gcc_filename_bounds bytes="+std::to_string(n)+" diagnostic_max=32767");
    result.resize(n+1);
    const auto ex=copy(p,result.data(),n+1);
    if(ex)return fail(error,"gcc_filename_exception code="+std::to_string(ex));
    if(result[n]!='\0'||result.find('\0')!=n)return fail(error,"gcc_filename_terminator");
    result.resize(n);return true;
}
DWORD lookup_call(Lookup fn,const char* arg,const char*& result) {
    __try {result=fn(arg);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool lookup_string(Lookup fn,const std::string& arg,std::string& result,std::string& error) {
    const char* pointer=nullptr;
    const auto ex=lookup_call(fn,arg.c_str(),pointer);
    if(ex||!pointer)return fail(error,"physfs_lookup_failed code="+std::to_string(ex));
    result.clear();
    for(size_t i=0;i<32768;++i){char c=0;if(!read(pointer,i,c,error))return false;if(!c)return true;result.push_back(c);}
    return fail(error,"physfs_lookup_string diagnostic_max=32767");
}
bool path_equal(const std::filesystem::path& a,const std::filesystem::path& b) {
    const auto x=a.lexically_normal().wstring(),y=b.lexically_normal().wstring();
    return CompareStringOrdinal(x.c_str(),int(x.size()),y.c_str(),int(y.size()),TRUE)==CSTR_EQUAL;
}
std::filesystem::path absolute_path(const std::filesystem::path& path) {
    if(!path.is_absolute())throw std::runtime_error("owned path must be absolute");
    return path.lexically_normal();
}
bool no_reparse(const std::filesystem::path& path,std::string& error) {
    const auto attrs=GetFileAttributesW(path.c_str());
    if(attrs==INVALID_FILE_ATTRIBUTES)return fail(error,"owned_route_attributes_failed winerror="+std::to_string(GetLastError()));
    return !(attrs&FILE_ATTRIBUTE_REPARSE_POINT)||fail(error,"owned_route_reparse_rejected");
}
bool same_file_object(const std::filesystem::path& candidate,std::string& error) {
    const auto other=CreateFileW(candidate.c_str(),FILE_READ_ATTRIBUTES,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(other==INVALID_HANDLE_VALUE)return fail(error,"resolved_file_identity_open winerror="+std::to_string(GetLastError()));
    BY_HANDLE_FILE_INFORMATION a{},b{};
    const auto a_ok=GetFileInformationByHandle(held,&a);const auto a_error=a_ok?0:GetLastError();
    const auto b_ok=GetFileInformationByHandle(other,&b);const auto b_error=b_ok?0:GetLastError();
    const auto closed=CloseHandle(other);const auto close_error=closed?0:GetLastError();
    if(!closed)return fail(error,"resolved_file_identity_close winerror="+std::to_string(close_error));
    if(!a_ok||!b_ok)return fail(error,"resolved_file_identity_query pinned_winerror="+std::to_string(a_error)+" resolved_winerror="+std::to_string(b_error));
    if(a.dwVolumeSerialNumber!=b.dwVolumeSerialNumber||a.nFileIndexHigh!=b.nFileIndexHigh||a.nFileIndexLow!=b.nFileIndexLow)
        return fail(error,"native_open_file_object_identity_mismatch");
    if(a.nFileSizeHigh!=b.nFileSizeHigh||a.nFileSizeLow!=b.nFileSizeLow||
       a.ftLastWriteTime.dwHighDateTime!=b.ftLastWriteTime.dwHighDateTime||a.ftLastWriteTime.dwLowDateTime!=b.ftLastWriteTime.dwLowDateTime)
        return fail(error,"native_open_file_object_metadata_changed");
    return true;
}
std::filesystem::path utf8_path(const std::string& value) {
    const auto count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.c_str(),int(value.size()),nullptr,0);
    if(!count)throw std::runtime_error("native VFS path UTF-8 conversion error="+std::to_string(GetLastError()));
    std::wstring wide(size_t(count),L'\0');
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.c_str(),int(value.size()),wide.data(),count)!=count)
        throw std::runtime_error("native VFS path UTF-8 conversion failed");
    return std::filesystem::path(wide);
}
bool mapped_file(std::string& error) {
#ifdef MT2MP_FRAMEWORK_PLUGIN
    if(!framework_binding(error))return fail(error,error);
#endif
    std::string source,mount,again;
    if(!lookup_string(real_dir,virtual_name,source,error)||!lookup_string(mount_point,source,mount,error)||
       !lookup_string(real_dir,virtual_name,again,error))return false;
    if(source!=again)return fail(error,"physfs_mount_source_changed");
    std::string relative=virtual_name;
    if(!mount.empty()) {
        if(mount.back()!='/')mount.push_back('/');
        if(relative.rfind(mount,0)!=0)return fail(error,"physfs_mount_prefix_mismatch");
        relative.erase(0,mount.size());
    }
    const auto resolved=absolute_path(utf8_path(source)/utf8_path(relative));
    if(!path_equal(resolved,physical))return fail(error,"native_open_physical_file_mismatch");
    if(!no_reparse(resolved,error))return false;
    return same_file_object(resolved,error);
}
bool string_format(const std::string& value,bool uuid) {
    if(value.size()!=(uuid?36u:64u))return false;
    for(size_t i=0;i<value.size();++i){const auto c=value[i];
        if(uuid&&(i==8||i==13||i==18||i==23)){if(c!='-')return false;}
        else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    }return true;
}
bool sha_handle(HANDLE file,std::string& sha,uint64_t& bytes,std::string& error) {
    std::vector<unsigned char> object;
    BCRYPT_ALG_HANDLE alg=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    struct Cleanup {BCRYPT_ALG_HANDLE& a;BCRYPT_HASH_HANDLE& h;~Cleanup(){
        std::array<char,180> message{};
        if(h){const auto rc=BCryptDestroyHash(h);if(rc){std::snprintf(message.data(),message.size(),"[MixedOpen] warn operation=hash-cleanup cause=BCryptDestroyHash ntstatus=%ld\n",static_cast<long>(rc));OutputDebugStringA(message.data());}}
        if(a){const auto rc=BCryptCloseAlgorithmProvider(a,0);if(rc){std::snprintf(message.data(),message.size(),"[MixedOpen] warn operation=hash-cleanup cause=BCryptCloseAlgorithmProvider ntstatus=%ld\n",static_cast<long>(rc));OutputDebugStringA(message.data());}}}} cleanup{alg,hash};
    auto rc=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,nullptr,0);
    if(rc)return fail(error,"hash_open ntstatus="+std::to_string(rc));
    DWORD object_bytes=0,result_bytes=0;
    rc=BCryptGetProperty(alg,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&object_bytes),sizeof(object_bytes),&result_bytes,0);
    if(rc||!object_bytes||object_bytes>65536)return fail(error,"hash_object_property ntstatus="+std::to_string(rc)+" actual="+std::to_string(object_bytes));
    object.resize(object_bytes);
    rc=BCryptCreateHash(alg,&hash,object.data(),object_bytes,nullptr,0,0);
    if(rc)return fail(error,"hash_create ntstatus="+std::to_string(rc));
    LARGE_INTEGER zero{};
    if(!SetFilePointerEx(file,zero,nullptr,FILE_BEGIN))return fail(error,"hash_seek winerror="+std::to_string(GetLastError()));
    std::array<unsigned char,65536> buffer{};bytes=0;
    for(;;){DWORD got=0;
        if(!ReadFile(file,buffer.data(),DWORD(buffer.size()),&got,nullptr))return fail(error,"hash_read winerror="+std::to_string(GetLastError()));
        if(!got)break;
        if(bytes>UINT64_MAX-got)return fail(error,"hash_byte_overflow");
        bytes+=got;rc=BCryptHashData(hash,buffer.data(),got,0);
        if(rc)return fail(error,"hash_update ntstatus="+std::to_string(rc));
    }
    std::array<unsigned char,32> digest{};rc=BCryptFinishHash(hash,digest.data(),DWORD(digest.size()),0);
    if(rc)return fail(error,"hash_finish ntstatus="+std::to_string(rc));
    sha.clear();for(auto c:digest){sha.push_back("0123456789abcdef"[c>>4]);sha.push_back("0123456789abcdef"[c&15]);}
    return true;
}
bool chain(std::string& error) {
    void* file=nullptr;int32_t mode=-1;std::string name;
    if(!native_file||!vt(native_file,0x1742fd0,error)||!read(native_file,0x70,mode,error)||mode!=5)
        return fail(error,"native_file_mode_or_type required_mode=5");
    if(!gcc_string(reinterpret_cast<unsigned char*>(native_file)+8,name,error)||name!=virtual_name)
        return fail(error,"native_file_filename_changed");
    if(native_stream&&(!vt(native_stream,0x17315c0,error)||!read(native_stream,0x18,file,error)||file!=native_file))
        return fail(error,"native_stream_file_changed");
    return mapped_file(error);
}
bool active(std::string& error) {
    if(init_phase!=InitPhase::Ready||installed_hooks!=required_hooks)return fail(error,"native_open_initialization_or_hook_receipt_unavailable");
    if(callback_failure.load()||held==INVALID_HANDLE_VALUE||current.state=="failed")return fail(error,"native_open_witness_unavailable previous="+current.error);
    if(current.thread&&current.thread!=GetCurrentThreadId())return fail(error,"native_open_thread_changed");
    return true;
}
bool mode_singleton(void*& mode,std::string& error) {
#ifdef MT2MP_MIXED_OPEN_TEST
    if(mode_override){mode=mode_override;return true;}
#endif
    return read(reinterpret_cast<void*>(image),0x13df320,mode,error);
}
bool task_owner(void* task,void*& mode,std::string& error) {
    void* singleton=nullptr;void* owned=nullptr;int32_t type=-1;unsigned char auto_delete=1;
    if(!vt(task,0x17013c0,error)||!read(task,8,type,error)||type!=2||!read(task,0xd,auto_delete,error)||auto_delete!=0||
       !read(task,0x10,mode,error)||!mode_singleton(singleton,error)||mode!=singleton||!vt(mode,0x16e6730,error)||
       !read(mode,0x2d0,owned,error)||owned!=task)return fail(error,"mode_init_task_owner_type_or_lifetime_mismatch");
    return true;
}
bool world_witness(WorldWitness& w,std::string& error) {
#ifdef MT2MP_FRAMEWORK_PLUGIN
    {
        mt2mp::game::live_host_state::apply_timing::Scope timing(
            mt2mp::game::live_host_state::apply_timing::Part::NativeOpenFrameworkBinding);
        if(!framework_binding(error,timed_account_attributes))return fail(error,error);
    }
#endif
    mt2mp::game::live_host_state::apply_timing::Scope timing(
        mt2mp::game::live_host_state::apply_timing::Part::NativeOpenWorldFields);
    void* active=nullptr;void* world=nullptr;void* map=nullptr;
    int32_t state=-1;
    if(!mode_singleton(w.mode,error)||w.mode!=native_mode||!vt(w.mode,0x16e6730,error)||!read(w.mode,0x2c4,state,error)||state!=4)
        return fail(error,"completed_origin_world_mode_not_active");
#ifdef MT2MP_MIXED_OPEN_TEST
    if(game_override){w.game=game_override;world=world_override;map=map_override;}
    else
#endif
    if(!read(reinterpret_cast<void*>(image),0x13df8f0,w.game,error)||!read(reinterpret_cast<void*>(image),0x13df9c0,world,error)||
       !read(reinterpret_cast<void*>(image),0x13df8c0,map,error))return false;
    if(!vt(w.game,0x1743740,error)||!read(w.game,0x28,active,error)||active!=w.mode||
       !read(w.mode,0x58,w.state,error)||!vt(w.state,0x16de920,error)||!read(w.state,0x160,w.world,error)||
       !vt(w.world,0x174aef0,error)||!read(w.world,0x158,w.map,error)||!vt(w.map,0x1742860,error)||w.world!=world||w.map!=map)
        return fail(error,"completed_origin_GameState_World_Map_reciprocity_mismatch");
    return true;
}
bool close_pin(std::string& error) {
    if(held!=INVALID_HANDLE_VALUE){++current.pin_close_calls;if(!CloseHandle(held))return fail(error,"retire_close winerror="+std::to_string(GetLastError()));++current.pin_close_confirmed;}
    held=INVALID_HANDLE_VALUE;current.held=false;retired_generation=current.generation;
    if(current.state!="failed")current.state="retired";
    return true;
}
Retirement waiting(const std::string& reason,std::string& error) {
    ++current.retirement_observations;
    if(current.retirement_wait!=reason){
        ++current.retirement_wait_changes;
        current.retirement_wait=reason;
        const auto line="[MixedOpen] info operation=retire uuid="+current.uuid+" cause="+reason+"; pin held, native request not replayed\n";
        OutputDebugStringA(line.c_str());
    }
    error="info: "+reason;return Retirement::Pending;
}
bool entry_compare(uintptr_t rva,const unsigned char* actual,size_t count,const char* expected,size_t bytes,std::string& error) {
    if(count!=bytes||std::memcmp(actual,expected,bytes))return fail(error,"native_code_guard_mismatch rva="+std::to_string(rva));
    return true;
}
bool native_code(uintptr_t base,uintptr_t rva,const char* expected,size_t bytes,std::string& error) {
    MEMORY_BASIC_INFORMATION m{};std::array<unsigned char,32> actual{};
    auto* address=reinterpret_cast<void*>(base+rva);
    if(bytes>actual.size()||!readable(address,bytes)||!mt2mp::game::live_host_state::apply_timing::virtual_query(address,&m,sizeof(m),mt2mp::game::live_host_state::apply_timing::Part::QueryOpenWitness)||m.Type!=MEM_IMAGE||
       reinterpret_cast<uintptr_t>(m.AllocationBase)!=base||(m.Protect&0xff)!=PAGE_EXECUTE_READ||copy(address,actual.data(),bytes))
        return fail(error,"immutable_native_code_unavailable rva="+std::to_string(rva));
    return entry_compare(rva,actual.data(),bytes,expected,bytes,error);
}
}

bool initialize(const std::filesystem::path& runtime_root,std::string& error) {
    std::lock_guard lock(gate);
    if(init_phase==InitPhase::Failed){error="initialize_failed_no_retry cause="+init_error;return false;}
    if(held!=INVALID_HANDLE_VALUE)return init_fail(error,"initialize_while_file_held");
    try {
        const auto candidate=absolute_path(runtime_root);
#ifdef MT2MP_FRAMEWORK_PLUGIN
        const auto candidate_paths=framework_open_paths::bind(candidate);
        if(init_phase==InitPhase::Ready)framework_open_paths::unchanged(configured_paths);
#endif
        if(init_phase==InitPhase::Ready){
            if(path_equal(candidate,configured_root))return true;
            return init_fail(error,"immutable_initialization_root_changed");
        }
        if(!no_reparse(candidate,error))return init_fail(error,error);
#ifndef MT2MP_FRAMEWORK_PLUGIN
        const auto component=candidate.filename().wstring();
        const bool client=component.rfind(L"client-",0)==0&&component.size()>7&&
            std::all_of(component.begin()+7,component.end(),[](wchar_t c){return c>=L'0'&&c<=L'9';});
#endif
        std::filesystem::path expected_game=candidate/L"game";
#ifdef MT2MP_FRAMEWORK_PLUGIN
        expected_game=candidate_paths.game;
        configured_paths=candidate_paths;
#else
#ifndef MT2MP_MIXED_OPEN_TEST
        if(const auto* installed=installed_profile::get()){
            if(!path_equal(candidate,absolute_path(installed->runtime_root)))return init_fail(error,"runtime_root_not_verified_installed_profile");
            expected_game=installed->game_dir;
        }else
#endif
        if(candidate.parent_path().filename()!=L"_MPTest"||(component!=L"host"&&!client))return init_fail(error,"runtime_root_not_MPTest_instance");
#endif
        if(tested){configured_root=candidate;init_phase=InitPhase::Ready;return true;}
        if(!native::ready())return init_fail(error,"native_exact_build_resolver_unavailable");
        image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        std::array<wchar_t,32768> executable{};const auto exe_n=GetModuleFileNameW(nullptr,executable.data(),DWORD(executable.size()));
        if(!exe_n||exe_n==executable.size()||!path_equal(absolute_path(executable.data()),absolute_path(expected_game/L"MT2.exe")))
            return init_fail(error,"native_executable_not_owned_runtime");
        if(!native_code(image,0x95ec00,"\x41\x57\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53",12,error)||
           !native_code(image,0xa02d10,stream_ctor_entry,sizeof(stream_ctor_entry)-1,error)||
           !native_code(image,0x96bbd0,reader_ctor_entry,sizeof(reader_ctor_entry)-1,error)||
           !native_code(image,0x95ecc1,"\x89\x73\x70",3,error)||
           !native_code(image,0x96bf3c,"\x80\x79\x18\x00",4,error)||
           !native_code(image,0x96bf63,"\x80\xbb\x90\x00\x00\x00\x00",7,error)||
           !native_code(image,0x96bf90,"\x8b\x83\x8c\x00\x00\x00\xeb\xdb",8,error)||
           !native_code(image,0xbd7a80,"\x48\x8b\x49\x10\xe9\x37\xc4\x8c\xff",9,error)||
           !native_code(image,0x5258b5,"\x80\x79\x0e\x00",4,error)||
           !native_code(image,0x526c15,"\xc6\x43\x0e\x01",4,error)||
           !native_code(image,0x4a5908,"\x48\xc7\x86\xd8\x00\x00\x00\x00\x00\x00\x00",11,error)||
           !native_code(image,0x4a592c,"\xe8\x4f\x8b\x3f\x00",5,error)||
           !native_code(image,0x96d370,"\xc7\x01\x01\x00\x00\x00\x48\x83\xc1\x08",10,error))return init_fail(error,error);
        const auto dll=GetModuleHandleW(L"libphysfs.dll");
        if(!dll)return init_fail(error,"owned_PhysicsFS_module_missing winerror="+std::to_string(GetLastError()));
        std::array<wchar_t,32768> path{};const auto n=GetModuleFileNameW(dll,path.data(),DWORD(path.size()));
        if(!n||n==path.size())return init_fail(error,"PhysicsFS_module_path_failed winerror="+std::to_string(GetLastError()));
        if(!path_equal(absolute_path(path.data()),absolute_path(expected_game/L"libphysfs.dll")))
            return init_fail(error,"PhysicsFS_module_outside_owned_runtime");
        HANDLE file=CreateFileW(path.data(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file==INVALID_HANDLE_VALUE)return init_fail(error,"PhysicsFS_hash_open winerror="+std::to_string(GetLastError()));
        std::string actual;uint64_t bytes=0;const auto hashed=sha_handle(file,actual,bytes,error);
        const auto closed=CloseHandle(file);const auto close_error=closed?0:GetLastError();
        if(!closed)return init_fail(error,"PhysicsFS_hash_close winerror="+std::to_string(close_error));
        if(!hashed)return init_fail(error,error);
        if(actual!="ad61b1b9df28bd9bc7e54a9a3c80e26e766b9f7f125138ffb6823777f0d6d375"||bytes!=451695)
            return init_fail(error,"PhysicsFS_dependency_hash_mismatch actual="+actual);
        real_dir=reinterpret_cast<Lookup>(GetProcAddress(dll,"PHYSFS_getRealDir"));
        mount_point=reinterpret_cast<Lookup>(GetProcAddress(dll,"PHYSFS_getMountPoint"));
        if(!real_dir||!mount_point)return init_fail(error,"PhysicsFS_required_export_missing winerror="+std::to_string(GetLastError()));
        const auto dll_base=reinterpret_cast<uintptr_t>(dll);
        if(reinterpret_cast<uintptr_t>(real_dir)!=dll_base+0x5f4f||reinterpret_cast<uintptr_t>(mount_point)!=dll_base+0x51d4)
            return init_fail(error,"PhysicsFS_export_address_mismatch");
        if(!native_code(dll_base,0x5f4f,"\x55\x48\x89\xe5\x48\x83\xec\x30\x48\x89\x4d\x10",12,error)||
           !native_code(dll_base,0x51d4,"\x55\x48\x89\xe5\x48\x83\xec\x30\x48\x89\x4d\x10",12,error))return init_fail(error,error);
        configured_root=candidate;init_phase=InitPhase::Ready;return true;
    }catch(const std::exception& ex){return init_fail(error,std::string("initialize exception: ")+ex.what());}
}
bool confirm_hooks(uint32_t mask,std::string& error) {
    std::lock_guard lock(gate);
    if(init_phase!=InitPhase::Ready){error="confirm_hooks_initialization_unavailable cause="+init_error;return false;}
    if(mask!=required_hooks)return init_fail(error,"native_open_incomplete_hook_receipt required=15 actual="+std::to_string(mask));
    installed_hooks=mask;return true;
}
void hooks_stopped(const std::string& cause) {
    std::lock_guard lock(gate);std::string error;
    init_fail(error,"native_open_hooks_stopped cause="+cause);
}
bool begin(const std::filesystem::path& runtime_root,const std::string& steam_id,const std::string& uuid,
           const std::string& sha,uint64_t bytes,uint64_t generation,std::string& error,const std::string& world_name) {
    std::lock_guard lock(gate);
    if(held!=INVALID_HANDLE_VALUE)return fail(error,"begin_while_previous_file_held");
    if(init_phase!=InitPhase::Ready||installed_hooks!=required_hooks)return fail(error,"begin_unconfirmed_initialization_or_hooks cause="+init_error);
    if(!net::snapshot_world_path::valid_folder(world_name)||!generation||generation<=retired_generation||!string_format(uuid,true)||!string_format(sha,false)||!bytes||
#ifdef MT2MP_FRAMEWORK_PLUGIN
       (steam_id.size()>20||!std::all_of(steam_id.begin(),steam_id.end(),[](char c){return c>='0'&&c<='9';})))
#else
       steam_id.size()<17||steam_id.size()>20||!std::all_of(steam_id.begin(),steam_id.end(),[](char c){return c>='0'&&c<='9';}))
#endif
        return fail(error,"invalid_owned_file_receipt_uuid_SHA_bytes_generation_user");
    try {
        if(!real_dir||!mount_point||!path_equal(absolute_path(runtime_root),configured_root))
            return fail(error,"owned_runtime_unconfigured_or_changed");
        if(!tested){native::WorldStatus world;if(!native::read_world_status(world,error))return fail(error,"begin_not_native_game_thread cause="+error);}
        const auto name=L"snapshot-"+std::wstring(uuid.begin(),uuid.end())+L".vrz";
#ifdef MT2MP_FRAMEWORK_PLUGIN
        const auto measured=framework_open_paths::live_write(configured_paths);
        const auto expected_write=steam_id.empty()?configured_paths.data:configured_paths.data/std::wstring(steam_id.begin(),steam_id.end());
        if(!private_profile::equal(measured,expected_write))return fail(error,"Framework native open measured write root changed before begin");
        configured_write=measured;
        const auto route_name=measured.lexically_relative(configured_root)/L"save"/net::snapshot_world_path::utf8_path(world_name)/name;
#else
        const auto route_name=std::filesystem::path(L"data")/std::wstring(steam_id.begin(),steam_id.end())/L"save"/net::snapshot_world_path::utf8_path(world_name)/name;
#endif
        physical=absolute_path(configured_root/route_name);
        const auto expected=(configured_root/route_name).lexically_normal();
        if(!path_equal(physical,expected))return fail(error,"snapshot_path_reparse_or_alias");
        auto route=configured_root;
        for(const auto& part:route_name){
            route/=part;if(!no_reparse(route,error))return false;
        }
        current={};callback_failure.store(false);current.uuid=uuid;current.sha256=sha;current.bytes=bytes;current.generation=generation;current.world_epoch=world_epoch;current.state="opening";current.game_thread=GetCurrentThreadId();
        native_file=nullptr;native_stream=nullptr;native_reader=nullptr;file_pending=stream_pending=reader_pending=false;
        native_task=nullptr;native_mode=nullptr;worker_pending=false;
        loaded_world={};
        virtual_name=net::snapshot_world_path::capture(world_name,uuid);
        held=CreateFileW(physical.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN,nullptr);
        if(held==INVALID_HANDLE_VALUE)return fail(error,"snapshot_pin_open winerror="+std::to_string(GetLastError()));
        current.held=true;current.state="hashing";
        std::string actual;
        if(!sha_handle(held,actual,current.hash_bytes,error))return false;
        if(actual!=sha||current.hash_bytes!=bytes)return fail(error,"snapshot_hash_or_bytes_mismatch actual_sha="+actual+" actual_bytes="+std::to_string(current.hash_bytes));
        current.state="pinned";return true;
    }catch(const std::exception& ex){return fail(error,std::string("begin exception: ")+ex.what());}
}
bool file_before(void* file,const void* gcc_filename,int32_t mode,std::string& error) noexcept try {
    std::lock_guard lock(gate);
    try {
        if(held==INVALID_HANDLE_VALUE)return false;
        std::string name;if(!gcc_string(gcc_filename,name,error))return false;
        if(name!=virtual_name){++current.foreign_files;error="info: foreign native file deliberately skipped";return false;}
        if(!active(error))return false;
        if(!worker_pending||!native_task)return fail(error,"native_file_ctor_outside_bound_mode_init_task");
        if(mode!=5||native_file||file_pending||stream_pending||reader_pending)return fail(error,"native_file_ctor_mode_or_lifetime");
        if(!mapped_file(error))return false;
        native_file=file;current.thread=GetCurrentThreadId();file_pending=true;++current.file_calls;return true;
    }catch(const std::exception& ex){return fail(error,std::string("file_before exception: ")+ex.what());}
}catch(const std::exception& ex){return emergency(ex.what());}catch(...){return emergency("unknown C++ exception in file_before");}
bool file_after(void* file,std::string& error) noexcept try {
    std::lock_guard lock(gate);
    try {if(!active(error)||!file_pending||file!=native_file)return fail(error,"native_file_ctor_return_unmatched");
        file_pending=false;++current.file_returns;if(!chain(error))return false;current.origin_confirmed=true;return true;
    }catch(const std::exception& ex){return fail(error,std::string("file_after exception: ")+ex.what());}
}catch(const std::exception& ex){return emergency(ex.what());}catch(...){return emergency("unknown C++ exception in file_after");}
bool stream_before(void* stream,void* file,std::string& error) noexcept try {
    std::lock_guard lock(gate);
    try {if(held==INVALID_HANDLE_VALUE||file!=native_file){++current.foreign_streams;error="info: foreign native stream deliberately skipped";return false;}
        if(!active(error)||!current.origin_confirmed||native_stream||file_pending||stream_pending)
            return fail(error,"serialiser_ctor_pre_read_origin_missing");
        if(!chain(error))return false;native_stream=stream;stream_pending=true;++current.stream_calls;return true;
    }catch(const std::exception& ex){return fail(error,std::string("stream_before exception: ")+ex.what());}
}catch(const std::exception& ex){return emergency(ex.what());}catch(...){return emergency("unknown C++ exception in stream_before");}
bool stream_after(void* stream,void* file,std::string& error) noexcept try {
    std::lock_guard lock(gate);
    try {if(!active(error)||!stream_pending||stream!=native_stream||file!=native_file)return fail(error,"serialiser_ctor_return_unmatched");
        stream_pending=false;++current.stream_returns;return chain(error);
    }catch(const std::exception& ex){return fail(error,std::string("stream_after exception: ")+ex.what());}
}catch(const std::exception& ex){return emergency(ex.what());}catch(...){return emergency("unknown C++ exception in stream_after");}
bool reader_before(void* reader,void* stream,std::string& error) noexcept try {
    std::lock_guard lock(gate);
    try {if(held==INVALID_HANDLE_VALUE||stream!=native_stream){++current.foreign_readers;error="info: foreign native reader deliberately skipped";return false;}
        if(!active(error)||stream_pending||native_reader||reader_pending)return fail(error,"reader_ctor_origin_missing");
        if(!chain(error))return false;native_reader=reader;reader_pending=true;++current.reader_calls;return true;
    }catch(const std::exception& ex){return fail(error,std::string("reader_before exception: ")+ex.what());}
}catch(const std::exception& ex){return emergency(ex.what());}catch(...){return emergency("unknown C++ exception in reader_before");}
bool reader_after(void* reader,void* stream,std::string& error) noexcept try {
    std::lock_guard lock(gate);
    try {void* actual=nullptr;
        if(!active(error)||!reader_pending||reader!=native_reader||stream!=native_stream)return fail(error,"reader_ctor_return_unmatched");
        reader_pending=false;++current.reader_returns;
        if(!read(reader,8,actual,error)||actual!=native_stream||!chain(error))return fail(error,"reader_stream_chain_changed");
        current.reader_confirmed=true;current.state="reading";return true;
    }catch(const std::exception& ex){return fail(error,std::string("reader_after exception: ")+ex.what());}
}catch(const std::exception& ex){return emergency(ex.what());}catch(...){return emergency("unknown C++ exception in reader_after");}
bool validate_reader(void* reader,uint64_t generation,std::string& error) noexcept try {
    std::lock_guard lock(gate);
    try {void* actual=nullptr;
        if(!active(error)||generation!=current.generation||!worker_pending||!current.reader_confirmed||reader!=native_reader||reader_pending)
            return fail(error,"reader_lifetime_generation_unconfirmed");
        if(!read(reader,8,actual,error))return false;
        if(actual!=native_stream)return fail(error,"reader_stream_pointer_changed");
        return chain(error);
    }catch(const std::exception& ex){return fail(error,std::string("validate_reader exception: ")+ex.what());}
}catch(const std::exception& ex){return emergency(ex.what());}catch(...){return emergency("unknown C++ exception in validate_reader");}
bool declared_children(void* reader,uint64_t generation,int32_t& count,std::string& error) noexcept try {
    if(!validate_reader(reader,generation,error))return false;
    std::lock_guard lock(gate);unsigned char valid=0,binary=0;
    if(!read(reader,0x18,valid,error)||valid!=1||!read(reader,0x90,binary,error)||binary!=1)
        return fail(error,"reader_current_binary_record_invalid");
    if(!read(reader,0x8c,count,error)||count<0)return fail(error,"reader_declared_childcount_invalid actual="+std::to_string(count));
    return true;
}catch(const std::exception& ex){return emergency(ex.what());}catch(...){return emergency("unknown C++ exception in declared_children");}
bool array_record(void* reader,uint64_t generation,ArrayKind kind,int32_t& count,std::string& error) noexcept try {
    if(!declared_children(reader,generation,count,error))return false;
    std::lock_guard lock(gate);int32_t type=-1;size_t bytes=0;void* data=nullptr;
    const char* label=kind==ArrayKind::Building?"building":kind==ArrayKind::DungeonInstance?"instance":nullptr;
    if(!label)return fail(error,"unsupported_native_array_kind");
    const auto required=std::strlen(label);
    if(!read(reader,0x20,type,error)||type!=1||!read(reader,0x28,data,error)||!read(reader,0x30,bytes,error)||bytes!=required||!readable(data,bytes))
        return fail(error,"native_array_record_label_descriptor_mismatch");
    std::array<char,8> actual{};
    const auto ex=copy(data,actual.data(),bytes);
    if(ex||std::memcmp(actual.data(),label,bytes))return fail(error,"native_array_record_label_bytes_mismatch code="+std::to_string(ex));
    return true;
}catch(const std::exception& ex){return emergency(ex.what());}catch(...){return emergency("unknown C++ exception in array_record");}
void original_failed(const std::string& operation,const std::string& cause) noexcept try {
    std::lock_guard lock(gate);std::string error;++current.original_failures;current.original_error=operation+": "+cause;
    if(operation=="file")file_pending=false;
    else if(operation=="stream")stream_pending=false;
    else if(operation=="reader")reader_pending=false;
    else if(operation=="worker")worker_pending=false;
    fail(error,"native_original_failed operation="+operation+" cause="+cause);
}catch(const std::exception& ex){emergency(ex.what());}catch(...){emergency("unknown C++ exception in original_failed");}
bool note_request_attempt(uint64_t generation,std::string& error) {
    std::lock_guard lock(gate);
    if(!generation||generation!=current.generation||current.native_request_attempted||!active(error))return fail(error,"native_load_request_replay_or_unbound");
    if(GetCurrentThreadId()!=current.game_thread)return fail(error,"native_request_not_bound_game_thread");
    current.native_request_attempted=true;
    return true;
}
bool worker_before(void* task,uint64_t generation,std::string& error) noexcept try {
    std::lock_guard lock(gate);void* mode=nullptr;int32_t state=-1;unsigned char complete=1;
    if(held==INVALID_HANDLE_VALUE||!current.native_request_attempted){
        if(!task_owner(task,mode,error))return false;
        if(world_epoch==UINT64_MAX)return fail(error,"native_world_epoch_counter_exhausted");
        current.world_epoch=++world_epoch;
        ++current.unarmed_task_runs;current.current_origin_available=false;
        current.origin_confirmed=false;current.reader_confirmed=false;current.state="invalidated";
        current.error="unarmed native Mode Task.Run invalidated prior world origin";error=current.error;
        if(held!=INVALID_HANDLE_VALUE)current.native_request_attempted=true;
        OutputDebugStringA("[MixedOpen] warn operation=Task.Run cause=unarmed native load; prior world origin unavailable, original unchanged\n");
        return false;
    }
    if(!generation||generation!=current.generation||!current.native_request_attempted||!active(error)||worker_pending||current.worker_calls||
       !task_owner(task,mode,error)||!read(mode,0x2c4,state,error)||state!=1||!read(task,0xe,complete,error)||complete!=0)
        return fail(error,"mode_init_worker_start_unconfirmed");
    if(world_epoch==UINT64_MAX)return fail(error,"native_world_epoch_counter_exhausted");
    current.world_epoch=++world_epoch;current.origin_world_epoch=current.world_epoch;
    native_task=task;native_mode=mode;current.thread=GetCurrentThreadId();worker_pending=true;++current.worker_calls;return true;
}catch(const std::exception& ex){return emergency(ex.what());}catch(...){return emergency("unknown C++ exception in worker_before");}
bool worker_after(void* task,uint64_t generation,std::string& error) noexcept try {
    std::lock_guard lock(gate);void* mode=nullptr;void* context=nullptr;
    if(!generation||generation!=current.generation||!worker_pending||task!=native_task||GetCurrentThreadId()!=current.thread||
       !task_owner(task,mode,error)||mode!=native_mode||!read(mode,0xd8,context,error)||context)
        return fail(error,"mode_DoInit_return_context_or_lifetime_unconfirmed");
    worker_pending=false;++current.worker_returns;
    file_pending=stream_pending=reader_pending=false;
    native_file=native_stream=native_reader=nullptr;
    if(current.file_calls!=1||current.file_returns!=1||current.stream_calls!=1||current.stream_returns!=1||current.reader_calls!=1||current.reader_returns!=1)
        return fail(error,"native_worker_return_has_incomplete_file_reader_origin");
    if(current.state!="failed")current.state="awaiting_native_retirement";
    return true;
}catch(const std::exception& ex){return emergency(ex.what());}catch(...){return emergency("unknown C++ exception in worker_after");}
Retirement retire(uint64_t generation,std::string& error) {
    std::lock_guard lock(gate);
    if(!generation||generation!=current.generation){fail(error,"retire_generation_mismatch");return Retirement::Rejected;}
    if(GetCurrentThreadId()!=current.game_thread){fail(error,"retire_not_bound_game_thread");return Retirement::Rejected;}
    if(held==INVALID_HANDLE_VALUE&&current.native_task_retired)return Retirement::Closed;
    if(!current.native_request_attempted||!native_task)return waiting("native Task.Run witness absent",error);
    void* mode=nullptr;void* context=nullptr;int32_t state=-1;unsigned char complete=0;
    if(!task_owner(native_task,mode,error)||mode!=native_mode||!read(mode,0x2c4,state,error)||state<0||state>4||
       !read(mode,0xd8,context,error)||!read(native_task,0xe,complete,error)||complete>1){fail(error,"retire_native_task_readback_rejected");return Retirement::Rejected;}
    current.terminal_mode_state=state;
    if(worker_pending||current.worker_returns!=1||context||complete!=1||state==1||file_pending||stream_pending||reader_pending){
        return waiting("native retirement pending mode="+std::to_string(state)+" complete="+std::to_string(complete)+" worker_returns="+std::to_string(current.worker_returns)+" context_clear="+std::to_string(context==nullptr),error);
    }
    const bool confirmed=current.origin_confirmed&&current.reader_confirmed&&current.state!="failed"&&!callback_failure.load();
    if(!close_pin(error))return Retirement::Rejected;
    if(!current.retirement_wait.empty()){
        ++current.retirement_recoveries;
        const auto line="[MixedOpen] info operation=retire uuid="+current.uuid+" recovery=exact native task retired observations="+std::to_string(current.retirement_observations)+"\n";
        OutputDebugStringA(line.c_str());
    }
    current.completed_origin=confirmed;
    current.native_task_retired=true;current.retirement_wait.clear();return Retirement::Closed;
}
bool retire_unstarted(uint64_t generation,std::string& error) {
    std::lock_guard lock(gate);
    if(!generation||generation!=current.generation||GetCurrentThreadId()!=current.game_thread||current.native_request_attempted||
       current.worker_calls||current.file_calls||current.stream_calls||current.reader_calls)return fail(error,"retire_unstarted_rejected_native_attempt_observed");
    return close_pin(error);
}
bool seal_loaded_world(uint64_t generation,std::string& error) {
    std::lock_guard lock(gate);
    if(callback_failure.load()||GetCurrentThreadId()!=current.game_thread||!generation||generation!=current.generation||current.state=="failed"||
       !current.native_task_retired||!current.origin_confirmed||!current.reader_confirmed||current.worker_returns!=1)
        return fail(error,"completed_origin_unconfirmed_or_wrong_thread_generation");
    WorldWitness witness;
    ++current.world_checks;
    if(!world_witness(witness,error))return false;
    if(current.world_witness_bound&&witness!=loaded_world)return fail(error,"completed_origin_world_changed_before_seal");
    loaded_world=witness;current.world_witness_bound=true;current.completed_origin=true;current.current_origin_available=true;
    return true;
}
bool validate_completed_origin(uint64_t generation,std::string& error) {
    std::lock_guard lock(gate);
    if(callback_failure.load()||GetCurrentThreadId()!=current.game_thread||!generation||generation!=current.generation||!current.completed_origin||
       !current.current_origin_available||!current.world_witness_bound||!current.origin_world_epoch||current.origin_world_epoch!=world_epoch||current.state=="failed"||current.state=="invalidated")
        return fail(error,"completed_origin_unavailable_wrong_thread_or_generation");
    WorldWitness witness;++current.world_checks;
    if(!world_witness(witness,error))return false;
    if(witness!=loaded_world)return fail(error,"completed_origin_world_changed");
    return true;
}
bool read_completed_world(uint64_t generation,CurrentWorld& result,std::string& error) {
    result={};
    std::lock_guard lock(gate);
    if(GetCurrentThreadId()!=current.game_thread) {error="current_world_read_wrong_thread";return false;}
    if(!generation||generation!=current.generation) {error="current_world_read_generation_mismatch";return false;}
    if(init_phase!=InitPhase::Ready||installed_hooks!=required_hooks||callback_failure.load()||
       !current.completed_origin||!current.current_origin_available||!current.world_witness_bound||
       !current.origin_world_epoch||current.origin_world_epoch!=world_epoch||
       current.state=="failed"||current.state=="invalidated") {
        error="current_world_read_origin_unavailable";return false;
    }
    WorldWitness first,second;
    {
        mt2mp::game::live_host_state::apply_timing::PageReadScope pages(
            mt2mp::game::live_host_state::apply_timing::page_read_scope==nullptr);
        ++current.world_checks;
        if(!world_witness(first,error))return false;
        ++current.world_checks;
        if(!world_witness(second,error))return false;
    }
    if(first!=loaded_world||second!=first)return fail(error,"current_world_read_witness_changed");
    if(callback_failure.load()) {error="current_world_read_observer_failure";return false;}
    CurrentWorld confirmed;
    confirmed.game=first.game;confirmed.mode=first.mode;confirmed.state=first.state;
    confirmed.world=first.world;confirmed.map=first.map;
    confirmed.generation=generation;confirmed.world_epoch=current.origin_world_epoch;confirmed.bytes=current.bytes;
    confirmed.uuid=current.uuid;confirmed.sha256=current.sha256;
    result=std::move(confirmed);error.clear();return true;
}
Status status(){
    std::lock_guard lock(gate);auto result=current;
    result.initialized=init_phase==InitPhase::Ready;result.hook_mask=installed_hooks;
    if(init_phase==InitPhase::Failed){result.state="failed";result.error=init_error;}
    if(!result.initialized||result.hook_mask!=required_hooks){result.current_origin_available=false;result.origin_confirmed=false;result.reader_confirmed=false;}
    if(callback_failure.load()){
        std::array<char,384> cause{};
        while(emergency_gate.test_and_set(std::memory_order_acquire)){}
        cause=emergency_cause;result.callback_error_truncated=emergency_truncated;
        emergency_gate.clear(std::memory_order_release);
        result.state="failed";result.error="observer callback exception cause="+std::string(cause.data());
        result.origin_confirmed=false;result.reader_confirmed=false;result.current_origin_available=false;
    }
    return result;
}
#ifdef MT2MP_MIXED_OPEN_TEST
void testing_backend(uintptr_t base,Lookup real,Lookup mount){std::lock_guard lock(gate);image=base;real_dir=real;mount_point=mount;tested=true;init_phase=InitPhase::Uninitialized;init_error.clear();installed_hooks=0;configured_root.clear();}
void testing_mode(void* mode){std::lock_guard lock(gate);mode_override=mode;}
void testing_world(void* game,void* world,void* map){std::lock_guard lock(gate);game_override=game;world_override=world;map_override=map;}
bool testing_same_file(const std::filesystem::path& candidate,std::string& error){std::lock_guard lock(gate);return same_file_object(candidate,error);}
void testing_callback_failure(const char* cause){emergency(cause);}
bool testing_entry_guard(uintptr_t rva,const unsigned char* bytes,size_t count,std::string& error){
    std::lock_guard lock(gate);
    if(rva==0xa02d10)return entry_compare(rva,bytes,count,stream_ctor_entry,sizeof(stream_ctor_entry)-1,error);
    if(rva==0x96bbd0)return entry_compare(rva,bytes,count,reader_ctor_entry,sizeof(reader_ctor_entry)-1,error);
    return fail(error,"testing_entry_guard_unsupported_RVA");
}
#endif
}
