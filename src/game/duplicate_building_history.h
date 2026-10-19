#pragma once
#include "npc_quest_targets.h"
#include "duplicate_building_values.h"
namespace mt2mp::game::duplicate_building_history {
namespace q=npc_quest_targets;
enum class Form:uint8_t {Art=1,Functional=2,Modular=3};
struct Saved {
    Form form=Form::Functional;
    uint64_t uid=0,parent_uid=0;
    uintptr_t root=0,dungeon=0,model=0;
    int32_t state=0;uint8_t user_set=0;
    std::array<uint8_t,0x6c> transform{};
    std::string name,type,variant,theme;
    std::string fluff_sha,layout_sha,plan_sha,bugs_sha;
    bool operator==(const Saved&)const=default;
};
template<class Memory,class Text,class Fluff,class Record,class Theme,class Model,class Find>
bool live(Memory& memory,uintptr_t image,uintptr_t building,Text&& text,Fluff&& fluff,Record&& record,
          Theme&& theme,Model&& model,Find&& find,Saved& out,std::string& e){
    out={};uintptr_t actual=0,layout=0,plan=0,bugs=0,parent=0;uint8_t art=0;
    if(!q::typed(memory,image,building,0x16d93c0,e)||!q::at(memory,building,0x18,out.uid,e)||!out.uid||
       !find(out.uid,actual,e)||actual!=building||!q::at(memory,building,0xcba,art,e)||art>1||
       !q::at(memory,building,0x7d0,layout,e)||!q::at(memory,building,0x170,out.root,e)||!out.root||
       !q::at(memory,building,0x100,out.transform,e)||!q::at(memory,building,0x7a8,out.state,e)||
       !q::at(memory,building,0xbd0,out.user_set,e)||out.user_set>1||
       !text(building+0x38,out.name,e)||!fluff(building,out.fluff_sha,e))
        return q::fail(e,e.empty()?"Duplicate live Building original class/full UID/scalars differ":e);
    if(art){out.form=Form::Art;
        if(!model(building,out.model,e)||!out.model)return q::fail(e,e.empty()?"Duplicate live Art Building original model absent":e);
        return true;
    }
    if(!text(building+0x768,out.type,e)||!text(building+0x788,out.variant,e)||!q::at(memory,building,0xbd8,bugs,e))return false;
    if(bugs&&!record(bugs,duplicate_building_values::OwnedKind::Bugs,out.bugs_sha,e))return false;
    if(layout){out.form=Form::Modular;
        if(!q::at(memory,building,0x7d8,plan,e)||!plan||!theme(building,out.theme,e)||
           !record(layout,duplicate_building_values::OwnedKind::Layout,out.layout_sha,e)||
           !record(plan,duplicate_building_values::OwnedKind::Plan,out.plan_sha,e))
            return q::fail(e,e.empty()?"Duplicate live Modular Building owned Layout/Plan/theme absent":e);
    }else{out.form=Form::Functional;
        if(!q::at(memory,building,0xa30,parent,e)||!q::at(memory,building,0xa38,out.dungeon,e))return false;
        if(parent&&(!q::at(memory,parent,0x18,out.parent_uid,e)||!out.parent_uid||
           !find(out.parent_uid,actual,e)||actual!=parent))return q::fail(e,e.empty()?"Duplicate live Building parent full UID differs":e);
    }
    return true;
}
// Read the complete original Make payload through its three concrete forms.
// Owned layouts/plans and saved Bug records use the existing original Record
// reader. Their addresses never become shared identities or wire values.
template<class Memory,class Text,class LocString,class Record,class Find>
bool inverse(Memory& memory,uintptr_t image,uintptr_t node,const Saved& expected,
             Text&& text,LocString&& loc_string,Record&& record,Find&& find,std::string& e){
    Saved value;uintptr_t ap=0,method=0,layout=0,plan=0,bugs=0,actual=0;
    if(!q::at(memory,node,0,ap,e))return false;
    size_t name=0,fluff=0,state=0,user_set=0,type=0,variant=0,root=0,transform=0;
    uintptr_t required_method=0;
    if(ap==image+0x1723270){value.form=Form::Art;required_method=0x548b00;
        name=0xc0;fluff=0xe0;state=0x118;user_set=0x11c;root=0x128;transform=0x130;
        if(!q::at(memory,node,0x120,value.model,e)||!value.model)return q::fail(e,e.empty()?"Duplicate Art inverse original model absent":e);
    }else if(ap==image+0x173c5b0){value.form=Form::Functional;required_method=0x548c70;
        name=0xc8;fluff=0xe8;state=0x120;user_set=0x124;type=0x128;variant=0x148;root=0x168;transform=0x170;
        if(!q::at(memory,node,0xc0,value.parent_uid,e)||!q::at(memory,node,0x1e0,value.dungeon,e)||!q::at(memory,node,0x1e8,bugs,e))return false;
    }else if(ap==image+0x1737870){value.form=Form::Modular;required_method=0x5491d0;
        name=0xc0;fluff=0xe0;state=0x118;user_set=0x11c;type=0x120;variant=0x140;root=0x198;transform=0x1a0;
        if(!text(node+0x160,value.theme,e)||!q::at(memory,node,0x180,layout,e)||!layout||
           !q::at(memory,node,0x188,plan,e)||!plan||!q::at(memory,node,0x190,bugs,e))
            return q::fail(e,e.empty()?"Duplicate Modular inverse owned layout/plan absent":e);
        if(!record(layout,false,value.layout_sha,e)||!record(plan,false,value.plan_sha,e))return false;
    }else return q::fail(e,"Duplicate Building inverse class is not an original Make form");
    if(!q::at(memory,ap,0x10,method,e)||method!=image+required_method||!q::at(memory,node,0xb8,value.uid,e)||!value.uid||
       !text(node+name,value.name,e)||!loc_string(node+fluff,value.fluff_sha,e)||!q::at(memory,node,state,value.state,e)||
       !q::at(memory,node,user_set,value.user_set,e)||value.user_set>1||!q::at(memory,node,root,value.root,e)||
       !q::at(memory,node,transform,value.transform,e))return q::fail(e,e.empty()?"Duplicate Building inverse native method/scalars invalid":e);
    if(type&&(!text(node+type,value.type,e)||!text(node+variant,value.variant,e)))return false;
    if(bugs&&!record(bugs,true,value.bugs_sha,e))return false;
    if(value!=expected)return q::fail(e,"Duplicate Building inverse full stored form/owner/identity/records differs");
    if(!find(value.uid,actual,e)||actual)return q::fail(e,e.empty()?"Duplicate Building inverse full UID is still alive":e);
    return true;
}
}
