#include "tutorial_network_tab_reader.h"
#include "native_tool_prepare/network_tab.h"
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

namespace network_adapter_fixture {
namespace n=mt2mp::game::network_tab;
struct Calls {
    Memory m=baseline();int presses=0,pins=0;bool reserved=false,pending=false,press_ok=true,ledger_ok=true;
    bool mutate_owner=false,mutate_final=false,throw_press=false;
    bool pin(n::Pin& p,std::string& e){++pins;if(mutate_owner&&pins==3)m.put(tab_address+0x280,bar+8);return n::inspect(m,image,world,p,e);}
    bool reserve(const n::Pin&,std::string& e){if(reserved){e="consumed token";return false;}reserved=true;return true;}
    bool press(const n::Pin&,std::string& e){++presses;if(throw_press)throw std::runtime_error("native boundary");if(!press_ok){e="original SEH";return false;}choose(m);if(pending)m.put(world+0x1f8,int32_t(0));return true;}
    bool status(const n::Pin& p,n::Status& out,std::string& e){return n::status(m,p,out,e);}
    bool unchanged(std::string& e){if(mutate_final)m.put(image+0x13df650,world+0x188);if(!ledger_ok)e="full20 ledger changed";return ledger_ok;}
};
void run(){
    std::string e;n::Pin p;n::Status s;
    {Calls c;n::Receipt r;check(n::prepare(c,r,e)&&r.attempted&&r.returned&&r.confirmed&&!r.unknown&&r.state.ready&&c.presses==1,"Network sibling original once");check(!n::prepare(c,r,e)&&!r.attempted&&c.presses==1,"Network selected no replay");}
    {Calls c;c.pending=true;n::Receipt r;e.clear();check(n::prepare(c,r,e)&&r.confirmed&&r.returned&&!r.unknown&&!r.state.ready&&r.state.cause.find("view pending")!=std::string::npos&&c.presses==1,"Network asynchronous layout is pending");choose(c.m);check(n::inspect(c.m,image,world,p,e)&&n::status(c.m,p,s,e)&&s.ready&&c.presses==1,"Network layout settles by observation only");}
    {auto m=baseline();choose(m);m.put(content_address+0x368,uintptr_t(0));e.clear();check(n::inspect(m,image,world,p,e)&&n::status(m,p,s,e)&&!s.ready&&s.cause=="Network original view request pending","Network request creation is pending");}
    {auto m=baseline();choose(m);m.put(content_address+0x280,uintptr_t(0));e.clear();check(n::inspect(m,image,world,p,e)&&n::status(m,p,s,e)&&!s.ready&&s.cause=="Network content layout owner pending","Network null layout owner is pending");}
    {auto m=baseline();choose(m);m.put(content_address+0x280,uintptr_t(77));e.clear();check(n::inspect(m,image,world,p,e)&&!n::status(m,p,s,e)&&e=="Network content foreign parent/window owner","Network foreign owner fails");}
    {auto m=baseline();choose(m);m.put(request,int32_t(1));e.clear();check(n::inspect(m,image,world,p,e)&&!n::status(m,p,s,e)&&e=="Network original view request mode differs","Network foreign request mode fails");}
    {auto m=baseline();choose(m);m.array(world+0x180+3*24,{request,request});e.clear();check(n::inspect(m,image,world,p,e)&&!n::status(m,p,s,e)&&e=="Network request not uniquely owned by WorldViewManager","Network duplicate request fails");}
    {Calls c;c.reserved=true;n::Receipt r;e.clear();check(!n::prepare(c,r,e)&&!r.attempted&&c.presses==0&&e=="consumed token","Network token once before original");}
    {Calls c;c.press_ok=false;n::Receipt r;e.clear();check(!n::prepare(c,r,e)&&r.attempted&&!r.returned&&r.unknown&&e=="original SEH"&&c.presses==1,"Network original failure retained");}
    {Calls c;c.ledger_ok=false;n::Receipt r;e.clear();check(!n::prepare(c,r,e)&&r.unknown&&!r.confirmed&&e=="full20 ledger changed","Network ledger failure retained");}
    {Calls c;c.mutate_owner=true;n::Receipt r;e.clear();check(!n::prepare(c,r,e)&&r.unknown&&!r.confirmed&&c.presses==1,"Network changed owner remains Unknown");}
    {Calls c;c.mutate_final=true;n::Receipt r;e.clear();check(!n::prepare(c,r,e)&&r.unknown&&!r.confirmed&&c.presses==1&&e.find("WorldView owner")!=std::string::npos,"Network final owner revalidation");}
    {Calls c;c.throw_press=true;n::Receipt r;try{n::prepare(c,r,e);check(false,"Network expected throw");}catch(const std::runtime_error&){check(r.attempted&&r.unknown&&!r.confirmed&&c.presses==1,"Network exception Unknown");}}
    {Memory m;const auto& bytes=mt2mp::game::tutorial_network_tab::press_code;for(size_t i=0;i<bytes.size();++i)m.data[image+0x57c950+i]=bytes[i];e.clear();check(n::verify(m,image,e),"Network complete original Press body");m.data[image+0x57c950+bytes.size()-1]^=1;check(!n::verify(m,image,e)&&e=="Network original Tab::Press body differs","Network body tail mutation fails");}
}
}

int main(){try{network_adapter_fixture::run();std::string e;t::Pin p;auto m=baseline();check(t::inspect(m,image,world,p,e)&&p.network==3,"read real enum membership");choose(m);check(t::inspect(m,image,world,p,e)&&t::selected(m,p,e),"owned selected request");
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
