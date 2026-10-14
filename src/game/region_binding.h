#pragma once
#include <string>

namespace mt2mp::game::ui_region_runtime::region_binding {
template<class Origin,class Binding>
bool validate(const Origin& native,const Binding& binding,const std::string& common_snapshot,
              const std::string& requested_snapshot,std::string& error) {
    // NativeOpenedfileOrigin identifies the local file load. Action snapshot IDs
    // identify the shared session baseline, which may be captured after that load.
    if(!binding.native_epoch||native.world_epoch!=binding.native_epoch){
        error="region native epoch actual="+std::to_string(native.world_epoch)+" required="+std::to_string(binding.native_epoch);return false;
    }
    if(!binding.native_generation||native.generation!=binding.native_generation){
        error="region native generation actual="+std::to_string(native.generation)+" required="+std::to_string(binding.native_generation);return false;
    }
    if(common_snapshot.empty()||requested_snapshot.empty()||common_snapshot!=requested_snapshot){
        error="region common snapshot actual="+(common_snapshot.empty()?std::string("<empty>"):common_snapshot)+
            " required="+(requested_snapshot.empty()?std::string("<empty>"):requested_snapshot);return false;
    }
    return true;
}
}
