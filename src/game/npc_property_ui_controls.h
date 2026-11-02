#pragma once
#include "typed_action/npc_property_action.h"
#include <cstdint>
#include <set>
#include <string>
#include <vector>
namespace mt2mp::game::npc_property_ui_controls {
struct Offered {uintptr_t receiver=0,text_edit=0,control=0;std::string command;};
inline bool routed(const std::string& actual,const std::string& command){
    return actual==command||actual=="MessageTo Parent "+command||actual=="MessageTo Window "+command;
}
template<class Read,class Text>bool find(Read&& read,Text&& text,uintptr_t image,uintptr_t window,
    uintptr_t target,const action_session::NpcPropertyIntent& intent,Offered& out,std::string& error){
    out={};error.clear();
    const auto fail=[&](const char* cause){if(error.empty())error=cause;return false;};
    const auto visible=[&](uintptr_t pane){
        std::set<uintptr_t> seen;
        while(pane!=window){uintptr_t owner=0,parent=0;uint8_t shown=0;
            if(!pane||!seen.insert(pane).second||seen.size()>64)return fail("NPC property control ancestry invalid");
            if(!read(pane,0x69,shown,error)||shown!=1||!read(pane,0x280,owner,error)||owner!=window||
               !read(pane,0x278,parent,error))return fail("NPC property control is hidden or has a different owner");
            pane=parent;
        }
        return true;
    };
    if(intent.kind==action_session::NpcPropertyKind::Name){uintptr_t header=0,klass=0,bound=0,edit=0,owner=0;
        if(!read(window,0x4e8,header,error)||!header||!read(header,0,klass,error)||klass!=image+0x16e58d0||
           !read(header,0x340,bound,error)||bound!=target||!read(header,0x320,edit,error)||!edit||
           !read(edit,0,klass,error)||klass!=image+0x1703290||
           !read(edit,0x280,owner,error)||owner!=window||!visible(header))return fail("NPC original Header/rename control binding differs");
        out={header,edit,header,"Rename"};return true;
    }
    if(intent.kind==action_session::NpcPropertyKind::OnlineState){
        uintptr_t status=0,klass=0,data=0;int32_t count=0,capacity=0;std::string route;
        const std::string command="SetState "+std::to_string(intent.online+1);
        if(!read(window,0x520,status,error)||!status||!read(status,0,klass,error)||klass!=image+0x16e7390||
           !text(status+0x318,route,error)||!routed(route,"SetState")||!visible(status)||
           !read(status,0x348,data,error)||!read(status,0x350,count,error)||!read(status,0x354,capacity,error)||
           count<1||count>128||capacity<count||capacity>100000||!data)return fail("NPC property original status control binding/options invalid");
        for(int32_t i=0;i<count;++i){uintptr_t button=0;std::string offered;
            if(!read(data,size_t(i)*8,button,error))return false;
            if(!button)continue;
            if(!read(button,0,klass,error)||klass!=image+0x16e3ec0||!text(button+0x388,offered,error))return fail("NPC property original state button invalid");
            if(!routed(offered,command))continue;
            if(out.control)return fail("NPC property repeated state control");
            uint8_t disabled=1,clickable=0;
            if(!visible(button)||!read(button,0x46d,disabled,error)||disabled||
               !read(button,0x174,clickable,error)||clickable!=1)return fail("NPC property requested state control is hidden or disabled");
            out={status,0,button,command};
        }
        return out.control!=0||fail("NPC property requested state is not offered");
    }
    const std::string wanted=intent.kind==action_session::NpcPropertyKind::RaiseLevel?"LevelUp":"LevelDown";
    std::vector<uintptr_t> pending{window};std::set<uintptr_t> seen;
    while(!pending.empty()){
        const auto pane=pending.back();pending.pop_back();uintptr_t klass=0,gather=0;
        if(!pane||!seen.insert(pane).second||seen.size()>2048)return fail("NPC property control tree cycle or limit");
        if(!read(pane,0,klass,error))return false;
        if(klass<image||klass>image+0x2000000)return fail("NPC property pane class outside pinned image");
        if(!read(klass,0x178,gather,error))return false;
        if(klass==image+0x16e3ec0){
            std::string command;if(!text(pane+0x388,command,error))return false;
            if(routed(command,wanted)){
                if(out.control)return fail("NPC property has multiple matching original controls");
                if(!visible(pane))return false;
                uint8_t disabled=1,clickable=0;
                if(!read(pane,0x46d,disabled,error)||disabled||!read(pane,0x174,clickable,error)||clickable!=1)
                    return fail("NPC property original level button is disabled");
                out={window,0,pane,wanted};
            }
        }
        if(gather==image+0xc35730)continue;
        if(gather!=image+0x8167c0)return fail("NPC property unsupported original pane traversal");
        for(size_t offset:{size_t(0x2c8),size_t(0x2f8)}){
            uintptr_t data=0;int32_t count=0,capacity=0;
            if(!read(pane,offset+8,data,error)||!read(pane,offset+16,count,error)||!read(pane,offset+20,capacity,error))return false;
            if(count<0||count>2048||capacity<count||capacity>100000||(count&&!data))return fail("NPC property original child extent invalid");
            for(int32_t i=0;i<count;++i){uintptr_t child=0,parent=0,owner=0;
                if(!read(data,size_t(i)*8,child,error)||!child||!read(child,0x278,parent,error)||parent!=pane||
                   !read(child,0x280,owner,error)||owner!=window)return fail("NPC property original child owner differs");
                pending.push_back(child);
            }
        }
    }
    return out.control!=0||fail("NPC property original offered control absent");
}
}
