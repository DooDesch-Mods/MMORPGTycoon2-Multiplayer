#include "native_free_runtime.h"
#include "runtime_contract_bytes.h"
#include "../native_uid/weak_lifetime_guard.h"
#include "../prop_lease/native_contract_bytes.h"
#include "../native_lease/native_contract_bytes.h"
#include "mt2mp/game/native_engine.h"
#include "native_open_runtime.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <bcrypt.h>
#include <MinHook.h>
#include <intrin.h>
#include <cstring>
#include <limits>
#include <mutex>
#include <new>
#include <vector>

namespace mt2mp::native_free_runtime {
namespace {
constexpr uintptr_t sized_rva=0x1264b60,free_iat_rva=0x1c52a00,malloc_iat_rva=0x1c52a70;
template<size_t N>void text(std::array<char,N>& out,const char* from)noexcept{size_t n=0;if(from)while(n+1<N&&from[n]){out[n]=from[n];++n;}out[n]=0;}
__declspec(noinline) bool copy(const void* from,void* to,size_t size,uint32_t& code)noexcept{
    __try{std::memcpy(to,from,size);return true;}__except((code=GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER)){return false;}
}
bool committed(uintptr_t address,size_t bytes,uintptr_t owner,bool execute,std::string& error){
    MEMORY_BASIC_INFORMATION info{};
    if(!address||bytes>std::numeric_limits<uintptr_t>::max()-address||!VirtualQuery(reinterpret_cast<void*>(address),&info,sizeof info)){error="error: VirtualQuery failed for exact native address";return false;}
    const auto end=uintptr_t(info.BaseAddress)+info.RegionSize;
    const auto p=info.Protect&0xff;
    const bool code=p==PAGE_EXECUTE||p==PAGE_EXECUTE_READ;
    const bool data=p==PAGE_READONLY||p==PAGE_READWRITE;
    if(info.State!=MEM_COMMIT||info.Type!=MEM_IMAGE||uintptr_t(info.AllocationBase)!=owner||address+bytes>end||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(execute?code:data)){error="error: native address is not expected committed executable/data MEM_IMAGE owner";return false;}
    return true;
}
bool bytes(uintptr_t address,const unsigned char* expected,size_t count,std::string& error){
    unsigned char actual[256]{};uint32_t code=0;
    if(count>sizeof actual||!copy(reinterpret_cast<void*>(address),actual,count,code)){error="error: native code read failed SEH="+std::to_string(code);return false;}
    if(std::memcmp(actual,expected,count)){error="error: exact native code bytes differ RVA/address="+std::to_string(address);return false;}return true;
}
bool file_sha(HANDLE file,ModuleReceipt& result,std::string& error){
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;unsigned char object[4096]{},buffer[65536]{},digest[32]{};DWORD object_size=0,got=0;
    NTSTATUS code=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0);
    if(code>=0)code=BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&object_size),sizeof object_size,&got,0);
    if(code>=0&&object_size<=sizeof object)code=BCryptCreateHash(algorithm,&hash,object,object_size,nullptr,0,0);else if(code>=0){code=NTSTATUS(0xc0000023u);error="error: BCrypt object bytes actual="+std::to_string(object_size)+" required_max="+std::to_string(sizeof object);}
    bool okay=code>=0;uint64_t total=0;DWORD read=0;
    while(okay){if(!ReadFile(file,buffer,sizeof buffer,&read,nullptr)){error="error: module file ReadFile failed code="+std::to_string(GetLastError());okay=false;break;}if(!read)break;total+=read;code=BCryptHashData(hash,buffer,read,0);if(code<0){okay=false;break;}}
    if(okay){code=BCryptFinishHash(hash,digest,sizeof digest,0);okay=code>=0;}
    if(!okay&&error.empty())error="error: BCrypt SHA256 primary failure status="+std::to_string(code);
    if(hash){const auto cleanup=BCryptDestroyHash(hash);if(cleanup<0){okay=false;error+=(error.empty()?"error: ":"; cleanup: ")+std::string("BCryptDestroyHash failed status=")+std::to_string(cleanup);}}
    if(algorithm){const auto cleanup=BCryptCloseAlgorithmProvider(algorithm,0);if(cleanup<0){okay=false;error+=(error.empty()?"error: ":"; cleanup: ")+std::string("BCryptCloseAlgorithmProvider failed status=")+std::to_string(cleanup);}}
    if(!okay){if(error.empty())error="error: BCrypt SHA256 failed status="+std::to_string(code);return false;}
    constexpr char hex[]="0123456789abcdef";for(size_t i=0;i<32;++i){result.sha256[i*2]=hex[digest[i]>>4];result.sha256[i*2+1]=hex[digest[i]&15];}result.sha256[64]=0;result.bytes=total;return true;
}
bool module(uintptr_t address,ModuleReceipt& result,std::string& error){
    HMODULE owner=nullptr;
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(address),&owner)){error="error: actual owner module pin failed code="+std::to_string(GetLastError());return false;}
    result={};result.base=uintptr_t(owner);result.pinned=true;
    const auto count=GetModuleFileNameW(owner,result.path.data(),DWORD(result.path.size()));
    if(!count||count>=result.path.size()){error="error: actual module path missing or truncated";return false;}
    const auto file=CreateFileW(result.path.data(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE){error="error: actual module file pin failed code="+std::to_string(GetLastError());return false;}
    // Keep the file locked for the receipt lifetime, including partial failures.
    result.held_file=uintptr_t(file);
    if(!file_sha(file,result,error))return false;
    DWORD unused=0;const auto length=GetFileVersionInfoSizeW(result.path.data(),&unused);
    if(!length){result.version_error=GetLastError();if(result.version_error==ERROR_RESOURCE_DATA_NOT_FOUND||result.version_error==ERROR_RESOURCE_TYPE_NOT_FOUND||result.version_error==ERROR_RESOURCE_NAME_NOT_FOUND||result.version_error==ERROR_RESOURCE_LANG_NOT_FOUND)return true;error="error: actual module version query failed code="+std::to_string(result.version_error);return false;}
    std::vector<unsigned char> data(length);VS_FIXEDFILEINFO* info=nullptr;UINT size=0;
    if(!GetFileVersionInfoW(result.path.data(),0,length,data.data())||!VerQueryValueW(data.data(),L"\\",reinterpret_cast<void**>(&info),&size)||size<sizeof(VS_FIXEDFILEINFO)||!info||info->dwSignature!=0xfeef04bd){error="error: actual module fixed version resource invalid";return false;}
    result.version={HIWORD(info->dwFileVersionMS),LOWORD(info->dwFileVersionMS),HIWORD(info->dwFileVersionLS),LOWORD(info->dwFileVersionLS)};result.version_present=true;return true;
}
bool pe_headers(uintptr_t base,IMAGE_NT_HEADERS64& nt,std::string& error){
    IMAGE_DOS_HEADER dos{};uint32_t code=0;
    if(!copy(reinterpret_cast<void*>(base),&dos,sizeof dos,code)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||dos.e_lfanew>0x100000||!copy(reinterpret_cast<void*>(base+uintptr_t(dos.e_lfanew)),&nt,sizeof nt,code)||nt.Signature!=IMAGE_NT_SIGNATURE||nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC){error="error: actual PE64 headers invalid SEH="+std::to_string(code);return false;}return true;
}
bool image_string(uintptr_t base,uint32_t rva,uint32_t image_bytes,char* out,size_t count,std::string& error){
    uint32_t code=0;for(size_t n=0;n<count;++n){if(uint64_t(rva)+n>=image_bytes||!copy(reinterpret_cast<void*>(base+rva+n),out+n,1,code)){error="error: actual PE string read failed SEH="+std::to_string(code);return false;}if(!out[n])return true;}error="error: actual PE string truncated";return false;
}
bool exported(HMODULE named,const char* symbol,ExportReceipt& out,ModuleReceipt& owning,std::string& error){
    out={};const auto target=GetProcAddress(named,symbol);if(!target){error="error: actual CRT export unavailable";return false;}
    if(!module(uintptr_t(target),owning,error)||!committed(uintptr_t(target),1,owning.base,true,error))return false;
    IMAGE_NT_HEADERS64 nt;if(!pe_headers(uintptr_t(named),nt,error))return false;
    const auto directory=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];IMAGE_EXPORT_DIRECTORY exports{};uint32_t code=0;
    if(!directory.VirtualAddress||uint64_t(directory.VirtualAddress)+directory.Size>nt.OptionalHeader.SizeOfImage||!copy(reinterpret_cast<void*>(uintptr_t(named)+directory.VirtualAddress),&exports,sizeof exports,code)||exports.NumberOfNames>100000||exports.NumberOfFunctions>100000){error="error: actual CRT export table invalid";return false;}
    for(uint32_t i=0;i<exports.NumberOfNames;++i){uint32_t name_rva=0,function_rva=0;uint16_t ordinal=0;char name[256]{};
        if(uint64_t(exports.AddressOfNames)+uint64_t(i)*4+4>nt.OptionalHeader.SizeOfImage||uint64_t(exports.AddressOfNameOrdinals)+uint64_t(i)*2+2>nt.OptionalHeader.SizeOfImage||!copy(reinterpret_cast<void*>(uintptr_t(named)+exports.AddressOfNames+i*4),&name_rva,4,code)||!copy(reinterpret_cast<void*>(uintptr_t(named)+exports.AddressOfNameOrdinals+i*2),&ordinal,2,code)||!image_string(uintptr_t(named),name_rva,nt.OptionalHeader.SizeOfImage,name,sizeof name,error)){if(error.empty())error="error: actual export name/ordinal read failed";return false;}
        if(std::strcmp(name,symbol))continue;
        if(ordinal>=exports.NumberOfFunctions||uint64_t(exports.AddressOfFunctions)+uint64_t(ordinal)*4+4>nt.OptionalHeader.SizeOfImage||!copy(reinterpret_cast<void*>(uintptr_t(named)+exports.AddressOfFunctions+ordinal*4),&function_rva,4,code)||function_rva>=nt.OptionalHeader.SizeOfImage){error="error: actual export function RVA invalid";return false;}
        out.export_rva=function_rva;out.resolved=uintptr_t(target);out.owner=owning.base;out.forwarded=function_rva>=directory.VirtualAddress&&uint64_t(function_rva)<uint64_t(directory.VirtualAddress)+directory.Size;
        if(out.forwarded){if(!image_string(uintptr_t(named),function_rva,nt.OptionalHeader.SizeOfImage,out.forwarder.data(),out.forwarder.size(),error))return false;}
        else if(uintptr_t(named)+function_rva!=uintptr_t(target)){error="error: CRT non-forwarded export differs from actual symbol owner";return false;}
        out.committed_executable_image=true;return true;
    }error="error: named CRT export absent from actual export directory";return false;
}
bool imported(uintptr_t base,const char* symbol,uint32_t expected_slot,std::string& error){
    IMAGE_NT_HEADERS64 nt;if(!pe_headers(base,nt,error))return false;
    const auto directory=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];uint32_t code=0;
    if(!directory.VirtualAddress||uint64_t(directory.VirtualAddress)+directory.Size>nt.OptionalHeader.SizeOfImage){error="error: exact executable import directory invalid";return false;}
    for(uint32_t offset=0;uint64_t(offset)+sizeof(IMAGE_IMPORT_DESCRIPTOR)<=directory.Size;offset+=sizeof(IMAGE_IMPORT_DESCRIPTOR)){
        IMAGE_IMPORT_DESCRIPTOR entry{};if(!copy(reinterpret_cast<void*>(base+directory.VirtualAddress+offset),&entry,sizeof entry,code)){error="error: exact import descriptor SEH="+std::to_string(code);return false;}if(!entry.Name)break;
        char dll[256]{};if(!image_string(base,entry.Name,nt.OptionalHeader.SizeOfImage,dll,sizeof dll,error))return false;if(_stricmp(dll,"msvcrt.dll"))continue;
        if(!entry.OriginalFirstThunk){error="error: exact CRT import lacks original name thunk";return false;}
        for(uint32_t index=0;index<4096;++index){uint64_t thunk=0;const auto lookup=uint64_t(entry.OriginalFirstThunk)+uint64_t(index)*8;if(lookup+8>nt.OptionalHeader.SizeOfImage||!copy(reinterpret_cast<void*>(base+uintptr_t(lookup)),&thunk,8,code)){error="error: exact CRT original thunk read invalid";return false;}if(!thunk)break;if(thunk&IMAGE_ORDINAL_FLAG64)continue;
            if(thunk>UINT32_MAX-2){error="error: exact CRT import name RVA invalid";return false;}char name[256]{};if(!image_string(base,uint32_t(thunk)+2,nt.OptionalHeader.SizeOfImage,name,sizeof name,error))return false;if(std::strcmp(name,symbol))continue;
            if(uint64_t(entry.FirstThunk)+uint64_t(index)*8!=expected_slot){error="error: exact CRT symbol IAT offset differs";return false;}return true;
        }error="error: exact CRT named import absent or exceeds metadata bound";return false;
    }error="error: exact executable lacks named CRT import descriptor";return false;
}
bool jump(uintptr_t from,uintptr_t wanted,std::string& error){unsigned char code[5]{};uint32_t seh=0;int32_t delta=0;if(!copy(reinterpret_cast<void*>(from),code,5,seh)){error="error: actual native tail JMP read SEH="+std::to_string(seh);return false;}std::memcpy(&delta,code+1,4);if(code[0]!=0xe9||from+5+intptr_t(delta)!=wanted){error="error: actual native tail JMP target differs";return false;}return true;}
bool abs_jump(uintptr_t from,uintptr_t wanted,std::string& error){unsigned char code[14]{};uint32_t seh=0;uintptr_t target=0;if(!copy(reinterpret_cast<void*>(from),code,sizeof code,seh)){error="error: actual MinHook absolute JMP read SEH="+std::to_string(seh);return false;}std::memcpy(&target,code+6,8);if(std::memcmp(code,"\xff\x25\0\0\0\0",6)||target!=wanted){error="error: actual MinHook trampoline/relay absolute route differs";return false;}return true;}
__declspec(noinline) void sized_call(Runtime* runtime,void* p,uint64_t n,uintptr_t origin){runtime->enter();__try{runtime->published_observer()->sized_event(p,n,origin);}__finally{runtime->leave();}}
__declspec(noinline) void free_call(Runtime* runtime,void* p,uintptr_t origin){runtime->enter();__try{runtime->published_observer()->free_event(p,origin);}__finally{runtime->leave();}}
__declspec(noinline) void sized_entry(void* p,uint64_t n){sized_call(&Runtime::instance(),p,n,uintptr_t(_ReturnAddress()));}
__declspec(noinline) void free_entry(void* p){free_call(&Runtime::instance(),p,uintptr_t(_ReturnAddress()));}
using Factory=void*(*)(const void*);
__declspec(noinline) bool factory_call(Factory factory,void*& holder,uint32_t& code){__try{holder=factory(nullptr);return true;}__except((GetExceptionCode()==0xe06d7363?EXCEPTION_CONTINUE_SEARCH:(code=GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER))){return false;}}
struct Holder {uintptr_t ap,context,bound,unused,weak,raw,proxy;};static_assert(sizeof(Holder)==56);
}
void Journal::error(const char* operation,const char* cause,uint32_t code)noexcept{
    if(operation&&std::strcmp(operation,"native-free-original")==0)original_failure_.store(true,std::memory_order_release);
    const auto ticket=tickets_.fetch_add(1,std::memory_order_relaxed)+1;auto& slot=slots_[(ticket-1)%slots_.size()];uint8_t empty=0;
    if(!slot.state.compare_exchange_strong(empty,1,std::memory_order_acquire)){dropped_.fetch_add(1,std::memory_order_relaxed);return;}
    slot.data={};slot.data.ticket=ticket;slot.data.thread=GetCurrentThreadId();slot.data.seh_code=code;text(slot.data.operation,operation);text(slot.data.original_cause,cause);
    if(operation&&std::strncmp(operation,"warn:",5)==0)slot.data.level=JournalEntry::Level::Warn;
    else if(operation&&std::strncmp(operation,"info:",5)==0)slot.data.level=JournalEntry::Level::Info;
    slot.state.store(2,std::memory_order_release);
}
size_t Journal::drain(JournalEntry* out,size_t capacity,uint64_t& dropped)noexcept{size_t count=0;if(out)for(auto& slot:slots_){uint8_t ready=2;if(count<capacity&&slot.state.compare_exchange_strong(ready,3,std::memory_order_acquire)){out[count++]=slot.data;slot.state.store(0,std::memory_order_release);}}dropped=dropped_.exchange(0,std::memory_order_acq_rel);return count;}
bool read_heap_module_receipt(HeapModuleReceipt& out,std::string& error){
    static std::mutex gate;static HeapModuleReceipt held;static bool attempted=false;static std::string failed;
    std::lock_guard lock(gate);
    if(attempted){out=held;if(!held.actual_exports){error=failed;return false;}return true;}
    attempted=true;held.process=GetCurrentProcessId();const auto crt=GetModuleHandleW(L"msvcrt.dll");
    try{
        if(!crt){failed="error: actual imported msvcrt module is not loaded";out=held;error=failed;return false;}
        if(!module(uintptr_t(crt),held.imported_crt,failed)||!exported(crt,"malloc",held.malloc_export,held.malloc_owner,failed)||!exported(crt,"free",held.free_export,held.free_owner,failed)){out=held;error=failed;return false;}
        held.actual_exports=true;out=held;return true;
    }catch(const std::exception& ex){failed=ex.what();out=held;error=failed;return false;}catch(...){failed="error: non-standard actual heap module receipt failure";out=held;error=failed;return false;}
}
namespace detail {
bool verify_live_bytes(uintptr_t address,const unsigned char* expected,size_t count,std::string& error){return bytes(address,expected,count,error);}
ExchangeReceipt replace_import_once(uintptr_t slot,uintptr_t expected,uintptr_t replacement)noexcept{
    ExchangeReceipt result;DWORD before=0,unused=0;
    if(!slot||slot%alignof(void*)||!expected||!replacement){result.win32_error=ERROR_INVALID_PARAMETER;return result;}
    if(!VirtualProtect(reinterpret_cast<void*>(slot),sizeof(void*),PAGE_READWRITE,&before)){result.win32_error=GetLastError();return result;}
    result.attempted=true;result.previous_protection=before;
    __try{result.observed=uintptr_t(InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(slot),reinterpret_cast<void*>(replacement),reinterpret_cast<void*>(expected)));result.exchanged=result.observed==expected;}
    __except((result.seh_code=GetExceptionCode(),EXCEPTION_EXECUTE_HANDLER)){result.exchanged=false;result.observed=0;}
    result.protection_restored=VirtualProtect(reinterpret_cast<void*>(slot),sizeof(void*),before,&unused)!=0;
    if(!result.protection_restored)result.win32_error=GetLastError();else if(!result.exchanged&&!result.seh_code)result.win32_error=ERROR_INVALID_STATE;
    return result;
}
}
Runtime& Runtime::instance()noexcept{alignas(Runtime) static unsigned char storage[sizeof(Runtime)];static Runtime* const runtime=new(storage)Runtime;return *runtime;}
bool Runtime::fail(Phase phase,const char* cause,std::string& error,uint32_t win32,int32_t mh,uint32_t seh)noexcept{receipt_.phase=phase;text(receipt_.cause,cause);receipt_.win32_error=win32;receipt_.minhook_status=mh;receipt_.seh_code=seh;journal_.error("native-free-runtime",cause,seh);try{error=receipt_.cause.data();}catch(const std::exception& ex){journal_.error("failure-output",ex.what(),0);}catch(...){journal_.error("failure-output","error: non-standard failure output allocation exception",0);}return false;}
bool Runtime::actual_world(WorldReceipt& out,std::string& error){
    out={};game::building_mixed_identity::opened_file::CurrentWorld actual;
    if(!game::native_open_runtime::read_current_world(actual,error))return false;
    const auto origin=game::building_mixed_identity::opened_file::status();
    if(!origin.game_thread||origin.game_thread!=GetCurrentThreadId()||!actual.generation||!actual.world_epoch||!actual.world||!actual.map){error="error: actual NativeOpen Mainthread/world/epoch witness invalid";return false;}
    game::native::WorldStatus status;if(!game::native::read_world_status(status,error))return false;
    if(status.active_mode_index!=3||!status.world_present||status.quitting||status.title_transition_pending||status.saves_in_progress!=0||status.edit_operations!=0){error="info: actual loaded world is not effects idle active_mode="+std::to_string(status.active_mode_index)+" saves="+std::to_string(status.saves_in_progress)+" edits="+std::to_string(status.edit_operations);return false;}
    game::building_mixed_identity::opened_file::CurrentWorld after;if(!game::native_open_runtime::read_current_world(after,error))return false;
    if(after.generation!=actual.generation||after.world_epoch!=actual.world_epoch||after.world!=actual.world||after.map!=actual.map){error="error: actual NativeOpen world changed during independent guards";return false;}
    out={actual.world_epoch,actual.generation,uintptr_t(actual.world),uintptr_t(actual.map),GetCurrentThreadId()};return true;
}
bool Runtime::routes(std::string& error){
    const auto base=receipt_.image;uint32_t seh=0;uintptr_t malloc_actual=0,free_actual=0;
    for(const auto& guard:contract::entries){if(!committed(base+guard.rva,guard.count,base,true,error))return false;const bool patched=guard.rva==sized_rva&&sized_visible_;if(!bytes(base+guard.rva+(patched?5:0),guard.bytes+(patched?5:0),guard.count-(patched?5:0),error))return false;}
    if(!copy(reinterpret_cast<void*>(base+malloc_iat_rva),&malloc_actual,8,seh)||!copy(reinterpret_cast<void*>(base+free_iat_rva),&free_actual,8,seh)){error="error: actual CRT IAT read SEH="+std::to_string(seh);return false;}
    if(malloc_actual!=receipt_.heap.malloc_export.resolved||free_actual!=(iat_visible_?uintptr_t(&free_entry):receipt_.heap.free_export.resolved)){error="error: actual CRT malloc/free IAT no longer matches owned routes";return false;}
    const auto crt=reinterpret_cast<HMODULE>(receipt_.heap.imported_crt.base);
    if(uintptr_t(GetProcAddress(crt,"malloc"))!=receipt_.heap.malloc_export.resolved||uintptr_t(GetProcAddress(crt,"free"))!=receipt_.heap.free_export.resolved){error="error: actual named CRT resolved export changed";return false;}
    if(!committed(base+sized_rva,5,base,true,error)||!jump(base+0xa01260,base+0xa915e8,error))return false;
    unsigned char thunk[6]{};int32_t delta=0;if(!copy(reinterpret_cast<void*>(base+0xa915e8),thunk,6,seh)){error="error: actual free import thunk SEH="+std::to_string(seh);return false;}std::memcpy(&delta,thunk+2,4);
    if(thunk[0]!=0xff||thunk[1]!=0x25||base+0xa915e8+6+intptr_t(delta)!=base+free_iat_rva){error="error: actual free import thunk route differs";return false;}
    if(sized_visible_){if(!jump(base+sized_rva,receipt_.sized_trampoline+14,error)||!abs_jump(receipt_.sized_trampoline,base+0xa01260,error)||!abs_jump(receipt_.sized_trampoline+14,uintptr_t(&sized_entry),error))return false;}
    else if(!jump(base+sized_rva,base+0xa01260,error))return false;
    return true;
}
bool Runtime::prepare_and_enable(std::string& error)try{
    if(receipt_.phase!=Phase::Idle){error="error: native Free installer is sticky; prepare never replays";return false;}
    bound_thread_.store(GetCurrentThreadId(),std::memory_order_release);
    receipt_.process=GetCurrentProcessId();receipt_.main_thread=GetCurrentThreadId();receipt_.image=uintptr_t(GetModuleHandleW(nullptr));
    if(!module(receipt_.image,receipt_.executable,error))return fail(Phase::Failed,error.c_str(),error);
    if(std::strcmp(receipt_.executable.sha256.data(),native_uid::required_exe_sha))return fail(Phase::Failed,"error: actual main executable SHA differs from MT2 exact ABI",error);
    if(!actual_world(receipt_.world,error))return fail(Phase::Failed,error.c_str(),error);
    if(!module(uintptr_t(&free_entry),receipt_.self,error)||!read_heap_module_receipt(receipt_.heap,error)||!imported(receipt_.image,"malloc",uint32_t(malloc_iat_rva),error)||!imported(receipt_.image,"free",uint32_t(free_iat_rva),error)||!committed(receipt_.image+free_iat_rva,8,receipt_.image,false,error)||!committed(receipt_.image+malloc_iat_rva,8,receipt_.image,false,error)||!routes(error))return fail(Phase::Failed,error.c_str(),error);
    for(const auto* observed:{&receipt_.executable,&receipt_.self,&receipt_.heap.imported_crt,&receipt_.heap.malloc_owner,&receipt_.heap.free_owner})if(!observed->version_present){const auto warning="warn: actual module has no fixed version resource; file SHA/path remain in receipt; win32="+std::to_string(observed->version_error);journal_.error("warn:module-version-absent",warning.c_str(),0);}
    receipt_.malloc_iat=receipt_.image+malloc_iat_rva;receipt_.free_iat=receipt_.image+free_iat_rva;receipt_.sized_target=receipt_.image+sized_rva;receipt_.crt_confirmed=true;
    void* original=nullptr;const auto created=MH_CreateHook(reinterpret_cast<void*>(receipt_.sized_target),reinterpret_cast<void*>(&sized_entry),&original);
    if(created!=MH_OK)return fail(Phase::Failed,"error: concrete sized MinHook creation failed",error,0,created);
    hook_created_=true;receipt_.sized_trampoline=uintptr_t(original);
    if(!original||!abs_jump(uintptr_t(original),receipt_.image+0xa01260,error)||!abs_jump(uintptr_t(original)+14,uintptr_t(&sized_entry),error))return fail(Phase::Failed,error.c_str(),error);
    receipt_.trampoline_route=true;observer_.emplace(receipt_.main_thread,journal_);
    if(!observer_->publish_disabled(reinterpret_cast<native_free::SizedFn>(original),reinterpret_cast<native_free::FreeFn>(receipt_.heap.free_export.resolved)))return fail(Phase::Failed,"error: typed originals disabled publication failed",error);
    published_.store(&*observer_,std::memory_order_release);receipt_.originals_published=true;receipt_.phase=Phase::Prepared;
    const auto enabled=MH_EnableHook(reinterpret_cast<void*>(receipt_.sized_target));
    if(enabled!=MH_OK)return fail(Phase::Unknown,"error: concrete sized hook enable failed; visibility requires inspection",error,0,enabled);
    sized_visible_=true;receipt_.hook_mask=1;
    const auto iat=detail::replace_import_once(receipt_.free_iat,receipt_.heap.free_export.resolved,uintptr_t(&free_entry));
    original_iat_protection_=iat.previous_protection;
    iat_visible_=iat.exchanged;receipt_.iat_protection_restored=iat.protection_restored;
    if(!iat.exchanged||!iat.protection_restored)return fail(Phase::Unknown,"error: concrete Game free IAT replace or protection restore failed",error,iat.win32_error,0,iat.seh_code);
    receipt_.hook_mask=3;
    if(!routes(error)||!actual_world(receipt_.world,error))return fail(Phase::Unknown,error.c_str(),error);
    if(!observer_->confirm_hooks(3)||!observer_->enable())return fail(Phase::Unknown,"error: actual typed hook mask3 ACK/Observer enable failed",error);
    receipt_.hooks_confirmed=true;receipt_.phase=Phase::Enabled;text(receipt_.cause,"info: actual CRT imports, sized trampoline and free IAT routes installed");return true;
}catch(const std::exception& ex){return fail(published_observer()?Phase::Unknown:Phase::Failed,ex.what(),error);}catch(...){return fail(published_observer()?Phase::Unknown:Phase::Failed,"error: non-standard actual Free installer failure",error);}
bool Runtime::read_actual_receipt(Receipt& out,std::string& error){
    if(GetCurrentThreadId()!=bound_thread_.load(std::memory_order_acquire)){out={};error="error: actual Free receipt requires bound Mainthread";return false;}
    out=status();if(receipt_.phase!=Phase::Enabled){error=receipt_.cause.data();if(error.empty())error="error: actual native Free runtime is not enabled";return false;}
    const auto availability=observer_->availability();
    if(!availability.main_thread){error="error: actual Observer availability outside bound Mainthread";return false;}
    if(availability.unknown_locked){fail(Phase::Unknown,availability.cause.data(),error,0,0,availability.seh_code);out=status();return false;}
    if(!availability.enabled||availability.active||availability.in_delete){error="info: actual Observer has an active or disabled native invocation";return false;}
    if(journal_.original_failure()){fail(Phase::Unknown,"error: actual native Free original failure locked further native effects",error);out=status();return false;}
    if(!routes(error)){fail(Phase::Unknown,error.c_str(),error);out=status();return false;}
    if(!actual_world(receipt_.world,error)){out=status();return false;}
    out=status();return out.crt_confirmed&&out.hooks_confirmed&&out.hook_mask==3&&observer_&&observer_->enabled();
}
bool Runtime::reserve_holder_generation(uint64_t& out,std::string& error){out=0;Receipt actual;if(!read_actual_receipt(actual,error))return false;if(next_holder_==UINT64_MAX){error="error: shared own holder generation exhausted";return false;}out=++next_holder_;return true;}
bool Runtime::next_free_invocation(uint64_t& out,std::string& error){out=0;Receipt actual;if(!read_actual_receipt(actual,error))return false;if(next_call_==UINT64_MAX){error="error: shared free invocation sequence exhausted";return false;}out=++next_call_;return true;}
native_free::Observer* Runtime::actual_observer()noexcept{if(GetCurrentThreadId()!=bound_thread_.load(std::memory_order_acquire)||receipt_.phase!=Phase::Enabled||journal_.original_failure()||!observer_)return nullptr;const auto a=observer_->availability();return a.main_thread&&a.enabled&&!a.active&&!a.in_delete&&!a.unknown_locked?published_observer():nullptr;}
bool Runtime::probe_null_holder(HolderKind kind,ProbeReceipt& result,std::string& error){
    result={};Receipt actual;if(probe_busy_||!read_actual_receipt(actual,error)){if(error.empty())error="error: own null holder probe reentrancy rejected";return false;}
    if(kind!=HolderKind::Prop&&kind!=HolderKind::Building){error="error: own null holder kind invalid";return false;}
    probe_busy_=true;void* holder=nullptr;uintptr_t factory=0,d0=0,link=0,weak=0;uint32_t code=0;
    try{
        if(kind==HolderKind::Prop){factory=receipt_.image+0xd8a940;d0=receipt_.image+0xbedc00;link=receipt_.image+0x17119f0;weak=receipt_.image+0x16f9e60;for(const auto& guard:prop_lease::contract::entries)if(!committed(receipt_.image+guard.rva,guard.count,receipt_.image,true,error)||!bytes(receipt_.image+guard.rva,guard.bytes,guard.count,error)){probe_busy_=false;return false;}}
        else {factory=receipt_.image+0xd8a780;d0=receipt_.image+0xbed320;link=receipt_.image+0x17116f0;weak=receipt_.image+0x16f9c20;for(const auto& guard:native_lease::contract::entries)if(!committed(receipt_.image+guard.rva,guard.count,receipt_.image,true,error)||!bytes(receipt_.image+guard.rva,guard.bytes,guard.count,error)){probe_busy_=false;return false;}}
        result.world=actual.world;if(!reserve_holder_generation(result.allocation_generation,error)){probe_busy_=false;return false;}result.factory_attempted=1;
        if(!factory_call(reinterpret_cast<Factory>(factory),holder,code)){result.seh_code=code;probe_busy_=false;return fail(Phase::Unknown,"error: own genuine null holder factory SEH; no retry/free guess",error,0,0,code);}
        result.factory_returned=1;private_probe_holder_=holder;private_probe_generation_=result.allocation_generation;Holder body{};
        if(!holder||!copy(holder,&body,sizeof body,code)||body.ap!=link||body.weak!=weak||body.context||body.bound||body.raw||body.proxy){result.seh_code=code;probe_busy_=false;return fail(Phase::Unknown,"error: own genuine null holder constructor readback differs; holder remains private",error,0,0,code);}
        result.constructor_confirmed=1;if(!next_free_invocation(result.call,error)){probe_busy_=false;return fail(Phase::Unknown,error.c_str(),error);}
        if(receipt_.world.epoch!=actual.world.epoch||receipt_.world.generation!=actual.world.generation||receipt_.world.world!=actual.world.world||receipt_.world.map!=actual.world.map){probe_busy_=false;return fail(Phase::Unknown,"error: actual world changed after own null holder allocation",error);}
        // The fresh null holder was never published to gameplay or callbacks.
        const native_free::Permit permit{result.call,result.allocation_generation,actual.world.generation,result.call,holder,56,true};
        if(!observer_->begin(permit)){probe_busy_=false;return fail(Phase::Unknown,"error: own null holder actual free permit rejected; no direct delete fallback",error);}
        const auto deleted=observer_->invoke_delete(reinterpret_cast<native_free::DeleteFn>(d0),permit);result.deletion=observer_->observe(permit);probe_busy_=false;
        if(!deleted)return fail(Phase::Unknown,result.deletion.cause.data(),error,0,0,result.deletion.seh_code);
        private_probe_holder_=nullptr;private_probe_generation_=0;result.confirmed=true;text(result.cause,"info: genuine native null holder Factory/readback/D0/Free roundtrip confirmed");return true;
    }catch(const std::exception& ex){probe_busy_=false;text(result.cause,ex.what());return fail(result.factory_attempted?Phase::Unknown:Phase::Failed,ex.what(),error);}catch(...){probe_busy_=false;return fail(result.factory_attempted?Phase::Unknown:Phase::Failed,"error: non-standard genuine null holder probe failure",error);}
}
bool Runtime::retire_routes(std::string& error){
    if(GetCurrentThreadId()!=receipt_.main_thread||probe_busy_||entry_inflight_.load(std::memory_order_acquire)||(observer_&&observer_->original_inflight())){error="info: native Free entries/originals are not retired";return false;}
    if(receipt_.phase!=Phase::Enabled&&receipt_.phase!=Phase::Unknown&&receipt_.phase!=Phase::Prepared){error="error: native Free route retirement state invalid";return false;}
    if(observer_&&!observer_->disable()){error="error: own free Observer still has an active invocation";return false;}
    // Inspect physical routes before any reversal, including failed enable/CAS paths.
    uintptr_t actual_iat=0;uint32_t code=0;
    if(!receipt_.free_iat||!copy(reinterpret_cast<void*>(receipt_.free_iat),&actual_iat,8,code))return fail(Phase::Unknown,"error: actual free IAT retirement read failed",error,0,0,code);
    if(actual_iat==uintptr_t(&free_entry))iat_visible_=true;
    else if(actual_iat==receipt_.heap.free_export.resolved)iat_visible_=false;
    else return fail(Phase::Unknown,"error: actual free IAT belongs to an unknown/foreign route; no blind reversal",error);
    std::string sized_error;
    if(jump(receipt_.sized_target,receipt_.image+0xa01260,sized_error))sized_visible_=false;
    else if(receipt_.sized_trampoline&&jump(receipt_.sized_target,receipt_.sized_trampoline+14,sized_error))sized_visible_=true;
    else return fail(Phase::Unknown,"error: actual sized code belongs to an unknown/partial route; no blind reversal",error);
    if(iat_visible_){const auto restored=detail::replace_import_once(receipt_.free_iat,uintptr_t(&free_entry),receipt_.heap.free_export.resolved);if(!restored.exchanged||!restored.protection_restored)return fail(Phase::Unknown,"error: own Game free IAT retirement failed",error,restored.win32_error,0,restored.seh_code);iat_visible_=false;}
    if(original_iat_protection_){DWORD unused=0;if(!VirtualProtect(reinterpret_cast<void*>(receipt_.free_iat),8,original_iat_protection_,&unused))return fail(Phase::Unknown,"error: own Game free IAT initial protection retirement failed",error,GetLastError());}
    if(sized_visible_){const auto disabled=MH_DisableHook(reinterpret_cast<void*>(receipt_.sized_target));if(disabled!=MH_OK&&disabled!=MH_ERROR_DISABLED)return fail(Phase::Unknown,"error: own sized MinHook retirement failed",error,0,disabled);sized_visible_=false;}
    receipt_.hook_mask=0;receipt_.hooks_confirmed=false;if(!routes(error))return fail(Phase::Unknown,error.c_str(),error);
    receipt_.phase=Phase::Retired;text(receipt_.cause,"info: own native Free routes retired; originals, modules and trampoline retained for late entries");return true;
}
Receipt Runtime::status()const noexcept{const auto thread=bound_thread_.load(std::memory_order_acquire);if(!thread)return {};if(GetCurrentThreadId()!=thread){Receipt rejected;rejected.phase=Phase::Failed;text(rejected.cause,"error: actual Free status requires bound Mainthread");return rejected;}auto out=receipt_;out.entry_inflight=entry_inflight_.load(std::memory_order_acquire);out.original_inflight=observer_?observer_->original_inflight():0;return out;}
bool Runtime::can_reclaim_global_minhook(std::string& error)const{if(!bound_thread_.load(std::memory_order_acquire))return true;error="error: native Free preparation owns process-lifetime storage/trampoline state; global MinHook cleanup is forbidden";return false;}
}
