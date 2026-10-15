#include "welcome_close.h"
#include "tutorial_observer.h"
#include "native_open_runtime.h"
#include "hud_time_runtime.h"
#include <windows.h>
#include <array>
#include <cstring>
#include <limits>
#include <set>

namespace mt2mp::game::tutorial_welcome {
namespace {
using World=building_mixed_identity::opened_file::CurrentWorld;
bool fail(std::string& e,const std::string& s){e=s;return false;}
DWORD copy(uintptr_t address,void* out,size_t bytes){
    __try{std::memcpy(out,reinterpret_cast<const void*>(address),bytes);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
bool bytes(uintptr_t address,void* out,size_t count,std::string& e){
    if(!address||count>4096||address>std::numeric_limits<uintptr_t>::max()-count)return fail(e,"welcome read range invalid");
    for(auto at=address;at<address+count;){MEMORY_BASIC_INFORMATION info{};
        if(!VirtualQuery(reinterpret_cast<void*>(at),&info,sizeof(info))||info.State!=MEM_COMMIT||
            (info.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(info.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return fail(e,"welcome memory unreadable");
        const auto base=reinterpret_cast<uintptr_t>(info.BaseAddress);
        if(base>std::numeric_limits<uintptr_t>::max()-info.RegionSize||base+info.RegionSize<=at)return fail(e,"welcome memory region overflow");
        at=base+info.RegionSize;
    }
    const auto code=copy(address,out,count);return !code||fail(e,"welcome read exception="+std::to_string(code));
}
template<class T>bool at(uintptr_t owner,size_t offset,T& out,std::string& e){
    if(owner>std::numeric_limits<uintptr_t>::max()-offset)return fail(e,"welcome owner offset overflow");
    return bytes(owner+offset,&out,sizeof(out),e);
}
bool literal(uintptr_t owner,size_t offset,const char* expected,std::string& e){
    uintptr_t data=0;uint64_t size=0;const auto length=std::strlen(expected);
    if(!at(owner,offset,data,e)||!at(owner,offset+8,size,e))return false;
    if(size!=length||length>=64)return fail(e,"welcome native string differs");
    std::array<char,64> actual{};return bytes(data,actual.data(),length+1,e)&&(std::memcmp(actual.data(),expected,length+1)==0||fail(e,"welcome native string differs"));
}
bool world(const Request& request,World& out,std::string& e){
    if(!native_open_runtime::read_current_world(out,e))return false;
    return (out.uuid==request.snapshot&&out.world_epoch==request.epoch&&out.generation==request.generation)||fail(e,"welcome expected origin differs");
}
struct Pin {uintptr_t state=0,log=0,instance=0,sequence=0,step=0,window=0,show=0;bool operator==(const Pin&)const=default;};
bool pin(const Request& request,uintptr_t image,Pin& p,std::string& e){
    const char* message=request.index==4?"welcome":request.index==103?"allDone":nullptr;
    if(!message)return fail(e,"tutorial close supports only welcome4 or allDone103");
    World current;if(!world(request,current,e))return false;p.state=reinterpret_cast<uintptr_t>(current.state);
    uintptr_t singleton=0,active=0;int32_t count=0;
    if(!at(p.state,0x1e8,p.log,e)||!at(image,0x13df330,singleton,e)||!at(p.log,0x20,active,e)||!at(p.log,0x28,count,e))return false;
    if(p.log!=singleton||count<1||count>64)return fail(e,"welcome scenario owner/count differs");
    for(int32_t i=0;i<count;++i){uintptr_t candidate=0;if(!at(active,size_t(i)*8,candidate,e))return false;
        uintptr_t data=0;uint64_t size=0;if(!at(candidate,0x18,data,e)||!at(candidate,0x20,size,e))return false;
        if(size!=16)continue;
        std::array<char,17> id{};if(!bytes(data,id.data(),id.size(),e))return false;
        if(std::memcmp(id.data(),"initial_tutorial",17))continue;
        if(p.instance)return fail(e,"welcome duplicate tutorial owner");p.instance=candidate;
    }
    uintptr_t type=0,steps=0,indexed=0,window_type=0,logic=0;int32_t next=0,step_count=0;uint8_t complete=0,target=0,locked=0,instance_complete=0,sequence_complete=0;
    if(!p.instance)return fail(e,"welcome tutorial absent");
    if(!at(p.instance,0,type,e)||type!=image+0x173ce70)return fail(e,e.empty()?"welcome instance type differs":e);
    if(!at(p.instance,0x38,instance_complete,e)||!at(p.instance,0x58,p.sequence,e)||!at(p.sequence,0,type,e)||!at(p.sequence,0x54,sequence_complete,e)||!at(p.sequence,0x48,p.step,e)||!at(p.sequence,0x50,next,e)||!at(p.sequence,0x28,step_count,e)||!at(p.sequence,0x20,steps,e))return false;
    if(instance_complete||sequence_complete||type!=image+0x16dbd80||step_count!=107||next!=request.index+1)return fail(e,"tutorial close active sequence/count/index differs required="+std::to_string(request.index));
    if(!at(steps,size_t(request.index)*8,indexed,e))return false;
    if(p.step!=indexed)return fail(e,"tutorial close current indexed pointer differs");
    if(!at(p.step,0,type,e)||!at(p.step,0x18,complete,e)||!literal(p.step,0x20,message,e)||!at(p.step,0x1a8,p.window,e))return false;
    if(type!=image+0x16fda00||complete||!p.window)return fail(e,"welcome step type/completion/window differs");
    if(!at(p.window,0,window_type,e)||!at(window_type,0x1d0,p.show,e)||!at(p.window,0x45f,target,e)||!at(p.window,0x377,locked,e)||!at(p.window,0x318,logic,e))return false;
    if(p.show!=image+0x81ec20||target!=1||locked)return fail(e,"welcome Show slot/visible target/close lock differs");
    if(logic){uintptr_t logic_type=0,command=0;
        if(!at(logic,0,logic_type,e)||!at(logic_type,0xc0,command,e))return false;
        if(command!=image+0x822710)return fail(e,"welcome window has custom command interception");
    }
    return true;
}
DWORD hide(const Pin& p){
    __try{reinterpret_cast<void(*)(void*,bool,bool)>(p.show)(reinterpret_cast<void*>(p.window),false,false);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
}
bool close(const Request& request,nlohmann::json& receipt,std::string& e){
    receipt={{"attempted",false},{"confirmed",false},{"unknown",false}};e.clear();
    try{
        if((request.index!=4&&request.index!=103)||!request.epoch||!request.generation||request.snapshot.empty()||request.token.empty()||request.token.size()>64)return fail(e,"tutorial close requires exact origin/index4 or103 and bounded token");
        // Mainthread serializes this operation. Keep used tokens for the process lifetime,
        // including failed calls, so a lost receipt cannot replay an uncertain mutation.
        static std::set<std::string> used;
        if(used.contains(request.token))return fail(e,"welcome token already attempted; read current tutorial before another operation");
        if(used.size()>=64)return fail(e,"welcome diagnostic token limit reached required<64");
        nlohmann::json observed;if(!tutorial_observer::read(observed,e))return false;
        World current;if(!world(request,current,e))return false;
        const auto image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        constexpr unsigned char guard[]={0x84,0xd2,0x75,0x09,0x80,0xb9,0x77,0x03,0,0,0,0x75,0x23};
        std::array<unsigned char,sizeof(guard)> actual{};
        if(!bytes(image+0x81ec20,actual.data(),actual.size(),e))return false;
        if(std::memcmp(actual.data(),guard,actual.size())&&!hud_time_runtime::verify_window_show_hook(image+0x81ec20,e))return false;
        Pin before,again;if(!pin(request,image,before,e)||!pin(request,image,again,e))return false;
        if(before!=again)return fail(e,"welcome native owner changed before call");
        used.insert(request.token);receipt["attempted"]=true;
        const auto exception=hide(before);
        if(exception){receipt["unknown"]=true;return fail(e,"welcome Show call exception="+std::to_string(exception));}
        World after;uint8_t target=1;
        if(!world(request,after,e)||after.state!=current.state||after.world!=current.world||after.map!=current.map||!at(before.window,0x45f,target,e)){
            receipt["unknown"]=true;if(e.empty())e="welcome origin changed after call";return false;
        }
        if(target!=0){receipt["unknown"]=true;return fail(e,"welcome native hide target readback differs required=0");}
        receipt["confirmed"]=true;receipt["effect"]=request.index==4?"welcome_window_hide_requested":"allDone_window_hide_requested";
        receipt["progression"]="await_normal_native_update";receipt["before"]=std::move(observed);return true;
    }catch(const std::exception& exception){receipt["unknown"]=receipt.value("attempted",false);return fail(e,std::string("welcome close exception: ")+exception.what());}
}
}

#include "tutorial_network_tab.inc"
