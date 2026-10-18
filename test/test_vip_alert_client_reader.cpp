#include "scenario_vip_authority/alert_client_reader.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace c=mt2mp::game::scenario_vip_authority::client_alert;
namespace probe=mt2mp::game::scenario_vip_authority::alert_probe;
constexpr uintptr_t image=0x140000000,mode=0x10000,fixture_state=0x20000,hud=0x30000,manager=0x40000,
    window=0x50000,list=0x60000,selector=0x70000,alert=0x80000,log_owner=0x90000,
    scenario=0xa0000,subscriber=0xb0000,toon=0xc0000;
struct Memory:placement_ui_probe::Memory {
    std::map<uintptr_t,unsigned char> bytes;uintptr_t unreadable=0,drift=0;unsigned drift_reads=0;
    template<class T>void put(uintptr_t p,size_t offset,T value){const auto* data=reinterpret_cast<const unsigned char*>(&value);
        for(size_t i=0;i<sizeof value;++i)bytes[p+offset+i]=data[i];}
    bool copy(uintptr_t p,void* out,size_t n,std::string& error)override{
        if(p==unreadable){error="original fixture unreadable";return false;}
        if(p==drift&&++drift_reads==2)put(p,0,int32_t(2));
        for(size_t i=0;i<n;++i){const auto found=bytes.find(p+i);if(found==bytes.end()){error="original fixture missing field";return false;}
            static_cast<unsigned char*>(out)[i]=found->second;}
        return true;
    }
    void type(uintptr_t p,uintptr_t rva){put(p,0,image+rva);}
    void array(uintptr_t p,size_t offset,uintptr_t data,std::initializer_list<uintptr_t> items){
        put(p,offset,data);put(p,offset+8,int32_t(items.size()));put(p,offset+12,int32_t(items.size()));
        size_t i=0;for(auto item:items)put(data,8*i++,item);
    }
    void text(uintptr_t p,size_t offset,uintptr_t data,const std::string& text){
        put(p,offset,data);put(p,offset+8,uint64_t(text.size()));for(size_t i=0;i<text.size();++i)put(data,i,text[i]);
    }
    void setup(){
        bytes.clear();unreadable=drift=0;drift_reads=0;
        put(mode,0x128,hud);put(image,0x13df8b0,hud);put(mode,0x110,manager);put(image,0x13df390,manager);
        type(hud,0x1742750);type(manager,0x16fe5d0);put(hud,0x88,window);put(image,0x13df270,list);
        type(window,0x1750000);text(window,0x1b0,0x100000,"alertlist_hud");type(list,0x16dd150);
        put(list,0x278,window);put(list,0x280,window);
        array(window,0x2d0,0x110000,{list});array(window,0x300,0,{});
        type(list+0x318,0x16e1ef0);array(list,0x320,0x120000,{selector});array(list,0x300,0x130000,{selector});
        array(manager,0x68,0x140000,{alert});array(manager,0x20,0,{});
        put(fixture_state,0x1e8,log_owner);put(image,0x13df330,log_owner);type(log_owner,0x16fd7b0);array(log_owner,0x20,0x150000,{scenario});
        type(selector,0x1704fa0);put(selector,0x318,list);put(selector,0x330,alert);put(selector,0x278,list);put(selector,0x280,window);
        type(alert,0x1749770);put(alert,0x18,int32_t(8));put(alert,0x130,selector);put(alert,0x138,uint8_t(1));
        put(alert,0xf0,int32_t(0));put(alert,0x68,toon);text(alert,0x88,0x160000,"Streamer Name");
        type(toon,0x1744e90);put(toon,0x940,subscriber);type(subscriber,0x16e75f0);put(subscriber,0x20,int32_t(151));put(subscriber,0x80,toon);
        type(scenario,0x1730e90);put(scenario,0xa0,alert);put(scenario,0x98,uint8_t(1));put(scenario,0x68,subscriber);
    }
};

unsigned checks=0;
void need(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
constexpr uintptr_t sub_manager=0x190000,sub_data=0x1a0000,proxy=0x1b0000;
void setup(Memory& m){
    m.setup();m.type(window,0x17883d0);m.text(window,0x1b0,0x100000,"alerts_hud");
    m.put(image,0x3c,int32_t(0x80));m.put(image,0x80+24+56,uint32_t(0x2000000));
    m.put(image,0x13df690,sub_manager);m.type(sub_manager,0x1725f40);m.array(sub_manager,0x40,sub_data,{subscriber});
    m.put(image+0x1744e90,0xa8,image+0xe3f530);m.put(toon,0x18,uint64_t(45));m.put(toon,0x28,proxy);
    m.text(toon,0x38,0x1c0000,"Streamer Name");m.put(proxy,0,toon+0x20);m.put(proxy,8,int32_t(1));
    m.put(alert,0x70,proxy);m.put(image+0x1749770,8,image+0x234c70);
    // A Client-created alert has no ScenarioVip allocation owner.
    m.put(scenario,0xa0,uintptr_t(0));
}
c::Owner owner(){c::Owner o;o.image=image;o.mode=mode;o.state=fixture_state;o.hud=hud;o.manager=manager;o.window=window;o.window_ap=image+0x17883d0;o.list=list;return o;}
int main(){try{
    Memory m;setup(m);c::Reader reader(m);std::string e;c::Subject s;uintptr_t selected=0;
    need(reader.subject(owner(),151,"Streamer Name",s,e),"exact live Subject");
    need(s.toon==toon&&s.subscriber==subscriber&&s.uid==45&&s.proxy==proxy,"actual identity fields");
    const auto bytes=m.bytes;
    need(reader.inspect(owner(),alert,s,true,1,selected,e)&&selected==selector,"Client-owned shown alert without Scenario allocation");
    need(m.bytes==bytes,"native reader performs no writes");
    probe::Graph graph;probe::Reader old(m,image,0x2000000);
    need(!old.read(mode,fixture_state,graph,e),"default Host probe retains Scenario owner guard");
    for(unsigned fault=0;fault<15;++fault){setup(m);
        switch(fault){
        case 0:m.put(toon,0x940,subscriber+8);break;
        case 1:m.put(toon,0x18,uint64_t(0));break;
        case 2:m.put(image+0x1744e90,0xa8,image+1);break;
        case 3:m.text(toon,0x38,0x1c0000,"Other name");break;
        case 4:m.put(proxy,0,toon+0x28);break;
        case 5:m.put(proxy,8,int32_t(0));break;
        case 6:m.put(alert,0x70,uintptr_t(0));break;
        case 7:m.put(alert,0xf0,int32_t(1));break;
        case 8:m.put(alert,0x18,int32_t(7));break;
        case 9:m.put(image+0x1749770,8,image+0x234b30);break;
        case 10:m.array(manager,0x20,0x170000,{alert});break;
        case 11:m.array(list,0x300,0,{});break;
        case 12:m.put(window,0,image+0x1750000);break;
        case 13:m.unreadable=alert+0x88;break;
        case 14:m.drift=manager+0x74;break;
        }
        need(!reader.inspect(owner(),alert,s,true,1,selected,e)&&!e.empty(),"selected native owner/lifetime failure rejects");
        if(fault==13)need(e=="original fixture unreadable","original read failure preserved");
    }
    setup(m);m.put(toon,0x28,uintptr_t(0));
    need(reader.subject(owner(),151,"Streamer Name",s,e)&&!s.proxy,"factory may allocate first Weak proxy");
    m.put(toon,0x28,proxy);need(reader.inspect(owner(),alert,s,true,1,selected,e),"returned factory proxy accepted with exact current ownership");
    setup(m);m.array(list,0x320,0,{});m.array(list,0x300,0,{});m.array(manager,0x68,0,{});
    m.put(alert,0x130,uintptr_t(0));m.put(alert,0x138,uint8_t(0));m.put(alert,0x68,uintptr_t(0));m.put(alert,0x70,uintptr_t(0));
    m.unreadable=toon;
    need(reader.inspect(owner(),alert,s,false,0,selected,e)&&!selected,"original hidden/weak-cleared retirement needs no expired Toon read");
    need(reader.absent(owner(),alert,selector,e),"former selector absent before and after D0");
    m.array(manager,0x20,0x170000,{alert});need(!reader.absent(owner(),alert,selector,e),"manager owned allocation rejects deletion confirmation");
    setup(m);m.array(sub_manager,0x40,sub_data,{subscriber,subscriber});
    need(!reader.subject(owner(),151,"Streamer Name",s,e),"ambiguous serialized ID rejects");
    std::cout<<checks<<" VIP Client reader checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<"failure after "<<checks<<": "<<x.what()<<'\n';return 1;}}
