#pragma once
#include <cstdint>
#include <string>
namespace mt2mp::game::dungeon_sound_record {
template<class Read,class Text>bool grid(Read& at,Text& text,uintptr_t image,uintptr_t window,uintptr_t object,std::string& error){
    uintptr_t ap=0,owner=0,binding=0;std::string command;
    if(!at(object,0,ap,error)||!at(object,0x280,owner,error)||!at(object,0x3b0,binding,error))return false;
    if(ap!=image+0x16dade0){error="Dungeon sound actual atmosphere Grid class differs";return false;}
    if(owner!=window){error="Dungeon sound Grid command targets another native Window";return false;}
    if(binding){error="Dungeon sound Grid has a native gameplay selection binding";return false;}
    if(!text(object,0x360,command,error))return false;
    if(command!="MessageTo Window SetAtmos"){error="Dungeon sound Grid command differs from the native atmosphere action";return false;}return true;
}
inline bool replay(uintptr_t receiver,int32_t atmosphere,uintptr_t expected_receiver,int32_t expected_atmosphere,bool preparation,uint32_t& calls,bool& execute,std::string& error){
    execute=false;if(receiver!=expected_receiver||atmosphere!=expected_atmosphere||calls){error="Dungeon sound original-once owner/argument/count differs";return false;}
    ++calls;execute=!preparation;return true;
}
template<class Read>bool read(Read& at,uintptr_t record,int32_t& value,std::string& error){
    uintptr_t tokens=0;int32_t count=0,capacity=0,type=0;
    if(!at(record,0x38,tokens,error)||!at(record,0x40,count,error)||!at(record,0x44,capacity,error))return false;
    if(!tokens||count!=1||capacity<count){error="Dungeon sound normal UI record requires one native token";return false;}
    if(!at(tokens,0,type,error)||!at(tokens,0x28,value,error))return false;
    if(type!=4){error="Dungeon sound normal UI argument is not an integer token";return false;}
    if(value<0||value>=6){error="Dungeon sound atmosphere is outside the six native rows";return false;}return true;
}
}
