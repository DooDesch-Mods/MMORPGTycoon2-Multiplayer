#include "dungeon_config_input.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
int main()try{constexpr uintptr_t image=0x140000000;std::array<unsigned char,0x438> bytes{};const auto object=uintptr_t(bytes.data());std::string error;unsigned checks=0;
    const auto put=[&](size_t offset,uintptr_t value){std::memcpy(bytes.data()+offset,&value,8);};
    auto read=[](uintptr_t base,size_t offset,auto& value,std::string&){std::memcpy(&value,reinterpret_cast<const void*>(base+offset),sizeof value);return true;};
    const auto check=[&](bool ok){++checks;if(!ok)throw std::runtime_error("Dungeon UI input guard comparison "+std::to_string(checks));};
    check(!mt2mp::game::dungeon_config_input::spinner(read,image,0,error));put(0,image+0x16c1720);check(mt2mp::game::dungeon_config_input::spinner(read,image,object,error));
    put(0,image+0x1787a30);check(!mt2mp::game::dungeon_config_input::spinner(read,image,object,error)&&error.find("address point")!=std::string::npos);put(0,image+0x16c1720);
    put(0x428,1);check(!mt2mp::game::dungeon_config_input::spinner(read,image,object,error)&&error.find("float gameplay binding")!=std::string::npos);put(0x428,0);
    put(0x430,1);check(!mt2mp::game::dungeon_config_input::spinner(read,image,object,error)&&error.find("integer gameplay binding")!=std::string::npos);put(0x430,0);
    check(mt2mp::game::dungeon_config_input::spinner(read,image,object,error));std::cout<<checks<<" exact Dungeon Spinner input binding comparisons passed\n";return 0;
}catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}
