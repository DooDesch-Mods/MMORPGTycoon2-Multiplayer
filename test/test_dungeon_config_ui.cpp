#include "dungeon_config_ui.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>
namespace ui=mt2mp::game::dungeon_config_ui;
constexpr uintptr_t image=0x10000000,window=0x1000,dungeon=0x2000,spinner=0x3000,accept=0x4000,offer_window=0x5000,page=0x6000,button=0x7000,view=0x8000,exterior=0x9000,tabs=0xa000;
std::map<std::pair<uintptr_t,size_t>,std::vector<unsigned char>> fields;
std::map<uintptr_t,std::string> strings;
template<class T>void put(uintptr_t base,size_t offset,T value){auto& bytes=fields[{base,offset}];bytes.resize(sizeof value);std::memcpy(bytes.data(),&value,sizeof value);}
template<class T>bool read(uintptr_t base,size_t offset,T& value,std::string& error){auto found=fields.find({base,offset});if(found==fields.end()||found->second.size()!=sizeof value){error="Exact field read failed";return false;}std::memcpy(&value,found->second.data(),sizeof value);return true;}
bool text(uintptr_t address,std::string& value,std::string& error){auto found=strings.find(address);if(found==strings.end()){error="Exact command read failed";return false;}value=found->second;return true;}
void ancestry(uintptr_t pane,uintptr_t parent,uintptr_t owner){put(pane,0x69,uint8_t(1));put(pane,0x278,parent);put(pane,0x280,owner);}
void setup(){fields.clear();strings.clear();
    put(window,0,image+0x173bee0);put(window,0x510,dungeon);put(window,0x45f,uint8_t(1));
    put(image+0x173bee0,0x168,image+0x6aea60);put(image+0x173bee0,0x1d0,image+0x81ec20);
    put(spinner,0,image+0x16c1720);put(spinner,0x428,uintptr_t(0));put(spinner,0x430,uintptr_t(0));ancestry(spinner,window,window);
    put(spinner,0x338,uint8_t(1));put(spinner,0x438,1.0f);put(spinner,0x43c,int32_t(1));put(spinner,0x440,int32_t(10));put(spinner,0x444,int32_t(1));put(spinner,0x44c,int32_t(4));
    strings[spinner+0x378]="MessageTo Window SetInstanceCount";
    put(accept,0,image+0x16e3ec0);ancestry(accept,window,window);put(accept,0x46d,uint8_t(0));put(accept,0x174,uint8_t(1));strings[accept+0x388]="MessageTo Window Accept";
    put(offer_window,0,image+0x172af10);put(image+0x172af10,0x168,image+0x66d990);put(offer_window,0x45f,uint8_t(1));put(offer_window,0x4f8,exterior);
    put(exterior,0xa38,dungeon);put(dungeon,0x6f8,exterior);ancestry(page,view,offer_window);strings[page+0x1b0]="{dungeoninfo_tab_instances}";
    put(tabs,0,image+0x17015b0);put(image+0x17015b0,0xf0,image+0x752d70);put(tabs,0x3b8,view);ancestry(tabs,offer_window,offer_window);
    put(view,0,image+0x16dfe90);put(image+0x16dfe90,0x168,image+0x7568e0);ancestry(view,tabs,offer_window);
    put(view,0x410,page);strings[view+0x3e8]="{dungeoninfo_tab_instances}";
    put(button,0,image+0x16e3ec0);ancestry(button,page,offer_window);put(button,0x46d,uint8_t(0));put(button,0x174,uint8_t(1));strings[button+0x388]="MessageTo Window OpenInstancesConfig";
}
unsigned checks=0;
void need(bool okay,const std::string& error){++checks;if(!okay)throw std::runtime_error(error);}
int main()try{std::string error;ui::Controls result;
    const auto native_read=[](uintptr_t base,size_t offset,auto& value,std::string& cause){return read(base,offset,value,cause);};
    for(int count=1;count<=10;++count){setup();error.clear();put(spinner,0x44c,int32_t(count));need(ui::validate(native_read,text,image,window,dungeon,spinner,accept,result,error)&&result.value==count,error);}
    for(int mode=0;mode<20;++mode){setup();error.clear();
        switch(mode){
        case 0:put(window,0x510,uintptr_t(1));break;case 1:put(window,0x45f,uint8_t(0));break;
        case 2:put(image+0x173bee0,0x168,image+1);break;case 3:put(image+0x173bee0,0x1d0,image+1);break;
        case 4:put(spinner,0,uintptr_t(1));break;case 5:put(spinner,0x428,uintptr_t(1));break;
        case 6:put(spinner,0x430,uintptr_t(1));break;case 7:put(spinner,0x69,uint8_t(0));break;
        case 8:put(spinner,0x280,uintptr_t(1));break;case 9:put(spinner,0x278,spinner);break;
        case 10:put(spinner,0x338,uint8_t(0));break;case 11:put(spinner,0x438,0.0f);break;
        case 12:put(spinner,0x43c,int32_t(0));break;case 13:put(spinner,0x440,int32_t(11));break;
        case 14:put(spinner,0x444,int32_t(2));break;case 15:put(spinner,0x44c,int32_t(11));break;
        case 16:strings[spinner+0x378]="foreign";break;case 17:put(accept,0x46d,uint8_t(1));break;
        case 18:put(accept,0x174,uint8_t(0));break;case 19:strings[accept+0x388]="MessageTo Window Cancel";break;}
        need(!ui::validate(native_read,text,image,window,dungeon,spinner,accept,result,error)&&!error.empty(),"Foreign/hidden/bound Config controls accepted");
    }
    ui::Tab tab;setup();error.clear();need(ui::offer(native_read,text,image,offer_window,exterior,button,dungeon,tab,error)&&tab.page==page&&tab.view==view,error);
    need(ui::prepare(native_read,text,image,offer_window,exterior,tabs,page,dungeon,tab,error),error);
    put(page,0x278,uintptr_t(0));put(page,0x280,uintptr_t(0));put(button,0x278,uintptr_t(0));
    need(ui::prepare(native_read,text,image,offer_window,exterior,tabs,page,dungeon,tab,error),"Original unlaid-out Instances page could not be selected");
    setup();
    put(page,0x69,uint8_t(0));need(!ui::offer(native_read,text,image,offer_window,exterior,button,dungeon,tab,error),"Hidden Instances page admitted as visible Configure");
    for(int mode=0;mode<18;++mode){setup();error.clear();
        switch(mode){case 0:put(offer_window,0x45f,uint8_t(0));break;case 1:put(offer_window,0,uintptr_t(1));break;
        case 2:put(exterior,0xa38,uintptr_t(0));break;case 3:put(image+0x172af10,0x168,image+1);break;
        case 4:put(button,0x278,offer_window);break;case 5:put(button,0x280,window);break;
        case 6:put(button,0x46d,uint8_t(1));break;case 7:put(page,0x69,uint8_t(0));break;
        case 8:put(view,0x278,view);break;case 9:strings[button+0x388]="MessageTo Parent OpenInstancesConfig";break;
        case 10:put(dungeon,0x6f8,uintptr_t(0));break;case 11:put(offer_window,0x4f8,uintptr_t(1));break;
        case 12:put(view,0,uintptr_t(1));break;case 13:put(image+0x16dfe90,0x168,image+1);break;
        case 14:strings[page+0x1b0]="quoted\"page";break;case 15:strings[page+0x1b0]="";break;
        case 16:put(view,0x410,uintptr_t(0));break;case 17:strings[view+0x3e8]="other";break;}
        need(!ui::offer(native_read,text,image,offer_window,exterior,button,dungeon,tab,error)&&!error.empty(),"Unavailable or foreign original Config offer accepted");
    }
    for(int mode=0;mode<5;++mode){setup();error.clear();
        switch(mode){case 0:put(tabs,0,uintptr_t(0));break;case 1:put(tabs,0x3b8,uintptr_t(0));break;
        case 2:put(image+0x17015b0,0xf0,image+1);break;case 3:strings[page+0x1b0]="other";break;case 4:put(view,0x278,window);break;}
        need(!ui::prepare(native_read,text,image,offer_window,exterior,tabs,page,dungeon,tab,error)&&!error.empty(),"Foreign original Instances tab preparation accepted");}
    std::cout<<checks<<" actual offered Config/Spinner/Accept ownership and visibility checks passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
