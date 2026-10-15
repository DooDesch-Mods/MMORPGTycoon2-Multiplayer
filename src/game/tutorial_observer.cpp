#include "tutorial_observer.h"
#include "native_open_runtime.h"
#include "tutorial_replication/native.h"
#include <windows.h>
#include <cstring>
#include <limits>

namespace mt2mp::game::tutorial_observer {
namespace {
DWORD copy_native(uintptr_t address,void* target,size_t count){
    __try{std::memcpy(target,reinterpret_cast<const void*>(address),count);return 0;}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}
}
class NativePort final:public detail::Port {
public:
    bool current(detail::Origin& out,std::string& error)override{
        building_mixed_identity::opened_file::CurrentWorld world;
        if(!native_open_runtime::read_current_world(world,error))return false;
        out={reinterpret_cast<uintptr_t>(world.game),reinterpret_cast<uintptr_t>(world.state),
             reinterpret_cast<uintptr_t>(world.world),reinterpret_cast<uintptr_t>(world.map),world.world_epoch,world.generation,world.uuid};
        return true;
    }
    bool image(uintptr_t& out,std::string& error)override{
        out=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if(out)return true;error="tutorial native game module unavailable";return false;
    }
    bool owned_sequence_entry(uintptr_t address,std::string& error)override{return tutorial_replication::verify_sequence_entry(address,error);}
    bool copy(uintptr_t address,void* target,size_t count,std::string& error)override{
        if(!address||!target||!count||address>std::numeric_limits<uintptr_t>::max()-count){error="tutorial memory range invalid";return false;}
        const auto end=address+count;auto cursor=address;
        while(cursor<end){MEMORY_BASIC_INFORMATION info{};
            if(!VirtualQuery(reinterpret_cast<const void*>(cursor),&info,sizeof(info))||info.State!=MEM_COMMIT||
               (info.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(info.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))){error="tutorial native memory not readable";return false;}
            const auto base=reinterpret_cast<uintptr_t>(info.BaseAddress);
            if(base>std::numeric_limits<uintptr_t>::max()-info.RegionSize||base+info.RegionSize<=cursor){error="tutorial native memory region range invalid";return false;}
            cursor=base+info.RegionSize;
        }
        const auto exception=copy_native(address,target,count);
        if(exception){error="tutorial native memory copy exception="+std::to_string(exception);return false;}
        return true;
    }
};
}
bool read(nlohmann::json& result,std::string& error){NativePort port;return detail::observe(port,result,error);}
}
