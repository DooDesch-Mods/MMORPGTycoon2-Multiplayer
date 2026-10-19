#pragma once
#include "../../src/game/hud_time_reader.h"
#include <cstring>
#include <functional>
#include <map>
#include <stdexcept>
#include <vector>
namespace time_mouse_tests {
namespace hud=mt2mp::game::hud_time;
inline constexpr uintptr_t image=0x140000000,game=0x300000000,mode=game+0x1000,world=game+0x2000,
    hud_object=game+0x3000,panel=game+0x4000,clock=game+0x5000,options=game+0x6000,pause_window=game+0x7000,children=game+0x8000,
    player=game+0xb000,inventory=game+0xc000,inventory_data=game+0xd000;
inline constexpr const char* exe_hash="bbab2dc68babcc644d0a1679548bf1e334b6c52aea669c3f61f6cb9d45c1aaf6";
inline constexpr std::array<float,4> scales{0,1,3,9};
inline constexpr std::array<const char*,4> names{"@Time Pause","@Time Normal","@Time Fast","@Time Fastest"};
struct NativeReadFixture {
    std::map<uintptr_t,std::vector<unsigned char>> blocks;
    uint32_t thread=17;bool throwing=false;int reads=0;std::function<void(uintptr_t)> callback;
    static uintptr_t slot(size_t i){return game+0x10000+i*0x10000;}
    static uintptr_t button(size_t i){return slot(i)+0x1000;}
    static uintptr_t item(size_t i){return slot(i)+0x2000;}
    static uintptr_t type(size_t i){return slot(i)+0x3000;}
    static uintptr_t item_proxy(size_t i){return slot(i)+0x4000;}
    static uintptr_t type_proxy(size_t i){return slot(i)+0x4100;}
    static uintptr_t inventory_item(size_t i){return slot(i)+0x7000;}
    void alloc(uintptr_t p,size_t n){blocks[p]=std::vector<unsigned char>(n);}
    void raw(uintptr_t p,const void* v,size_t n){auto it=blocks.upper_bound(p);if(it==blocks.begin())throw std::runtime_error("fixture write absent");--it;
        if(p-it->first+n>it->second.size())throw std::runtime_error("fixture write overflow");std::memcpy(it->second.data()+p-it->first,v,n);}
    template<class T>void put(uintptr_t p,T v){raw(p,&v,sizeof v);}
    template<class T>T get(uintptr_t p){T out{};std::string cause;if(!read(this,p,&out,sizeof out,cause))throw std::runtime_error(cause);return out;}
    void vt(uintptr_t p,uintptr_t rva){put(p,image+rva);}
    void name(uintptr_t field,uintptr_t text,const char* value){alloc(text,32);raw(text,value,std::strlen(value));put(field,text);put(field+8,uint64_t(std::strlen(value)));}
    static uint32_t tid(void* user){return static_cast<NativeReadFixture*>(user)->thread;}
    static bool read(void* user,uintptr_t at,void* out,size_t n,std::string& cause){auto& m=*static_cast<NativeReadFixture*>(user);++m.reads;
        if(m.throwing)throw std::runtime_error("fixture original read failure");if(m.callback)m.callback(at);
        auto it=m.blocks.upper_bound(at);if(it==m.blocks.begin()){cause="fixture address missing";return false;}--it;
        if(at-it->first+n>it->second.size()){cause="fixture range missing";return false;}
        std::memcpy(out,it->second.data()+at-it->first,n);return true;}
    hud::Services services(){return {this,tid,read};}
    hud::Config config(){return {image,17,exe_hash};}
    NativeReadFixture(){
        for(auto p:{game,mode,world,hud_object,panel,clock,options,pause_window,children,player,inventory,inventory_data})alloc(p,0x1000);
        for(auto pair:{std::pair{uintptr_t(0x13df8f0),game},{0x13df320,mode},{0x13df8b0,hud_object},{0x13df9a0,clock},{0x13df1e0,options},{0x13df9f0,player}}){alloc(image+pair.first,8);put(image+pair.first,pair.second);}
        struct Prologue{uintptr_t rva;const char* bytes;size_t n;};
        for(auto g:{Prologue{0x62c400,"\x41\x57\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53\x48\x83\xec\x78",16},
          {0x583b20,"\x57\x56\x53\x48\x83\xec\x20\x48\x8b\x05\xd2\x74\x0c\x01",14},
          {0x5816c0,"\x41\x57\x41\x56\x41\x55\x41\x54\x55\x57\x56\x53\x48\x81\xec\xc8",16},
          {0x20e8d0,"\x48\x8b\x05\xc9\x10\x1d\x01\x48\x85\xc0\x74\x25\xf3\x0f\x10\x40",16}}){alloc(image+g.rva,g.n);raw(image+g.rva,g.bytes,g.n);}
        vt(game,0x1743740);vt(mode,0x16e6730);vt(hud_object,0x1742750);vt(panel,0x16e4140);vt(clock,0x17499d0);vt(options,0x16c0db0);vt(pause_window,0x17883d0);vt(player,0x17873b0);vt(inventory,0x16df0f0);
        put(game+0x28,mode);put(mode+0x2c4,int32_t(4));put(mode+0x58,world);put(mode+0x128,hud_object);put(hud_object+0x60,panel);
        put(panel+0x45f,uint8_t(1));name(panel+0x1b0,game+0x9000,"clockpanel");
        put(clock+0x58,0.f);put(clock+0x5c,1.f);put(clock+0x44,int32_t(400));put(clock+0x48,int32_t(28));put(clock+0x50,uint8_t(1));put(clock+0x68,pause_window);
        name(pause_window+0x1b0,game+0xa000,"pause_hud");put(pause_window+0x45f,uint8_t(1));put(options+0x191,uint8_t(1));
        vt(panel+0x2c8,0x16ef880);put(panel+0x2d0,children);put(panel+0x2d8,int32_t(4));put(panel+0x2dc,int32_t(4));vt(panel+0x2f8,0x1748bf0);
        put(player+0x1d0,inventory);put(inventory+8,inventory_data);put(inventory+0x10,int32_t(4));
        for(size_t i=0;i<4;++i){auto s=slot(i),b=button(i),it=item(i),t=type(i),proxy=item_proxy(i),inv=inventory_item(i);
            for(auto p:{s,b,it,t,proxy,inv})alloc(p,0x1000);put(children+i*8,s);vt(s,0x16db1c0);vt(b,0x16e5b90);vt(it,0x17437f0);vt(t,0x1723f50);vt(inv,0x17437f0);
            put(s+0x278,panel);put(s+0x280,panel);put(s+0x368,b);put(s+0x360,it);name(s+0x318,s+0x5000,names[i]);
            put(b+0x350,s);put(b+0x278,s);put(b+0x280,panel);put(b+0x2d0,it);put(b+0x2d8,proxy);put(it+0x28,proxy);put(proxy,it+0x20);
            put(it+0x68,t);put(it+0x70,type_proxy(i));put(type_proxy(i),t+0x38);put(t+0x40,type_proxy(i));put(t+0x15c,scales[i]);put(it+0x38,int32_t(i?1:0));name(t+0x48,s+0x6000,names[i]);
            put(b+0x380,uint8_t(i==0));put(b+0x381,uint8_t(1));put(b+0x45f,uint8_t(1));put(s+0x45f,uint8_t(1));
            put(inventory_data+i*8,inv);put(inv+0x68,t);put(inv+0x70,type_proxy(i));put(inv+0x38,int32_t(i?1:0));
        }
    }
};
}
