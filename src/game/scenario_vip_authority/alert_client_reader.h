#pragma once
#include "alert_lifecycle.h"
#include "alert_probe_reader.h"
#include "../toon_weapon_visual/authority.h"

namespace mt2mp::game::scenario_vip_authority::client_alert {
class Reader {
    placement_ui_probe::Memory& memory_;
    template<class T>bool at(uintptr_t p,size_t off,T& out,std::string& e){return toon_weapon_visual::visual_read(memory_,p,off,out,e);}
    bool text(uintptr_t p,size_t off,std::string& out,std::string& e){
        uintptr_t data=0;uint64_t size=0;
        if(!at(p,off,data,e)||!at(p,off+8,size,e))return false;
        if(size>256||(!data&&size))return vip::fail(e,"VIP selected name extent differs");
        out.resize(size_t(size));
        if(size&&!memory_.copy(data,out.data(),out.size(),e))return false;
        return (out.find('\0')==std::string::npos&&npc_census::utf8(out))||vip::fail(e,"VIP selected name UTF-8 differs");
    }
    bool subject_once(const Owner& o,int32_t id,Subject& out,std::string& e){
        std::vector<toon_weapon_visual::ToonOwner> rows;
        if(!toon_weapon_visual::toon_census(memory_,o.image,rows,e))return false;
        const auto it=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==id;});
        if(it==rows.end()||!it->toon)return vip::fail(e,"VIP active arrival requires a current live Toon");
        out.id=id;out.subscriber=it->subscriber;out.toon=it->toon;
        uintptr_t ap=0,name_method=0,target=0;int32_t refs=0;
        if(!at(out.toon,0,ap,e)||!at(ap,0xa8,name_method,e)||!at(out.toon,0x18,out.uid,e)||
           !at(out.toon,0x28,out.proxy,e)||!text(out.toon,0x38,out.name,e))return false;
        if(!out.uid||name_method!=o.image+0xe3f530)return vip::fail(e,"VIP live Toon UID/name accessor differs");
        if(out.proxy){
            if(!at(out.proxy,0,target,e)||!at(out.proxy,8,refs,e))return false;
            if(target!=out.toon+0x20||refs<0)return vip::fail(e,"VIP live Toon weak owner differs");
        }
        return true;
    }
public:
    explicit Reader(placement_ui_probe::Memory& m):memory_(m){}
    bool graph(const Owner& o,alert_probe::Graph& out,std::string& e){
        int32_t pe=0;uint32_t size=0;
        if(!at(o.image,0x3c,pe,e))return false;
        if(pe<0||pe>4096)return vip::fail(e,"VIP image header extent differs");
        if(!at(o.image,size_t(pe)+24+56,size,e))return false;
        alert_probe::Reader reader(memory_,o.image,size);
        if(!reader.read_ui(o.mode,o.state,out,e))return false;
        if(!out.present)return vip::fail(e,out.absent.c_str());
        return (out.window_ap==o.image+0x17883d0&&out.window_name=="alerts_hud")||vip::fail(e,"VIP actual alerts_hud Window class/name differs");
    }
    bool subject(const Owner& o,int32_t id,const std::string& name,Subject& out,std::string& e){
        Subject a,b;if(!subject_once(o,id,a,e)||!subject_once(o,id,b,e))return false;
        if(a!=b)return vip::fail(e,"VIP selected Toon owner changed during double read");
        if(b.name!=name)return vip::fail(e,"VIP copied Host name differs from current Toon name");
        out=std::move(b);e.clear();return true;
    }
    static bool contains(const alert_probe::Array& a,uintptr_t p){return std::find(a.items.begin(),a.items.end(),p)!=a.items.end();}
    bool inspect(const Owner& o,uintptr_t alert,const Subject& held,bool live,int expected_shown,uintptr_t& selector,std::string& e){
        alert_probe::Graph graph_now;if(!graph(o,graph_now,e))return false;
        if(graph_now.hud!=o.hud||graph_now.manager!=o.manager||graph_now.window!=o.window||graph_now.list!=o.list)
            return vip::fail(e,"VIP retained Alert owner graph differs");
        uintptr_t ap=0,d0=0,toon=0,proxy=0;int32_t type=0,extra=0;uint8_t shown=0;std::string name;
        if(!at(alert,0,ap,e)||!at(ap,8,d0,e)||!at(alert,0x18,type,e)||!at(alert,0xf0,extra,e)||
           !at(alert,0x130,selector,e)||!at(alert,0x138,shown,e)||!text(alert,0x88,name,e)||
           !at(alert,0x68,toon,e)||!at(alert,0x70,proxy,e))return false;
        if(ap!=o.image+0x1749770||d0!=o.image+0x234c70||type!=8||extra||name!=held.name||shown>1||
           (expected_shown>=0&&shown!=expected_shown)||contains(graph_now.owned,alert))
            return vip::fail(e,"VIP retained Alert type/name/lifetime/ownership differs");
        if(shown){
            if(!selector||!contains(graph_now.active,alert)||!contains(graph_now.selectors,selector)||!contains(graph_now.panes,selector))
                return vip::fail(e,"VIP shown Alert lacks reciprocal selector membership");
            uintptr_t selected=0;if(!at(selector,0x330,selected,e))return false;
            if(selected!=alert)return vip::fail(e,"VIP shown selector points to another Alert");
        }else if(selector||contains(graph_now.active,alert))return vip::fail(e,"VIP hidden Alert retains active UI membership");
        if(live){
            Subject current;if(!subject(o,held.id,held.name,current,e))return false;
            if(current.subscriber!=held.subscriber||current.toon!=held.toon||current.uid!=held.uid||toon!=current.toon||!proxy||proxy!=current.proxy)
                return vip::fail(e,"VIP Alert live weak Subject identity differs");
            uintptr_t target=0;int32_t refs=0;if(!at(proxy,0,target,e)||!at(proxy,8,refs,e))return false;
            if(target!=toon+0x20||refs<=0)return vip::fail(e,"VIP Alert live weak lease differs");
        }
        // Retiring alerts may already have been hidden and weak-cleared by
        // original NotifyToonDestroyed. Never dereference that retired Subject.
        e.clear();return true;
    }
    bool absent(const Owner& o,uintptr_t alert,uintptr_t selector,std::string& e){
        alert_probe::Graph g;if(!graph(o,g,e))return false;
        if(g.hud!=o.hud||g.manager!=o.manager||g.window!=o.window||g.list!=o.list)
            return vip::fail(e,"VIP removal owner graph differs");
        return (!contains(g.active,alert)&&!contains(g.owned,alert)&&(!selector||
            (!contains(g.selectors,selector)&&!contains(g.panes,selector))))||vip::fail(e,"VIP removed Alert/selector remains in owner arrays");
    }
};
}
