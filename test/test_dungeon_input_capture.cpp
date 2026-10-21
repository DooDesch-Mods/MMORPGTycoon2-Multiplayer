#include "dungeon_gizmo_move_read.h"
#include "dungeon_input_capture.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace dungeon_capture_fixture {
namespace d=mt2mp::game::dungeon_input_capture;
constexpr uintptr_t image=0x140000000,cursor=0x200000000,receiver=0x210000000,data=0x220000000,ghost=0x230000000,position=0x240000000;
unsigned checks=0;
void expect(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;std::map<uintptr_t,std::string> strings;
    template<class T>void put(uintptr_t p,const T& value){const auto* begin=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[p+i]=begin[i];}
    bool copy(uintptr_t p,void* output,size_t count,std::string& error){auto* target=static_cast<unsigned char*>(output);
        for(size_t i=0;i<count;++i){const auto found=bytes.find(p+i);if(found==bytes.end()){error="controlled native read failed at "+std::to_string(p+i);return false;}target[i]=found->second;}return true;}
    template<class T>bool at(uintptr_t p,size_t offset,T& value,std::string& error){return copy(p+offset,&value,sizeof value,error);}
    bool text(uintptr_t p,size_t offset,std::string& value,std::string& error){const auto found=strings.find(p+offset);
        if(found==strings.end()){error="controlled native text missing";return false;}value=found->second;return true;}
};
Memory fixture(bool key){
    Memory memory;memory.put(image+0x13df9d0,cursor);memory.put(cursor,image+0x1786dd0);memory.put(cursor+0x190,receiver);
    memory.put(receiver,image+(key?0x173d330:0x173e870));const auto source=key?ghost+0x100:receiver+0x388;
    memory.put(source,std::array<float,4>{0,0,0,1});memory.put(source+0x10,std::array<float,3>{10,20,30});memory.put(source+0x1c,std::array<float,3>{1,1,1});
    memory.put(position,std::array<float,3>{10,20,30});
    if(key){memory.put(receiver+0x6e0,uint8_t(0));memory.put(receiver+0x6e1,uint8_t(1));memory.put(receiver+0x740,ghost);
        memory.strings[receiver+0x168]="native-key";memory.strings[receiver+0x188]="native-key-variant";}
    else {memory.put(receiver+0x468,int32_t(1));memory.put(receiver+0x178,data);memory.put(receiver+0x180,int32_t(2));memory.put(receiver+0x184,int32_t(2));memory.put(receiver+0x3f4,uint8_t(1));
        memory.strings[data+0x40]="native-door";memory.strings[data+0x60]="native-door-variant";}
    return memory;
}
bool read(Memory& memory,bool key,d::Purchase& out,std::string& error,uintptr_t caller=0,uintptr_t type=0){
    auto at=[&](uintptr_t p,size_t offset,auto& value,std::string& cause){return memory.at(p,offset,value,cause);};
    auto copy=[&](uintptr_t p,void* output,size_t count,std::string& cause){return memory.copy(p,output,count,cause);};
    auto text=[&](uintptr_t p,size_t offset,std::string& value,std::string& cause){return memory.text(p,offset,value,cause);};
    return d::purchase(image,caller?caller:image+(key?0x5a636a:0x5a34b2),type?type:(key?receiver+0x168:data+0x40),position,at,copy,text,out,error);
}
void run(){std::string error;d::Purchase out;
    auto memory=fixture(false);expect(read(memory,false,out,error)&&out.receiver==receiver&&out.source==receiver+0x388&&
        out.intent.type=="native-door"&&out.intent.variant=="native-door-variant"&&!out.clear_placement,"actual selected Door ListData and cached transform captured");
    expect(!read(memory,false,out,error,image+0x147a25),"foreign Purchase caller rejected without fallback");
    expect(!read(memory,false,out,error,0,data),"another valid ListData row cannot replace actual selected row");
    memory=fixture(false);memory.put(receiver+0x468,int32_t(-1));expect(!read(memory,false,out,error),"missing selected Door row rejected");
    memory=fixture(false);memory.put(receiver+0x184,int32_t(1));expect(!read(memory,false,out,error),"invalid native ListData capacity rejected");
    memory=fixture(false);memory.put(receiver+0x3f4,uint8_t(0));expect(!read(memory,false,out,error),"illegal original Door preview cannot submit");
    memory=fixture(false);memory.put(receiver,image+0x173d330);expect(!read(memory,false,out,error),"foreign active cursor class rejected");
    memory=fixture(false);memory.bytes.erase(cursor+0x190);expect(!read(memory,false,out,error)&&error.starts_with("controlled native read failed"),"original read cause preserved");
    memory=fixture(false);memory.put(position,std::array<float,3>{11,20,30});expect(!read(memory,false,out,error),"Purchase argument must match actual cached transform");
    memory=fixture(true);expect(read(memory,true,out,error)&&out.clear_placement&&out.source==ghost+0x100&&
        out.intent.tool==mt2mp::action_session::DungeonInputTool::Key&&out.intent.type=="native-key","actual Key completed placement binds original Clear path");
    memory=fixture(true);memory.put(receiver,image+0x1732980);
    expect(read(memory,true,out,error,image+0x5b7f9a)&&out.treasure&&out.clear_placement&&
        out.intent.tool==mt2mp::action_session::DungeonInputTool::Treasure&&out.source==ghost+0x100,
        "actual Treasure Update binds selected original resource and completed PlacementLogic");
    memory.put(cursor+0x190,ghost);expect(!read(memory,true,out,error,image+0x5b7f9a),"Treasure foreign active receiver rejected");
    memory=fixture(true);memory.put(receiver,image+0x1732980);memory.put(receiver+0x6e1,uint8_t(0));
    expect(!read(memory,true,out,error,image+0x5b7f9a),"Treasure incomplete placement rejected before Purchase");
    memory=fixture(true);memory.put(receiver,image+0x1732980);
    expect(!read(memory,true,out,error,image+0x5a636a),"Treasure receiver cannot use Key caller admission");
    memory=fixture(true);
    memory.put(receiver+0x6e0,uint8_t(1));expect(!read(memory,true,out,error),"ongoing Key placement cannot call original Clear assertion path");
    memory=fixture(true);memory.put(receiver+0x6e1,uint8_t(0));expect(!read(memory,true,out,error),"absent completed Key placement cannot submit");
    memory=fixture(true);memory.put(receiver+0x740,uintptr_t(0));expect(!read(memory,true,out,error),"missing actual Key preview rejected");
    memory=fixture(true);expect(!read(memory,true,out,error,0,receiver+0x188),"Key variant pointer cannot masquerade as selected type");
    memory=fixture(true);memory.strings[receiver+0x168]=std::string("bad\0key",7);expect(!read(memory,true,out,error),"native resource embedded NUL rejected");
}
}
#include "gizmo_move_read_cases.inc"
int main(){try{const auto control_check=[](bool ok,const char* why){dungeon_capture_fixture::expect(ok,why);};gizmo_move_read_fixture::run(control_check);using namespace dungeon_capture_fixture;run();std::cout<<checks<<" actual Dungeon input capture checks passed\n";return 0;}
catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
