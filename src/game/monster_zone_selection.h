#pragma once
#include "npc_quest_targets.h"
#include <array>
#include <set>
namespace mt2mp::game::monster_zone_selection {
namespace q=npc_quest_targets;
struct Catalogue {
    uintptr_t manager=0,data=0,character=0;
    int32_t count=0,capacity=0,id=-1;
    bool operator==(const Catalogue&)const=default;
};
template<class Memory>
bool read(Memory& m,uintptr_t image,int32_t id,Catalogue& out,std::string& e){
    out={};out.id=id;
    if(id<0||!q::at(m,image,0x13df7a0,out.manager,e)||
       !q::typed(m,image,out.manager,0x1732430,e)||
       !q::typed(m,image,out.manager+0xd0,0x16eb860,e)||
       !q::at(m,out.manager,0xd8,out.data,e)||!q::at(m,out.manager,0xe0,out.count,e)||
       !q::at(m,out.manager,0xe4,out.capacity,e))return false;
    if(out.count<1||out.count>4096||out.capacity<out.count||!out.data||id>=out.count)
        return q::fail(e,"Monster catalogue bounds or requested index differs");
    std::set<uintptr_t> pointers;std::set<int32_t> ids;
    for(int32_t i=0;i<out.count;++i){
        uintptr_t character=0;std::array<int32_t,2> key{};
        if(!q::at(m,out.data,size_t(i)*8,character,e)||!q::typed(m,image,character,0x1705990,e)||
           !q::at(m,character,0x2c,key,e))return false;
        if(!pointers.insert(character).second||key[0]!=1||key[1]!=i||!ids.insert(key[1]).second)
            return q::fail(e,"Monster catalogue member identity differs");
        if(i==id)out.character=character;
    }
    uintptr_t manager=0,data=0;int32_t count=0,capacity=0;
    return q::at(m,image,0x13df7a0,manager,e)&&q::at(m,out.manager,0xd8,data,e)&&
        q::at(m,out.manager,0xe0,count,e)&&q::at(m,out.manager,0xe4,capacity,e)&&
        ((manager==out.manager&&data==out.data&&count==out.count&&capacity==out.capacity&&out.character)||
         q::fail(e,"Monster catalogue changed during bounded read"));
}
struct Binding {
    uintptr_t brush=0,window=0,character=0,image=0;
    uint32_t thread=0;
    bool operator==(const Binding&)const=default;
};
// This scope belongs to one original AddPaint on a private constructed brush.
// It never changes the visible ThemeWindow selection.
class Scope {
    const Binding*& slot_;const Binding* previous_;
public:
    Scope(const Binding*& slot,const Binding& value):slot_(slot),previous_(slot){slot_=&value;}
    ~Scope(){slot_=previous_;}
    Scope(const Scope&)=delete;Scope& operator=(const Scope&)=delete;
};
class PaintScope {
    uintptr_t& brush_;bool& active_;uintptr_t prior_brush_;bool prior_active_;
public:
    PaintScope(uintptr_t& brush,bool& active,uintptr_t value):brush_(brush),active_(active),
        prior_brush_(brush),prior_active_(active){brush_=value;active_=true;}
    ~PaintScope(){brush_=prior_brush_;active_=prior_active_;}
    PaintScope(const PaintScope&)=delete;PaintScope& operator=(const PaintScope&)=delete;
};
inline bool matches(const Binding* p,uintptr_t private_brush,uintptr_t receiver,
                    uintptr_t caller,uint32_t thread,bool inside_paint){
    if(!p||!inside_paint||!p->brush||p->brush!=private_brush||!p->character||
       !p->window||p->window!=receiver||!p->thread||p->thread!=thread||!p->image)return false;
    return caller==p->image+0x5f1842||caller==p->image+0x5f1956||caller==p->image+0x5f1989;
}
}
