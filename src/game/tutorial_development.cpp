#include "tutorial_development.h"
#include "native_open_runtime.h"
#include "placement_ui_probe/ui_probe.h"
#include <windows.h>

namespace mt2mp::game::tutorial_development {
namespace {
class NativePort final:public detail::Port {
    placement_ui_probe::ProcessMemory memory_;
public:
    bool current(detail::Origin& out,std::string& error)override{return native_open_runtime::read_current_world(out,error);}
    bool image(uintptr_t& out,std::string& error)override{
        out=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if(out)return true;error="development native image unavailable";return false;
    }
    bool copy(uintptr_t address,void* out,size_t count,std::string& error)override{return memory_.copy(address,out,count,error);}
};
}
bool read(nlohmann::json& result,std::string& error){NativePort port;return detail::observe(port,result,error);}
}
