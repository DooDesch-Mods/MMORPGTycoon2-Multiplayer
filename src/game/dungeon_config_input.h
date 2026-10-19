#pragma once
#include <cstdint>
#include <string>
namespace mt2mp::game::dungeon_config_input {
template<class Read>bool spinner(Read& read,uintptr_t image,uintptr_t object,std::string& error){
    uintptr_t ap=0,binding=0;
    if(!object||!read(object,0,ap,error)||ap!=image+0x16c1720){error="Dungeon config named instances_count Spinner address point differs";return false;}
    if(!read(object,0x428,binding,error))return false;if(binding){error="Dungeon config instances_count Spinner float gameplay binding is present";return false;}
    if(!read(object,0x430,binding,error))return false;if(binding){error="Dungeon config instances_count Spinner integer gameplay binding is present";return false;}return true;
}
}
