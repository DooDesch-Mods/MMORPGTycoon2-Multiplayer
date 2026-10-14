#include <windows.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <memory>
#include <intrin.h>
#include <iterator>
#include "mt2mp/core/installed_profile.h"
#include "mt2mp/core/private_profile_paths.h"

namespace {
HMODULE self=nullptr,system_wsock=nullptr,mod=nullptr;
HMODULE sdl_module=nullptr;
using Void=void(*)();using Duplicate=char*(*)(const char*);using WriteDir=int(*)(const char*);
Void steam_callbacks=nullptr,steam_shutdown=nullptr,mod_shutdown=nullptr;
Duplicate duplicate=nullptr;WriteDir original_write_dir=nullptr;
INIT_ONCE socket_once=INIT_ONCE_STATIC_INIT,profile_once=INIT_ONCE_STATIC_INIT;
std::atomic<unsigned> isolation{0},mod_state{0};std::atomic<bool> stopping{false};
std::atomic<bool> log_root_ready{false};
std::filesystem::path game_dir,root,data_dir;
std::string run_id,pref_utf8;
mt2mp::private_profile::DirectoryPins root_pins;
std::unique_ptr<mt2mp::private_profile::DirectoryPins> write_pins;
void* system_exports[75]{};
struct Export {WORD ordinal;const char* name;};
constexpr Export exports[]={
#include "wsock32_exports.inc"
};
static_assert(std::size(exports)==75);
void record(const char* level,const char* operation,DWORD error=0){
    char message[1024]{};sprintf_s(message,"%s: %s Win32=%lu\n",level,operation,error);OutputDebugStringA(message);
    if(!log_root_ready.load())return;
    HANDLE log=CreateFileW((root/L"mt2mp_bootstrap.log").c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(log==INVALID_HANDLE_VALUE){OutputDebugStringA("error: private bootstrap log open failed\n");return;}
    DWORD written=0;const DWORD bytes=DWORD(std::strlen(message));
    if(!WriteFile(log,message,bytes,&written,nullptr)||written!=bytes)OutputDebugStringA("error: private bootstrap log write failed\n");
    if(!CloseHandle(log))OutputDebugStringA("error: private bootstrap log close failed\n");
}
[[noreturn]] void fatal(const char* operation,DWORD error){
    record("error",operation,error);TerminateProcess(GetCurrentProcess(),error?error:ERROR_DLL_INIT_FAILED);
    __fastfail(7);
}
BOOL CALLBACK resolve_system(PINIT_ONCE,void*,void**){
    try{
        std::array<wchar_t,32768> directory{};const UINT n=GetSystemDirectoryW(directory.data(),UINT(directory.size()));
        mt2mp::private_profile::require(n>0&&n<directory.size(),"GetSystemDirectoryW");
        const auto expected=std::filesystem::path(directory.data())/L"wsock32.dll";
        system_wsock=LoadLibraryExW(expected.c_str(),nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
        if(!system_wsock||system_wsock==self)throw std::runtime_error("System32 WSOCK32 load or self-handle exclusion failed");
        if(!mt2mp::private_profile::equal(mt2mp::private_profile::module_file(system_wsock),mt2mp::private_profile::canonical_file(expected)))
            throw std::runtime_error("System32 WSOCK32 loaded module path differs");
        for(size_t i=0;i<std::size(exports);++i){
            const auto named=GetProcAddress(system_wsock,exports[i].name);
            const auto ordinal=GetProcAddress(system_wsock,MAKEINTRESOURCEA(exports[i].ordinal));
            if(!named||named!=ordinal)throw std::runtime_error("System32 WSOCK32 export/name ordinal mismatch: "+std::string(exports[i].name));
            system_exports[i]=reinterpret_cast<void*>(named);
        }
        return TRUE;
    }catch(const std::exception& error){fatal(error.what(),GetLastError());}
}
void ensure_system(){if(!InitOnceExecuteOnce(&socket_once,resolve_system,nullptr,nullptr))fatal("System32 WSOCK32 resolution failed",GetLastError());}
#include "../src/core/dropin_proof_policy.inc"
BOOL CALLBACK prepare_profile(PINIT_ONCE,void*,void**){
    try{
        for(const wchar_t* name:{L"MT2MP_TEST_ROOT",L"MT2MP_RUNTIME_ROOT",L"MT2MP_RUN_ID"})
            if(GetEnvironmentVariableW(name,nullptr,0))throw std::runtime_error("installed loader cannot mix launcher test variables");
        game_dir=mt2mp::private_profile::module_file(nullptr).parent_path();
        if(!mt2mp::private_profile::equal(mt2mp::private_profile::module_file(self),game_dir/L"WSOCK32.dll"))
            throw std::runtime_error("drop-in loader is outside actual executable directory");
        if(!mt2mp::private_profile::equal(mt2mp::private_profile::module_file(sdl_module),game_dir/L"SDL3.dll"))
            throw std::runtime_error("original SDL preference import is outside actual game directory");
        const auto legacy_steam=game_dir/L"steam_api64_original.dll";
        if(GetModuleHandleW(legacy_steam.c_str()))throw std::runtime_error("drop-in and legacy Steam proxy cannot own one process");
        checked_dropin_proof_root(game_dir);
        const auto requested=mt2mp::private_profile::runtime_root(game_dir);
        root_pins.verify(requested.parent_path().parent_path());
        root_pins.verify(requested,true);root=requested;log_root_ready.store(true);data_dir=root/L"data";
        root_pins.verify(data_dir,true);root_pins.verify(root/L"diagnostics",true);
        run_id=mt2mp::private_profile::new_run_id();pref_utf8=mt2mp::private_profile::utf8(data_dir)+"/";
        ensure_system();record("info","private installed profile prepared before native preference path");return TRUE;
    }catch(const std::exception& error){fatal(error.what(),GetLastError());}
}
void ensure_profile(){if(!InitOnceExecuteOnce(&profile_once,prepare_profile,nullptr,nullptr))fatal("installed private profile setup failed",GetLastError());}
char* private_pref_path(const char*,const char*){
    ensure_profile();char* path=duplicate(pref_utf8.c_str());if(!path)fatal("SDL_strdup private preference path failed",ERROR_NOT_ENOUGH_MEMORY);
    isolation.fetch_or(1);return path;
}
int private_write_dir(const char* value){
    ensure_profile();if(!value)return original_write_dir(nullptr);
    try{
        auto requested=std::filesystem::path(mt2mp::private_profile::wide_utf8(value));
        // SDL preference paths end with a separator; retain raw parent and alias components for pin validation.
        while(requested.has_relative_path()&&requested.filename().empty())requested=requested.parent_path();
        if(!mt2mp::private_profile::equal(requested,data_dir)&&!mt2mp::private_profile::descendant(requested,data_dir))
            throw std::runtime_error("PHYSFS_setWriteDir outside confirmed private data root refused");
        auto pins=std::make_unique<mt2mp::private_profile::DirectoryPins>();pins->verify(requested);
        const int result=original_write_dir(value);
        if(!result){record("error","original PHYSFS_setWriteDir refused private directory");return 0;}
        write_pins=std::move(pins);isolation.fetch_or(2);record("info","private PHYSFS write directory acknowledged");return result;
    }catch(const std::exception& error){record("error",error.what(),GetLastError());return 0;}
}
void callbacks(){
    steam_callbacks();if(stopping.load())return;
    if(isolation.load()!=3){static std::atomic<bool> reported{false};if(!reported.exchange(true))OutputDebugStringA("info: mod initialization waits for private SDL and PhysFS acknowledgements\n");return;}
    unsigned expected=0;if(!mod_state.compare_exchange_strong(expected,1))return;
    try{
    const auto path=game_dir/L"MT2_Multiplayer.dll";
    if(!mt2mp::private_profile::equal(mt2mp::private_profile::canonical_file(path),path))fatal("drop-in mod exact path is an alias",ERROR_INVALID_NAME);
    mod=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_SYSTEM32);
    if(!mod||!mt2mp::private_profile::equal(mt2mp::private_profile::module_file(mod),mt2mp::private_profile::canonical_file(path)))
        fatal("drop-in mod exact-path load failed",GetLastError());
    using Initialize=bool(*)(HMODULE,const mt2mp::installed_profile::Receipt*);
    const auto initialize=reinterpret_cast<Initialize>(GetProcAddress(mod,"MT2MP_InitInstalled"));
    mod_shutdown=reinterpret_cast<Void>(GetProcAddress(mod,"MT2MP_Shutdown"));
    if(!initialize||!mod_shutdown)fatal("drop-in mod required export absent",ERROR_PROC_NOT_FOUND);
    mt2mp::installed_profile::Receipt receipt;
    receipt.loader=self;receipt.process_id=GetCurrentProcessId();receipt.isolation_flags=isolation.load();
    receipt.game_dir=game_dir.c_str();receipt.runtime_root=root.c_str();receipt.user_data_dir=data_dir.c_str();receipt.run_id=run_id.c_str();
    if(!initialize(mod,&receipt))fatal("installed mod initialization failed; private profile retained",ERROR_DLL_INIT_FAILED);
    mod_state.store(2);record("info","installed mod initialized after native Steam callback");
    }catch(const std::exception& error){fatal(error.what(),GetLastError());}
}
void shutdown(){
    if(!stopping.exchange(true)){
        if(mod_state.load()==1)fatal("Steam shutdown raced synchronous mod initialization",ERROR_BUSY);
        if(mod_state.exchange(3)==2)mod_shutdown();
        record("info","installed mod shutdown completed before original Steam shutdown");
    }
    steam_shutdown();
}
struct ImportPatch {ULONGLONG* slot=nullptr;ULONGLONG original=0,replacement=0;};
ImportPatch patches[4]{};size_t patch_count=0;
bool same(const char* a,const char* b,bool insensitive=false){
    while(*a&&*b){char x=*a++,y=*b++;if(insensitive){if(x>='A'&&x<='Z')x=static_cast<char>(x+32);if(y>='A'&&y<='Z')y=static_cast<char>(y+32);}if(x!=y)return false;}return *a==*b;
}
bool patch_import(const char* library,const char* name,void* replacement,void** original){
    auto* base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>1048576)return false;
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return false;
    const size_t size=nt->OptionalHeader.SizeOfImage;
    const auto in_image=[size](size_t offset,size_t bytes){return offset<size&&bytes<=size-offset;};
    const auto imports=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if(!in_image(imports.VirtualAddress,imports.Size))return false;
    auto* descriptors=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+imports.VirtualAddress);
    for(size_t i=0;i<imports.Size/sizeof(IMAGE_IMPORT_DESCRIPTOR)&&descriptors[i].Name;++i){
        const auto& descriptor=descriptors[i];
        if(!in_image(descriptor.Name,1)||!std::memchr(base+descriptor.Name,0,size-descriptor.Name))return false;
        if(!same(reinterpret_cast<char*>(base+descriptor.Name),library,true))continue;
        if(!descriptor.OriginalFirstThunk)return false;
        for(size_t j=0;;++j){
            const size_t source=descriptor.OriginalFirstThunk+j*sizeof(IMAGE_THUNK_DATA64),target=descriptor.FirstThunk+j*sizeof(IMAGE_THUNK_DATA64);
            if(!in_image(source,sizeof(IMAGE_THUNK_DATA64))||!in_image(target,sizeof(IMAGE_THUNK_DATA64)))return false;
            const auto entry=reinterpret_cast<IMAGE_THUNK_DATA64*>(base+source)->u1.AddressOfData;if(!entry)break;
            if(IMAGE_SNAP_BY_ORDINAL64(entry))continue;
            if(!in_image(size_t(entry),sizeof(WORD)+1)||!std::memchr(base+entry+sizeof(WORD),0,size-size_t(entry)-sizeof(WORD)))return false;
            if(!same(reinterpret_cast<char*>(base+entry+sizeof(WORD)),name))continue;
            auto* slot=&reinterpret_cast<IMAGE_THUNK_DATA64*>(base+target)->u1.Function;
            if(!*slot||patch_count>=std::size(patches))return false;
            DWORD protection=0;if(!VirtualProtect(slot,sizeof(*slot),PAGE_READWRITE,&protection))return false;
            *original=reinterpret_cast<void*>(*slot);patches[patch_count++]={slot,*slot,reinterpret_cast<ULONGLONG>(replacement)};
            *slot=reinterpret_cast<ULONGLONG>(replacement);DWORD prior=0;
            return VirtualProtect(slot,sizeof(*slot),protection,&prior)!=0;
        }
    }return false;
}
void rollback(){
    while(patch_count){const auto patch=patches[--patch_count];DWORD old=0;
        if(*patch.slot!=patch.replacement||!VirtualProtect(patch.slot,sizeof(*patch.slot),PAGE_READWRITE,&old)){
            OutputDebugStringA("error: drop-in IAT rollback guard/protection failed\n");continue;}
        *patch.slot=patch.original;DWORD ignored=0;
        if(!VirtualProtect(patch.slot,sizeof(*patch.slot),old,&ignored))OutputDebugStringA("error: drop-in IAT rollback protection restore failed\n");
    }
}
[[maybe_unused]] bool install_bridges(){
    void* original_pref=nullptr;
    const bool pref=patch_import("SDL3.dll","SDL_GetPrefPath",reinterpret_cast<void*>(&private_pref_path),&original_pref);
    if(pref&&GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              reinterpret_cast<LPCWSTR>(original_pref),&sdl_module))
        duplicate=reinterpret_cast<Duplicate>(GetProcAddress(sdl_module,"SDL_strdup"));
    if(!pref||!duplicate||
       !patch_import("libphysfs.dll","PHYSFS_setWriteDir",reinterpret_cast<void*>(&private_write_dir),reinterpret_cast<void**>(&original_write_dir))||
       !patch_import("steam_api64.dll","SteamAPI_RunCallbacks",reinterpret_cast<void*>(&callbacks),reinterpret_cast<void**>(&steam_callbacks))||
       !patch_import("steam_api64.dll","SteamAPI_Shutdown",reinterpret_cast<void*>(&shutdown),reinterpret_cast<void**>(&steam_shutdown))){
        OutputDebugStringA("error: drop-in EXE import bridge installation failed\n");rollback();return false;
    }return true;
}
}
extern "C" void** mt2mp_wsock_resolve(unsigned index){
    const DWORD saved=GetLastError();if(index>=std::size(exports))fatal("WSOCK32 export index invalid",ERROR_INVALID_PARAMETER);
    ensure_system();void** target=&system_exports[index];SetLastError(saved);return target;
}
#ifndef MT2MP_DROPIN_LOADER_TEST
BOOL APIENTRY DllMain(HMODULE module,DWORD reason,LPVOID){
    if(reason==DLL_PROCESS_ATTACH){self=module;if(!DisableThreadLibraryCalls(module))OutputDebugStringA("warn: drop-in thread notifications remain enabled\n");return install_bridges()?TRUE:FALSE;}
    if(reason==DLL_PROCESS_DETACH)stopping.store(true);
    return TRUE;
}
#endif
