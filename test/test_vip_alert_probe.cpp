#include "scenario_vip_authority/alert_probe_reader.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
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
int main(){try{
    Memory memory;memory.setup();probe::Reader reader(memory,image,0x2000000);probe::Graph graph;std::string error;
    need(reader.read(mode,fixture_state,graph,error),"complete actual reader");
    need(graph.present&&graph.window_ap==image+0x1750000&&graph.window_name=="alertlist_hud","actual Window subclass is observed, not guessed");
    need(graph.ancestry.size()==1&&graph.alerts.size()==1&&graph.alerts[0].subscriber_id==151&&graph.alerts[0].name=="Streamer Name","owner ancestry and copied identity");
    need(graph.selectors.count==1&&graph.panes.count==1&&graph.active.count==1&&graph.owned.count==0,"both selector arrays and manager ownership");
    const auto original=memory.bytes;need(reader.read(mode,fixture_state,graph,error)&&memory.bytes==original,"read-only fresh repeat leaves every byte unchanged");
    memory.put(subscriber,0x20,int32_t(152));need(reader.read(mode,fixture_state,graph,error)&&graph.alerts[0].subscriber_id==152,"next read observes fresh ID");
    for(unsigned fault=0;fault<20;++fault){memory.setup();
        switch(fault){
        case 0:memory.put(mode,0x128,uintptr_t(hud+8));break;
        case 1:memory.type(manager,1);break;
        case 2:memory.put(window,0,uintptr_t(9));break;
        case 3:memory.put(list,0x280,uintptr_t(window+8));break;
        case 4:memory.put(list,0x278,list);memory.array(list,0x2d0,0x180000,{list});break;
        case 5:memory.array(window,0x2d0,0,{});break;
        case 6:memory.put(list,0x328,int32_t(513));break;
        case 7:memory.array(list,0x320,0x120000,{selector,selector});break;
        case 8:memory.array(list,0x300,0,{});break;
        case 9:memory.put(selector,0x318,uintptr_t(list+8));break;
        case 10:memory.put(alert,0x130,uintptr_t(selector+8));break;
        case 11:memory.put(alert,0x138,uint8_t(0));break;
        case 12:memory.array(manager,0x68,0,{});break;
        case 13:memory.array(manager,0x20,0x170000,{alert});break;
        case 14:memory.put(scenario,0xa0,uintptr_t(0));break;
        case 15:memory.put(toon,0x940,uintptr_t(subscriber+8));break;
        case 16:memory.text(alert,0x88,0x160000,std::string("a\0b",3));break;
        case 17:memory.unreadable=alert+0x88;break;
        case 18:memory.drift=manager+0x74;break;
        case 19:memory.put(list,0x320,UINTPTR_MAX-uintptr_t(3));break;
        }
        need(!reader.read(mode,fixture_state,graph,error)&&!graph.present,"invalid owner graph rejects without observation");
        need(!error.empty(),"original rejection cause retained");
        if(fault==17)need(error=="original fixture unreadable","memory failure preserved");
        if(fault==18)need(error=="VIP alert owner graph changed during double read","header drift rejected");
    }
    memory.setup();constexpr uintptr_t pane=0xd0000;memory.type(pane,0x1750100);
    memory.put(list,0x278,pane);memory.put(pane,0x278,window);memory.put(pane,0x280,window);
    memory.array(pane,0x2d0,0x180000,{list});memory.array(pane,0x300,0,{});memory.array(window,0x2d0,0x110000,{pane});
    need(reader.read(mode,fixture_state,graph,error)&&graph.ancestry.size()==2,"actual intermediate pane ancestry");
    memory.put(pane,0x280,uintptr_t(window+8));need(!reader.read(mode,fixture_state,graph,error)&&!graph.present,"intermediate pane foreign Window rejected");
    memory.setup();memory.put(mode,0x128,uintptr_t(0));memory.put(image,0x13df8b0,uintptr_t(0));
    need(reader.read(mode,fixture_state,graph,error)&&!graph.present&&!graph.absent.empty(),"missing HUD is explicit absence");
    memory.setup();memory.put(hud,0x88,uintptr_t(0));need(reader.read(mode,fixture_state,graph,error)&&!graph.present&&!graph.absent.empty(),"missing Window is explicit absence");
    memory.setup();memory.put(image,0x13df270,uintptr_t(0));need(reader.read(mode,fixture_state,graph,error)&&!graph.present&&!graph.absent.empty(),"missing AlertList is explicit absence");
    memory.setup();memory.array(list,0x320,0,{});memory.array(list,0x300,0,{});memory.array(manager,0x68,0,{});
    need(reader.read(mode,fixture_state,graph,error)&&graph.present&&graph.alerts.empty(),"owner graph can be observed with no active arrival");
    std::cout<<checks<<" VIP alert owner reader checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<"failure after "<<checks<<": "<<error.what()<<"\n";return 1;}}
