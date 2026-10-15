#include "tutorial_network_tab_reader.h"
#include <map>
#include <iostream>
#include <stdexcept>
namespace t=mt2mp::game::tutorial_network_tab;
struct Memory{
 std::map<uintptr_t,unsigned char> data;uintptr_t allocation=0x900000;
 bool copy(uintptr_t p,void* out,size_t n,std::string& e){for(size_t i=0;i<n;++i){auto f=data.find(p+i);if(f==data.end()){e="missing fixture byte";return false;}static_cast<unsigned char*>(out)[i]=f->second;}return true;}
 template<class T>void put(uintptr_t p,T value){auto b=reinterpret_cast<unsigned char*>(&value);for(size_t i=0;i<sizeof(T);++i)data[p+i]=b[i];}
 void str(uintptr_t p,const std::string& s){auto q=allocation;allocation+=0x200;put(p,q);put(p+8,uint64_t(s.size()));for(size_t i=0;i<=s.size();++i)data[q+i]=static_cast<unsigned char>(s.c_str()[i]);}
 void array(uintptr_t h,std::vector<uintptr_t> a){auto q=allocation;allocation+=0x200;put(h+8,q);put(h+16,int32_t(a.size()));put(h+20,int32_t(a.size()));for(size_t i=0;i<a.size();++i)put(q+i*8,a[i]);}
};
constexpr uintptr_t image=0x10000000,world=0x100000,bar=0x200000,tab_address=0x210000,content_address=0x220000,cursor=0x230000,request=0x240000;
Memory baseline(){Memory m;m.put(image+0x13df650,world+0x180);for(int i=0;i<5;++i)m.str(image+0x1a36520+uintptr_t(i)*32,i==3?"Network":"Other"+std::to_string(i));m.put(world+0x1f8,int32_t(0));
 m.put(image+0x1a3aec8,bar);m.put(bar,image+0x16dcec0);m.array(image+0x1a3af10,{bar});m.put(bar+0x69,uint8_t(1));m.put(bar+0x45f,uint8_t(1));m.put(bar+0x377,uint8_t(0));m.put(bar+0x318,uintptr_t(0));m.str(bar+0x1b0,"Edit");m.array(bar+0x518,{tab_address});m.array(bar+0x2f8,{tab_address});m.array(bar+0x4e8,{content_address});
 m.put(tab_address,image+0x16fe380);m.str(tab_address+0x2c8,"Network");m.put(tab_address+0x278,bar);m.put(tab_address+0x280,bar);m.put(tab_address+0x69,uint8_t(1));m.put(tab_address+0x174,uint8_t(1));m.put(tab_address+0x360,uint8_t(0));m.str(tab_address+0x308,"ActionBar Edit SelectContent \"Network\"");
 m.put(content_address,image+0x171ccf0);m.str(content_address+0x1b0,"Network");m.put(content_address+0x348,int32_t(3));m.put(content_address+0x368,uintptr_t(0));m.put(image+0x13df9d0,cursor);m.put(cursor,image+0x1786dd0);return m;}
void choose(Memory& m){m.put(tab_address+0x360,uint8_t(1));m.put(world+0x1f8,int32_t(3));m.put(content_address+0x368,request);m.put(request,int32_t(3));m.put(content_address+0x278,bar);m.put(content_address+0x280,bar);m.put(content_address+0x69,uint8_t(1));m.put(bar+0x598,content_address);m.array(world+0x180+3*24,{request});}
int checks=0;void check(bool value,const char* name){++checks;if(!value)throw std::runtime_error(name);}
struct Calls {Memory m=baseline();bool reserved=false,press_ok=true,ledger_ok=true,throw_press=false;int presses=0,pins=0;bool changed=false;
 bool pin(t::Pin& p,std::string& e){++pins;if(changed&&pins==3)m.put(tab_address+0x278,uintptr_t(0));return t::inspect(m,image,world,p,e);}
 bool reserve(std::string& e){if(reserved){e="consumed";return false;}reserved=true;return true;}
 bool press(const t::Pin&,std::string& e){++presses;if(throw_press)throw std::runtime_error("native boundary");if(!press_ok){e="original SEH";return false;}choose(m);return true;}
 bool selected(const t::Pin& p,std::string& e){return t::selected(m,p,e);}
 bool unchanged(std::string& e){if(!ledger_ok)e="ledger cause";return ledger_ok;}
};
int main(){try{std::string e;t::Pin p;auto m=baseline();check(t::inspect(m,image,world,p,e)&&p.network==3,"read real enum membership");choose(m);check(t::inspect(m,image,world,p,e)&&t::selected(m,p,e),"owned selected request");
 auto reject=[&](auto mutate,const char* label){auto x=baseline();mutate(x);e.clear();check(!t::inspect(x,image,world,p,e)&&!e.empty(),label);};
 reject([](Memory& x){x.put(image+0x13df650,world+0x188);},"foreign manager");
 reject([](Memory& x){x.str(image+0x1a36520+32,"Network");},"duplicate enum");
 reject([](Memory& x){x.array(image+0x1a3af10,{bar,bar});},"duplicate current registry");
 reject([](Memory& x){x.put(bar+0x45f,uint8_t(0));},"hidden bar");
 reject([](Memory& x){x.put(bar+0x377,uint8_t(1));},"locked bar");
 reject([](Memory& x){x.put(bar+0x318,uintptr_t(123));},"custom logic");
 reject([](Memory& x){x.array(bar+0x518,{tab_address,tab_address});},"duplicate Network tab_address");
 reject([](Memory& x){x.array(bar+0x2f8,{});},"not native child");
 reject([](Memory& x){x.put(tab_address+0x280,bar+8);},"foreign tab_address owner");
 reject([](Memory& x){x.put(tab_address+0x174,uint8_t(0));},"not clickable");
 reject([](Memory& x){x.put(tab_address+0x69,uint8_t(0));},"not visible");
 reject([](Memory& x){x.str(tab_address+0x308,"ActionBar Edit SelectContent \"Grid\"");},"wrong command");
 reject([](Memory& x){x.put(content_address+0x348,int32_t(2));},"wrong view");
 reject([](Memory& x){x.array(bar+0x4e8,{content_address,content_address});},"duplicate content_address");
 reject([](Memory& x){x.put(bar+0x528,int32_t(65));},"bounded tabs");
 {auto x=baseline();choose(x);x.array(world+0x180+3*24,{});e.clear();check(t::inspect(x,image,world,p,e)&&!t::selected(x,p,e),"request owner absence");}
 {Calls c;t::Receipt r;e.clear();check(t::activate(c,r,e)&&r.attempted&&r.returned&&r.confirmed&&!r.unknown&&c.presses==1,"original once");check(!t::activate(c,r,e)&&!r.attempted&&c.presses==1,"selected no replay");}
 {Calls c;c.press_ok=false;t::Receipt r;e.clear();check(!t::activate(c,r,e)&&r.attempted&&!r.returned&&r.unknown&&e=="original SEH","partial original cause");}
 {Calls c;c.changed=true;t::Receipt r;e.clear();check(!t::activate(c,r,e)&&r.returned&&r.unknown&&!r.confirmed,"post owner fail");}
 {Calls c;c.ledger_ok=false;t::Receipt r;e.clear();check(!t::activate(c,r,e)&&r.unknown&&e=="ledger cause","ledger unchanged");}
 {Calls c;c.throw_press=true;t::Receipt r;try{t::activate(c,r,e);check(false,"expected throw");}catch(const std::runtime_error&){check(r.attempted&&r.unknown&&!r.confirmed,"exception receipt");}}
 std::cout<<"PASS "<<checks<<" actual reader/orchestration cases\n";return 0;}catch(const std::exception& x){std::cerr<<x.what()<<"\n";return 1;}}
