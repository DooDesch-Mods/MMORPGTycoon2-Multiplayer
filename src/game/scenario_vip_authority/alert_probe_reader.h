#pragma once
#include "../placement_ui_probe/ui_probe.h"
#include "../npc_census.h"
#include <algorithm>
#include <array>
#include <set>

namespace mt2mp::game::scenario_vip_authority::alert_probe {
struct Array {
    uintptr_t data=0;int32_t count=0,capacity=0;std::vector<uintptr_t> items;
    bool operator==(const Array&)const=default;
};
struct Pane {
    uintptr_t object=0,ap=0,parent=0,window=0;Array children,panes;
    bool operator==(const Pane&)const=default;
};
struct Alert {
    uintptr_t object=0,selector=0,toon=0,scenario=0,subscriber=0;
    int32_t type=0,extra=0,subscriber_id=-1;std::string name;
    bool operator==(const Alert&)const=default;
};
struct Graph {
    bool present=false;std::string absent;
    uintptr_t hud=0,manager=0,window=0,window_ap=0,list=0,log=0;
    std::string window_name;std::vector<Pane> ancestry;
    Array selectors,panes,active,owned,scenarios;std::vector<Alert> alerts;
    bool operator==(const Graph&)const=default;
};
class Reader {
    placement_ui_probe::Memory& memory_;uintptr_t image_;uint32_t image_size_;
    unsigned reads_=0;
    bool fail(std::string& e,const std::string& cause){e=cause;return false;}
    bool bytes(uintptr_t p,void* out,size_t n,std::string& e){
        if(!p||n>UINTPTR_MAX-p)return fail(e,"VIP alert read extent invalid");
        if(++reads_>65536)return fail(e,"VIP alert read budget exceeded required<=65536");
        return memory_.copy(p,out,n,e);
    }
    template<class T>bool at(uintptr_t p,size_t offset,T& out,std::string& e){
        if(!p||p>UINTPTR_MAX-offset)return fail(e,"VIP alert field address overflow");
        return bytes(p+offset,&out,sizeof out,e);
    }
    bool type(uintptr_t p,uintptr_t rva,std::string& e){uintptr_t actual=0;
        return at(p,0,actual,e)&&(actual==image_+rva||fail(e,"VIP alert known class differs RVA="+std::to_string(rva)));
    }
    bool module_ap(uintptr_t p,uintptr_t& ap,std::string& e){
        if(!at(p,0,ap,e))return false;
        return (ap>=image_&&ap-image_<image_size_)||fail(e,"VIP alert observed pane AP outside pinned image");
    }
    bool text(uintptr_t p,size_t offset,std::string& out,std::string& e){
        uintptr_t data=0;uint64_t size=0;
        if(!at(p,offset,data,e)||!at(p,offset+8,size,e))return false;
        if(size>256||(!data&&size))return fail(e,"VIP alert copied string bound exceeded required<=256");
        out.resize(size_t(size));if(size&&!bytes(data,out.data(),size_t(size),e))return false;
        return (out.find('\0')==std::string::npos&&npc_census::utf8(out))||fail(e,"VIP alert copied string is not safe UTF-8");
    }
    bool array(uintptr_t owner,size_t data_offset,Array& out,std::string& e,int32_t limit=512){
        if(!at(owner,data_offset,out.data,e)||!at(owner,data_offset+8,out.count,e)||!at(owner,data_offset+12,out.capacity,e))return false;
        if(out.count<0||out.count>limit||out.capacity<out.count||out.capacity>limit||
           (out.count&&!out.data)||out.data>UINTPTR_MAX-size_t(out.count)*8)
            return fail(e,"VIP alert array bound differs required=0.."+std::to_string(limit)+" actual="+std::to_string(out.count)+" capacity="+std::to_string(out.capacity));
        std::set<uintptr_t> unique;
        for(int32_t i=0;i<out.count;++i){uintptr_t item=0;if(!at(out.data,size_t(i)*8,item,e))return false;
            if(!item||!unique.insert(item).second)return fail(e,"VIP alert array has null or duplicate owner");out.items.push_back(item);
        }
        return true;
    }
    static bool contains(const Array& a,uintptr_t p){return std::find(a.items.begin(),a.items.end(),p)!=a.items.end();}
    bool ancestry(Graph& out,std::string& e){
        uintptr_t child=out.list;std::set<uintptr_t> seen;
        for(unsigned depth=0;child!=out.window;++depth){
            if(!child||depth>=32||!seen.insert(child).second)return fail(e,"VIP alert ancestry null, cycle or depth bound");
            Pane pane;pane.object=child;
            if(!module_ap(child,pane.ap,e)||!at(child,0x278,pane.parent,e)||!at(child,0x280,pane.window,e))return false;
            if(pane.window!=out.window||!pane.parent)return fail(e,"VIP alert ancestry foreign Window or absent parent");
            if(!array(pane.parent,0x2d0,pane.children,e)||!array(pane.parent,0x300,pane.panes,e))return false;
            const auto matches=unsigned(contains(pane.children,child))+unsigned(contains(pane.panes,child));
            if(matches!=1)return fail(e,"VIP alert parent lacks unique reciprocal pane membership");
            out.ancestry.push_back(std::move(pane));child=out.ancestry.back().parent;
        }
        return true;
    }
    bool once(uintptr_t mode,uintptr_t state,Graph& out,std::string& e,bool arrivals){
        uintptr_t hud_singleton=0,manager_singleton=0;
        if(!at(mode,0x128,out.hud,e)||!at(image_,0x13df8b0,hud_singleton,e)||
           !at(mode,0x110,out.manager,e)||!at(image_,0x13df390,manager_singleton,e))return false;
        if(out.hud!=hud_singleton||out.manager!=manager_singleton)return fail(e,"VIP alert current Mode singleton owner differs");
        if(!out.hud||!out.manager){out.absent="VIP alert current HUD or AlertManager absent";return true;}
        if(!type(out.hud,0x1742750,e)||!type(out.manager,0x16fe5d0,e)||
           !at(out.hud,0x88,out.window,e)||!at(image_,0x13df270,out.list,e))return false;
        if(!out.window||!out.list){out.absent="VIP alert HUD Window or AlertList absent";return true;}
        // The loader may select a Window subclass. Record its actual AP, never guess it.
        if(!module_ap(out.window,out.window_ap,e)||!text(out.window,0x1b0,out.window_name,e)||
           !type(out.list,0x16dd150,e)||!ancestry(out,e)||!type(out.list+0x318,0x16e1ef0,e)||
           !array(out.list,0x320,out.selectors,e)||!array(out.list,0x300,out.panes,e)||
           !array(out.manager,0x68,out.active,e)||!array(out.manager,0x20,out.owned,e))return false;
        uintptr_t log_singleton=0;
        if(!at(state,0x1e8,out.log,e)||!at(image_,0x13df330,log_singleton,e))return false;
        if(out.log!=log_singleton||!type(out.log,0x16fd7b0,e))return fail(e,e.empty()?"VIP alert ScenarioLog owner differs":e);
        if(!array(out.log,0x20,out.scenarios,e,64))return false;
        for(auto selector:out.selectors.items){
            uintptr_t list=0,parent=0,window=0;Alert row;row.selector=selector;uint8_t shown=0;
            if(!type(selector,0x1704fa0,e)||!at(selector,0x318,list,e)||!at(selector,0x330,row.object,e)||
               !at(selector,0x278,parent,e)||!at(selector,0x280,window,e))return false;
            if(list!=out.list||parent!=out.list||window!=out.window||!contains(out.panes,selector))
                return fail(e,"VIP alert selector parent, Window or dual-array membership differs");
            uintptr_t reciprocal=0;
            if(!type(row.object,0x1749770,e)||!at(row.object,0x18,row.type,e)||
               !at(row.object,0x130,reciprocal,e)||!at(row.object,0x138,shown,e))return false;
            if(reciprocal!=selector||shown!=1||!contains(out.active,row.object))return fail(e,"VIP alert shown selector or manager active membership differs");
            if(row.type!=8||!arrivals)continue;
            if(!at(row.object,0xf0,row.extra,e)||row.extra!=0||contains(out.owned,row.object))
                return fail(e,e.empty()?"VIP arrival extra or manager ownership differs":e);
            if(!at(row.object,0x68,row.toon,e)||!type(row.toon,0x1744e90,e)||!text(row.object,0x88,row.name,e))return false;
            unsigned matches=0;
            for(auto instance:out.scenarios.items){uintptr_t ap=0,alert=0;
                if(!at(instance,0,ap,e))return false;if(ap!=image_+0x1730e90)continue;
                if(!at(instance,0xa0,alert,e))return false;if(alert!=row.object)continue;
                uint8_t arrived=0;uintptr_t toon=0,backlink=0;
                if(!at(instance,0x98,arrived,e)||!at(instance,0x68,row.subscriber,e)||
                   !type(row.subscriber,0x16e75f0,e)||!at(row.subscriber,0x20,row.subscriber_id,e)||
                   !at(row.subscriber,0x80,toon,e)||!at(row.toon,0x940,backlink,e))return false;
                if(arrived!=1||row.subscriber_id<0||toon!=row.toon||backlink!=row.subscriber)
                    return fail(e,"VIP arrival Scenario/Subscriber/Toon ownership differs");
                row.scenario=instance;++matches;
            }
            if(matches!=1)return fail(e,"VIP arrival lacks unique current ScenarioVip owner");
            out.alerts.push_back(std::move(row));
        }
        out.present=true;return true;
    }
public:
    Reader(placement_ui_probe::Memory& m,uintptr_t image,uint32_t image_size):memory_(m),image_(image),image_size_(image_size){}
    bool read_impl(uintptr_t mode,uintptr_t state,Graph& out,std::string& e,bool arrivals){
        out={};e.clear();reads_=0;if(!image_||image_size_<=0x1749770||image_>UINTPTR_MAX-image_size_)return fail(e,"VIP alert pinned image extent differs");
        Graph a,b;if(!once(mode,state,a,e,arrivals)||!once(mode,state,b,e,arrivals))return false;
        if(a!=b)return fail(e,"VIP alert owner graph changed during double read");
        out=std::move(b);e.clear();return true;
    }
    bool read(uintptr_t mode,uintptr_t state,Graph& out,std::string& e){return read_impl(mode,state,out,e,true);}
    // Client-owned alerts have no ScenarioVip allocation owner.
    bool read_ui(uintptr_t mode,uintptr_t state,Graph& out,std::string& e){return read_impl(mode,state,out,e,false);}
};
}
