#include "world_prop_reader.h"
namespace mt2mp::game::native_open_runtime {
bool read_current_world(building_mixed_identity::opened_file::CurrentWorld&,std::string&);
}
namespace mt2mp::build_actions::private_native::host {
bool RootSceneryReadBatch::read_current(opened::CurrentWorld& out,std::string& error){
    bool reused=false;
    if(!reuse_current(out,reused,error))return false;
    if(reused)return true;
    if(!mt2mp::game::native_open_runtime::read_current_world(out,error))return reject_current(error);
    return remember_current(out,error);
}
bool NativeOrigin::current(opened::CurrentWorld& out,std::string& error){return RootSceneryReadBatch::read_current(out,error);}
}
