#include "native_tool_prepare/buildings_tab.h"
#include "native_tool_prepare/scenery_tab.h"
#include <map>
#include <iostream>
#include <stdexcept>
namespace t=mt2mp::game::buildings_tab;
struct Memory{
 std::map<uintptr_t,unsigned char> data;uintptr_t allocation=0x900000;
 bool copy(uintptr_t p,void* out,size_t n,std::string& e){for(size_t i=0;i<n;++i){auto f=data.find(p+i);if(f==data.end()){e="missing fixture byte";return false;}static_cast<unsigned char*>(out)[i]=f->second;}return true;}
 template<class T>void put(uintptr_t p,T value){auto b=reinterpret_cast<unsigned char*>(&value);for(size_t i=0;i<sizeof(T);++i)data[p+i]=b[i];}
 void str(uintptr_t p,const std::string& s){auto q=allocation;allocation+=0x200;put(p,q);put(p+8,uint64_t(s.size()));for(size_t i=0;i<=s.size();++i)data[q+i]=static_cast<unsigned char>(s.c_str()[i]);}
 void array(uintptr_t h,std::vector<uintptr_t> a){auto q=allocation;allocation+=0x200;put(h+8,q);put(h+16,int32_t(a.size()));put(h+20,int32_t(a.size()));for(size_t i=0;i<a.size();++i)put(q+i*8,a[i]);}
};
constexpr uintptr_t image=0x10000000,world=0x100000,bar=0x200000,tab_address=0x210000,content_address=0x220000,cursor=0x230000,request=0x240000;
Memory baseline(){Memory m;m.put(image+0x13df650,world+0x180);for(int i=0;i<5;++i)m.str(image+0x1a36520+uintptr_t(i)*32,i==3?"Edit":"Other"+std::to_string(i));m.put(world+0x1f8,int32_t(0));
 m.put(image+0x1a3aec8,bar);m.put(bar,image+0x16dcec0);m.array(image+0x1a3af10,{bar});m.put(bar+0x69,uint8_t(1));m.put(bar+0x45f,uint8_t(1));m.put(bar+0x377,uint8_t(0));m.put(bar+0x318,uintptr_t(0));m.str(bar+0x1b0,"Edit");m.array(bar+0x518,{tab_address});m.array(bar+0x2f8,{tab_address});m.array(bar+0x4e8,{content_address});
 m.put(tab_address,image+0x16fe380);m.str(tab_address+0x2c8,"Buildings");m.put(tab_address+0x278,bar);m.put(tab_address+0x280,bar);m.put(tab_address+0x69,uint8_t(1));m.put(tab_address+0x174,uint8_t(1));m.put(tab_address+0x360,uint8_t(0));m.str(tab_address+0x308,"ActionBar Edit SelectContent \"Buildings\"");
 m.put(content_address,image+0x171ccf0);m.str(content_address+0x1b0,"Buildings");m.put(content_address+0x348,int32_t(3));m.put(content_address+0x368,uintptr_t(0));m.put(content_address+0x34c,int32_t(5));m.put(world+0x1fc,int32_t(0));m.put(bar+0x598,uintptr_t(0));m.put(content_address+0x278,uintptr_t(0));m.put(content_address+0x280,uintptr_t(0));m.put(content_address+0x69,uint8_t(0));m.put(image+0x13df9d0,cursor);m.put(cursor,image+0x1786dd0);return m;}
void choose(Memory& m){m.put(world+0x1fc,int32_t(5));m.put(tab_address+0x360,uint8_t(1));m.put(world+0x1f8,int32_t(3));m.put(content_address+0x368,request);m.put(request,int32_t(3));m.put(content_address+0x278,bar);m.put(content_address+0x280,bar);m.put(content_address+0x69,uint8_t(1));m.put(bar+0x598,content_address);m.array(world+0x180+3*24,{request});}
int checks=0;void check(bool value,const char* name){++checks;if(!value)throw std::runtime_error(name);}
struct Calls {Memory m=baseline();bool reserved=false,press_ok=true,ledger_ok=true,throw_press=false;int presses=0,pins=0;bool pending=false,changed_after_sample=false;bool changed=false;
 bool pin(t::Pin& p,std::string& e){++pins;if(changed&&pins==3)m.put(tab_address+0x278,uintptr_t(0));return t::inspect(m,image,world,p,e);}
 bool reserve(const t::Pin&,std::string& e){if(reserved){e="consumed";return false;}reserved=true;return true;}
 bool press(const t::Pin&,std::string& e){++presses;if(throw_press)throw std::runtime_error("native boundary");if(!press_ok){e="original SEH";return false;}choose(m);if(pending)m.put(world+0x1f8,int32_t(0));return true;}
 bool status(const t::Pin& p,t::Status& out,std::string& e){return t::status(m,p,out,e);}
 bool unchanged(std::string& e){if(changed_after_sample)m.put(image+0x13df650,world+0x188);if(!ledger_ok)e="ledger cause";return ledger_ok;}
};

namespace scenery_adapter_fixture {
namespace st=mt2mp::game::scenery_tab;
Memory scenery_baseline(){
    auto m=baseline();m.str(tab_address+0x2c8,"Scenery");
    m.str(tab_address+0x308,"ActionBar Edit SelectContent \"Scenery\"");
    m.str(content_address+0x1b0,"Scenery");m.put(content_address+0x34c,int32_t(7));return m;
}
void scenery_choose(Memory& m){choose(m);m.put(world+0x1fc,int32_t(7));}
struct Calls {
    Memory m=scenery_baseline();int presses=0,pins=0;bool reserved=false,pending=false,press_ok=true,ledger_ok=true;
    bool mutate_owner=false,mutate_final=false,throw_press=false;
    bool pin(st::Pin& p,std::string& e){++pins;if(mutate_owner&&pins==3)m.put(tab_address+0x280,bar+8);return st::inspect(m,image,world,p,e);}
    bool reserve(const st::Pin&,std::string& e){if(reserved){e="consumed token";return false;}reserved=true;return true;}
    bool press(const st::Pin&,std::string& e){++presses;if(throw_press)throw std::runtime_error("native boundary");if(!press_ok){e="original SEH";return false;}scenery_choose(m);if(pending)m.put(world+0x1fc,int32_t(5));return true;}
    bool status(const st::Pin& p,st::Status& out,std::string& e){return st::status(m,p,out,e);}
    bool unchanged(std::string& e){if(mutate_final)m.put(image+0x13df650,world+0x188);if(!ledger_ok)e="full20 ledger changed";return ledger_ok;}
};
void run(){
    std::string e;st::Pin p;st::Status s;
    {auto m=scenery_baseline();check(st::inspect(m,image,world,p,e)&&p.edit==3&&p.edit_kind==7,"Scenery reads actual Edit mode and own subtype");scenery_choose(m);check(st::inspect(m,image,world,p,e)&&st::status(m,p,s,e)&&s.ready,"Scenery exact current native content");}
    {Calls c;st::Receipt r;check(st::prepare(c,r,e)&&r.attempted&&r.returned&&r.confirmed&&!r.unknown&&r.state.ready&&c.presses==1,"Scenery original once");check(!st::prepare(c,r,e)&&!r.attempted&&c.presses==1,"Scenery selected no replay");}
    {Calls c;c.pending=true;st::Receipt r;e.clear();check(st::prepare(c,r,e)&&r.confirmed&&r.returned&&!r.unknown&&!r.state.ready&&r.state.cause.find("edit subtype pending")!=std::string::npos&&c.presses==1,"Scenery asynchronous subtype is pending");scenery_choose(c.m);check(st::inspect(c.m,image,world,p,e)&&st::status(c.m,p,s,e)&&s.ready&&c.presses==1,"Scenery layout settles without another Press");}
    {auto m=scenery_baseline();m.str(tab_address+0x2c8,"Buildings");e.clear();check(!st::inspect(m,image,world,p,e)&&e=="Scenery tab absent from actual ActionBar","Scenery missing tab rejected");}
    {auto m=scenery_baseline();m.put(tab_address+0x69,uint8_t(0));e.clear();check(!st::inspect(m,image,world,p,e),"Scenery hidden tab rejected");}
    {auto m=scenery_baseline();m.put(tab_address+0x174,uint8_t(0));e.clear();check(!st::inspect(m,image,world,p,e),"Scenery disabled tab rejected");}
    {auto m=scenery_baseline();m.str(tab_address+0x308,"ActionBar Edit SelectContent \"Buildings\"");e.clear();check(!st::inspect(m,image,world,p,e)&&e=="Scenery tab authored command differs","Scenery cannot reuse Buildings command");}
    {auto m=scenery_baseline();m.put(content_address+0x348,int32_t(1));e.clear();check(!st::inspect(m,image,world,p,e),"Scenery does not accept Network view");}
    {auto m=scenery_baseline();m.array(bar+0x518,{tab_address,tab_address});e.clear();check(!st::inspect(m,image,world,p,e),"Scenery duplicate tab rejected");}
    {auto m=scenery_baseline();scenery_choose(m);m.put(content_address+0x368,uintptr_t(0));e.clear();check(st::inspect(m,image,world,p,e)&&st::status(m,p,s,e)&&!s.ready&&s.cause=="Scenery original view request pending","Scenery request creation pending");}
    {auto m=scenery_baseline();scenery_choose(m);m.put(content_address+0x280,uintptr_t(0));e.clear();check(st::inspect(m,image,world,p,e)&&st::status(m,p,s,e)&&!s.ready&&s.cause=="Scenery content layout owner pending","Scenery null layout pending");}
    {auto m=scenery_baseline();scenery_choose(m);m.put(content_address+0x280,uintptr_t(77));e.clear();check(st::inspect(m,image,world,p,e)&&!st::status(m,p,s,e)&&e=="Scenery content foreign parent/window owner","Scenery foreign owner fails");}
    {auto m=scenery_baseline();scenery_choose(m);m.put(request,int32_t(1));e.clear();check(st::inspect(m,image,world,p,e)&&!st::status(m,p,s,e)&&e=="Scenery original view request mode differs","Scenery foreign view request fails");}
    {auto m=scenery_baseline();scenery_choose(m);m.array(world+0x180+3*24,{request,request});e.clear();check(st::inspect(m,image,world,p,e)&&!st::status(m,p,s,e),"Scenery duplicate request fails");}
    {Calls c;c.reserved=true;st::Receipt r;e.clear();check(!st::prepare(c,r,e)&&!r.attempted&&c.presses==0&&e=="consumed token","Scenery token once before original");}
    {Calls c;c.press_ok=false;st::Receipt r;e.clear();check(!st::prepare(c,r,e)&&r.attempted&&!r.returned&&r.unknown&&e=="original SEH"&&c.presses==1,"Scenery original failure retained");}
    {Calls c;c.ledger_ok=false;st::Receipt r;e.clear();check(!st::prepare(c,r,e)&&r.unknown&&!r.confirmed&&e=="full20 ledger changed","Scenery full ledger failure retained");}
    {Calls c;c.mutate_owner=true;st::Receipt r;e.clear();check(!st::prepare(c,r,e)&&r.unknown&&!r.confirmed&&c.presses==1,"Scenery changed owner Unknown");}
    {Calls c;c.mutate_final=true;st::Receipt r;e.clear();check(!st::prepare(c,r,e)&&r.unknown&&!r.confirmed&&c.presses==1&&e.find("WorldView owner")!=std::string::npos,"Scenery final owner revalidation");}
    {Calls c;c.throw_press=true;st::Receipt r;try{st::prepare(c,r,e);check(false,"Scenery expected throw");}catch(const std::runtime_error&){check(r.attempted&&r.unknown&&!r.confirmed,"Scenery exception Unknown");}}
    {Memory m;const auto& bytes=mt2mp::game::tutorial_network_tab::press_code;for(size_t i=0;i<bytes.size();++i)m.data[image+0x57c950+i]=bytes[i];e.clear();check(st::verify(m,image,e),"Scenery complete original Press pin");m.data[image+0x57c950+bytes.size()-1]^=1;check(!st::verify(m,image,e)&&e=="Scenery original Tab::Press body differs","Scenery body tail mutation fails");}
}
}

int main(){try{scenery_adapter_fixture::run();std::string e;t::Pin p;t::Status status;auto m=baseline();check(t::inspect(m,image,world,p,e)&&p.edit==3,"read real enum membership");choose(m);check(t::inspect(m,image,world,p,e)&&t::status(m,p,status,e)&&status.ready,"owned selected request");
 auto reject=[&](auto mutate,const char* label){auto x=baseline();mutate(x);e.clear();check(!t::inspect(x,image,world,p,e)&&!e.empty(),label);};
 reject([](Memory& x){x.put(image+0x13df650,world+0x188);},"foreign manager");
 reject([](Memory& x){x.str(image+0x1a36520+32,"Edit");},"duplicate enum");
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
 {auto x=baseline();choose(x);x.array(world+0x180+3*24,{});e.clear();check(t::inspect(x,image,world,p,e)&&!t::status(x,p,status,e),"request owner absence");}
 {Calls c;t::Receipt r;e.clear();check(t::prepare(c,r,e)&&r.attempted&&r.returned&&r.confirmed&&!r.unknown&&c.presses==1,"original once");check(!t::prepare(c,r,e)&&!r.attempted&&c.presses==1,"selected no replay");}
 {Calls c;c.press_ok=false;t::Receipt r;e.clear();check(!t::prepare(c,r,e)&&r.attempted&&!r.returned&&r.unknown&&e=="original SEH","partial original cause");}
 {Calls c;c.changed=true;t::Receipt r;e.clear();check(!t::prepare(c,r,e)&&r.returned&&r.unknown&&!r.confirmed,"post owner fail");}
 {Calls c;c.ledger_ok=false;t::Receipt r;e.clear();check(!t::prepare(c,r,e)&&r.unknown&&e=="ledger cause","ledger unchanged");}
 {Calls c;c.throw_press=true;t::Receipt r;try{t::prepare(c,r,e);check(false,"expected throw");}catch(const std::runtime_error&){check(r.attempted&&r.unknown&&!r.confirmed,"exception receipt");}}

 {auto x=baseline();e.clear();check(t::inspect(x,image,world,p,e)&&t::status(x,p,status,e)&&!status.ready&&status.cause=="Buildings tab not active","inactive is effectless not ready");}
 {Calls c;c.pending=true;t::Receipt out;e.clear();check(t::prepare(c,out,e)&&out.confirmed&&out.returned&&!out.unknown&&!out.state.ready&&out.state.cause.find("Edit view pending")!=std::string::npos&&c.presses==1,"original once with asynchronous mode pending");choose(c.m);check(t::inspect(c.m,image,world,p,e)&&t::status(c.m,p,status,e)&&status.ready&&c.presses==1,"read-only mode settles without another Press");}
 {Calls c;c.reserved=true;t::Receipt out;e.clear();check(!t::prepare(c,out,e)&&!out.attempted&&c.presses==0&&e=="consumed","used token before original");}
 {Calls c;c.changed_after_sample=true;t::Receipt out;e.clear();check(!t::prepare(c,out,e)&&out.returned&&out.unknown&&!out.confirmed&&c.presses==1&&e.find("WorldView owner")!=std::string::npos,"final full readback owner guard remains Unknown");}
 {auto x=baseline();choose(x);x.put(content_address+0x280,uintptr_t(77));e.clear();check(t::inspect(x,image,world,p,e)&&!t::status(x,p,status,e)&&e=="Buildings content foreign parent/window owner","foreign is failure not pending");}
 {auto x=baseline();choose(x);x.put(content_address+0x280,uintptr_t(0));e.clear();check(t::inspect(x,image,world,p,e)&&t::status(x,p,status,e)&&!status.ready&&status.cause=="Buildings content layout owner pending","null layout deferred without acceptance");}
 {auto x=baseline();choose(x);x.put(request,int32_t(1));e.clear();check(t::inspect(x,image,world,p,e)&&!t::status(x,p,status,e)&&e=="Buildings original view request mode differs","foreign request mode fails");}
 {auto x=baseline();choose(x);x.put(world+0x1fc,int32_t(1));e.clear();check(t::inspect(x,image,world,p,e)&&t::status(x,p,status,e)&&!status.ready&&status.cause.find("edit subtype pending")!=std::string::npos,"native subtype remains pending");}
 {auto x=baseline();choose(x);x.array(world+0x180+3*24,{request,request});e.clear();check(t::inspect(x,image,world,p,e)&&!t::status(x,p,status,e),"duplicate request owner fails");}
 {Memory x;const auto& bytes=mt2mp::game::tutorial_network_tab::press_code;for(size_t i=0;i<bytes.size();++i)x.data[image+0x57c950+i]=bytes[i];e.clear();check(t::verify(x,image,e),"full original Press body");x.data[image+0x57c950+bytes.size()-1]^=1;check(!t::verify(x,image,e)&&e=="Buildings original Tab::Press body differs","body tail tamper fails");}
 std::cout<<"PASS "<<checks<<" actual reader/orchestration cases\n";return 0;}catch(const std::exception& x){std::cerr<<x.what()<<"\n";return 1;}}
