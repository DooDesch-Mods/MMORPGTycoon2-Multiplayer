#include "dungeon_sound_record.h"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
int main()try{unsigned checks=0;std::string error;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    constexpr uintptr_t image=0x140000000,window=42;std::array<unsigned char,0x3b8> object{};std::string command="MessageTo Window SetAtmos";
    const auto put=[&](size_t offset,uintptr_t value){std::memcpy(object.data()+offset,&value,sizeof value);};
    put(0,image+0x16dade0);put(0x280,window);put(0x3b0,0);
    auto read=[](uintptr_t base,size_t offset,auto& value,std::string&){std::memcpy(&value,reinterpret_cast<const void*>(base+offset),sizeof value);return true;};
    auto text=[&](uintptr_t base,size_t offset,std::string& out,std::string& e){if(base!=uintptr_t(object.data())||offset!=0x360){e="wrong command field";return false;}out=command;return true;};
    const auto sample=[&]{return mt2mp::game::dungeon_sound_record::grid(read,text,image,window,uintptr_t(object.data()),error);};
    check(sample(),"Actual native command binding rejected");
    put(0x280,window+1);check(!sample()&&error.find("another native Window")!=std::string::npos,"Foreign callback Window accepted");put(0x280,window);
    put(0x3b0,1);check(!sample()&&error.find("gameplay selection binding")!=std::string::npos,"Gameplay selection binding accepted");put(0x3b0,0);
    put(0,image+0x1787a30);check(!sample()&&error.find("class")!=std::string::npos,"Foreign UI class accepted");put(0,image+0x16dade0);
    for(const auto* value:{"MessageTo Window Destroy","MessageTo Parent SetAtmos","MessageTo Window SetAtmos 1",""}){command=value;check(!sample()&&error.find("Grid command differs")!=std::string::npos,"Foreign/extra command accepted");}
    command="MessageTo Window SetAtmos";auto failed_text=[](uintptr_t,size_t,std::string&,std::string& e){e="original command read cause";return false;};
    check(!mt2mp::game::dungeon_sound_record::grid(read,failed_text,image,window,uintptr_t(object.data()),error)&&error=="original command read cause","Native command read failure lost");
    auto failed_read=[](uintptr_t,size_t,auto&,std::string& e){e="original Grid read cause";return false;};
    check(!mt2mp::game::dungeon_sound_record::grid(failed_read,text,image,window,uintptr_t(object.data()),error)&&error=="original Grid read cause","Native Grid read failure lost");
    std::cout<<checks<<" actual Dungeon sound Grid command/receiver/binding comparisons passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}
