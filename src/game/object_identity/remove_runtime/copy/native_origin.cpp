#include "world_prop_reader.h"
namespace mt2mp::game::native_open_runtime {
bool read_current_world(building_mixed_identity::opened_file::CurrentWorld&,std::string&);
}
namespace mt2mp::build_actions::private_native::host {
bool NativeOrigin::current(opened::CurrentWorld& out,std::string& error){
    return mt2mp::game::native_open_runtime::read_current_world(out,error);
}
}
