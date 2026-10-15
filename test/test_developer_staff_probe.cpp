#include "developer_staff/probe.h"
#include "developer_staff/window.h"
#include <functional>
#include <iostream>
#include <map>
#include <stdexcept>
namespace staff=mt2mp::game::developer_staff;
namespace {
unsigned checks=0;
void check(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
staff::Request request(staff::Command command=staff::Command::Hire){return {command,"11111111-1111-4111-8111-111111111111","22222222-2222-4222-8222-222222222222",7,9,6};}
struct Port:staff::ProbePort {
    staff::Capture value;unsigned admissions=0,calls=0,afters=0;
    bool auth_ok=true,call_ok=true,after_ok=true,throw_call=false,throw_after=false,missing_cleanup=false;
    std::function<void(unsigned)> on_auth;
    std::function<void(staff::Capture&)> on_after;
    Port(){value.pin.origin={reinterpret_cast<void*>(1),reinterpret_cast<void*>(2),reinterpret_cast<void*>(3),reinterpret_cast<void*>(4),reinterpret_cast<void*>(5),9,7,100,"22222222-2222-4222-8222-222222222222",std::string(64,'a')};
        value.image=0x140000000;value.thread=7;value.state.business_count=value.state.lounge_count=6;value.state.rows.resize(50);
        value.pin.ledger.cash=value.pin.ledger.record_cash=1000;value.pin.ledger.record_present=1;value.pin.ledger.record_day=value.pin.ledger.flat_day=8;
    }
    bool authorize(staff::Capture& out,uintptr_t& window,std::string& e)override{
        ++admissions;if(on_auth)on_auth(admissions);if(!auth_ok){e="actual authorization rejected";return false;}out=value;window=0x123456;return true;
    }
    bool call(staff::Command command,uintptr_t,uintptr_t,staff::NativeReceipt& r,std::string& e)override{
        ++calls;r.constructed=r.label_confirmed=r.attempted=true;if(throw_call)throw std::runtime_error("original call exception");
        if(!call_ok){e="original native UICommand SEH=123";r.seh_code=123;return false;}
        r.returned=r.accepted=r.cleanup_attempted=true;r.cleanup_confirmed=!missing_cleanup;
        if(command==staff::Command::Show){r.constructed=r.label_confirmed=r.accepted=r.cleanup_attempted=r.cleanup_confirmed=false;return true;}
        value.state.business_count+=command==staff::Command::Hire?1:-1;
        if(command==staff::Command::Hire){auto& l=value.pin.ledger;l.cash-=500;l.record_cash-=500;l.record_expense+=500;++l.line_counts[10];l.line_income[10]-=500;}
        return true;
    }
    bool after(staff::Capture& out,std::string& e)override{
        ++afters;if(throw_after)throw std::runtime_error("original readback exception");if(!after_ok){e="actual readback failed";return false;}
        if(on_after)on_after(value);out=value;return true;
    }
};
void fixtures(){std::string e;staff::Receipt out;
    {staff::Probe probe;Port port;check(probe.run(request(),port,out,e)&&out.immediate_effect&&!out.unknown&&port.calls==1&&port.admissions==2&&port.afters==1,"actual Hire probe did not acknowledge exact original immediate effect");
        const auto encoded=staff::encode(out);check(encoded.at("peer_ack")==false&&encoded.at("retirement_confirmed")==false&&encoded.at("after").at("ledger").at("cash")==500,"probe receipt overclaimed retirement/peer completion or lost full accounting");
        check(!probe.run(request(),port,out,e)&&out.replay_rejected&&out.immediate_effect&&port.calls==1,"successful token replayed original UICommand");}
    {staff::Probe probe;Port port;check(probe.run(request(staff::Command::Fire),port,out,e)&&out.after.state.business_count==5&&out.after.state.lounge_count==6&&out.before.pin.ledger==out.after.pin.ledger,"Fire probe hid asynchronous employment or charged refund");}
    {staff::Probe probe;Port port;check(probe.run(request(staff::Command::Show),port,out,e)&&out.immediate_effect&&!out.native.constructed&&out.before.state==out.after.state&&out.before.pin.ledger==out.after.pin.ledger,"Show demanded invented command-record receipt or changed gameplay");}
    {staff::Probe probe;Port port;port.on_after=[](auto& c){--c.pin.ledger.cash;};check(!probe.run(request(staff::Command::Show),port,out,e)&&out.unknown,"Show changed gameplay but reported display-only success");}
    {staff::Probe probe;Port port;port.auth_ok=false;check(!probe.run(request(),port,out,e)&&e=="actual authorization rejected"&&port.calls==0&&probe.retained().empty(),"pre-admission failure consumed token or called native");}
    {staff::Probe probe;Port port;port.value.pin.origin.generation++;check(!probe.run(request(),port,out,e)&&port.calls==0&&e.find("Origin")!=std::string::npos,"foreign World origin admitted");}
    {staff::Probe probe;Port port;port.value.pin.time_scale=1;check(!probe.run(request(),port,out,e)&&port.calls==0,"running Clock admitted for fixture");}
    {staff::Probe probe;Port port;port.value.state.lounge_count=5;check(!probe.run(request(),port,out,e)&&port.calls==0,"unsettled native count admitted");}
    {staff::Probe probe;Port port;port.value.pin.ledger.cash=port.value.pin.ledger.record_cash=499;check(!probe.run(request(),port,out,e)&&port.calls==0,"unaffordable Hire called native");}
    {staff::Probe probe;Port port;auto r=request();r.expected_count=50;port.value.state.business_count=port.value.state.lounge_count=50;check(!probe.run(r,port,out,e)&&port.calls==0,"full pool hired51");}
    {staff::Probe probe;Port port;auto r=request(staff::Command::Fire);r.expected_count=0;port.value.state.business_count=port.value.state.lounge_count=0;check(!probe.run(r,port,out,e)&&port.calls==0,"empty pool fired below0");}
    {staff::Probe probe;Port port;port.on_auth=[&](unsigned n){if(n==2)++port.value.pin.ledger.line_counts[19];};check(!probe.run(request(),port,out,e)&&port.calls==0&&e.find("changed before")!=std::string::npos,"second preflight changed fullLedger accepted");}
    {staff::Probe probe;Port port;port.call_ok=false;check(!probe.run(request(),port,out,e)&&out.unknown&&probe.locked()&&e=="original native UICommand SEH=123"&&port.afters==0,"partial native failure lost original cause/Unknown");
        check(!probe.run(request(),port,out,e)&&out.replay_rejected&&port.calls==1,"failed native token replayed");
        auto different=request();different.token[0]='3';check(!probe.run(different,port,out,e)&&port.calls==1&&e==probe.cause(),"new token bypassed retained Unknown");}
    {staff::Probe probe;Port port;port.missing_cleanup=true;check(!probe.run(request(),port,out,e)&&out.unknown&&probe.locked()&&e.find("cleanup")!=std::string::npos,"incomplete genuine record cleanup reported success");}
    {staff::Probe probe;Port port;port.throw_call=true;check(!probe.run(request(),port,out,e)&&out.unknown&&probe.locked()&&e.find("original call exception")!=std::string::npos,"native callback exception became success");}
    {staff::Probe probe;Port port;port.throw_after=true;check(!probe.run(request(),port,out,e)&&out.unknown&&probe.locked()&&e.find("original readback exception")!=std::string::npos,"post-call readback exception became success");}
    {staff::Probe probe;Port port;port.after_ok=false;check(!probe.run(request(),port,out,e)&&out.unknown&&e=="actual readback failed","post-call read failure hidden");}
    {staff::Probe probe;Port port;port.on_after=[](auto& c){++c.pin.ledger.line_counts[19];};check(!probe.run(request(),port,out,e)&&out.unknown&&!out.immediate_effect,"unrelated twentieth ledger effect was acknowledged");}
    {staff::Probe probe;Port port;port.on_after=[](auto& c){++c.state.lounge_count;};check(!probe.run(request(),port,out,e)&&out.unknown,"unexpected native population mutation was accepted as count-only effect");}
    {staff::Probe probe;Port port;port.on_after=[](auto& c){c.state.business_count+=1;};check(!probe.run(request(),port,out,e)&&out.unknown,"double native count execution acknowledged");}
    {staff::Probe probe;Port port;port.on_after=[](auto& c){c.state.business_count-=1;};check(!probe.run(request(),port,out,e)&&out.unknown,"missing native count execution acknowledged");}
    {staff::Probe probe;Port port;port.on_after=[](auto& c){++c.pin.origin.generation;};check(!probe.run(request(),port,out,e)&&out.unknown,"post-call World replacement acknowledged");}
    {staff::Probe probe;Port port;auto r=request();r.token="bad";check(!probe.run(r,port,out,e)&&port.admissions==0&&port.calls==0,"invalid token reached native admission");}
}
struct WindowMemory {
    std::map<uintptr_t,uint8_t> bytes;uintptr_t unreadable=0;
    template<class T>void put(uintptr_t address,T value){const auto* p=reinterpret_cast<const uint8_t*>(&value);for(size_t i=0;i<sizeof(T);++i)bytes[address+i]=p[i];}
    WindowMemory(){put(0x1000+0xa0,uintptr_t(0x2000));put(0x1000+0xa8,int32_t(1));put(0x1000+0xac,int32_t(4));put(0x2000,uintptr_t(0x3000));put(0x3000,uintptr_t(0x140000000+0x1722e60));put(0x3000+0x1b0,uintptr_t(0x4000));put(0x3000+0x1b8,uint64_t(11));put(0x3000+0x318,uintptr_t(0));const char name[]="development";for(size_t i=0;i<sizeof(name);++i)bytes[0x4000+i]=uint8_t(name[i]);}
    bool copy(uintptr_t p,void* out,size_t count,std::string& e){if(p==unreadable){e="actual window read failed";return false;}auto* target=static_cast<uint8_t*>(out);for(size_t i=0;i<count;++i){if(!bytes.contains(p+i)){e="unmapped window fixture";return false;}target[i]=bytes.at(p+i);}return true;}
};
void windows(){staff::Capture c;c.image=0x140000000;c.pin.origin.mode=reinterpret_cast<void*>(0x1000);std::string e;uintptr_t out=0;
    auto read=[&](WindowMemory& m){return staff::read_window([&](uintptr_t p,void* target,size_t n,std::string& error){return m.copy(p,target,n,error);},c,out,e);};
    {WindowMemory m;check(read(m)&&out==0x3000,"actual owned DevelopmentWindow rejected");}
    {WindowMemory m;m.put(0x1000+0xac,int32_t(0));check(!read(m)&&e.find("bounds")!=std::string::npos,"invalid actual Mode array accepted");}
    {WindowMemory m;m.put(0x2000,uintptr_t(0));check(!read(m)&&e.find("null/duplicate")!=std::string::npos,"null native window accepted");}
    {WindowMemory m;m.put(0x1000+0xa8,int32_t(2));m.put(0x2008,uintptr_t(0x3000));check(!read(m)&&e.find("null/duplicate")!=std::string::npos,"duplicate actual owned window accepted");}
    {WindowMemory m;m.put(0x3000,uintptr_t(0x140000000+0x1722e61));check(!read(m)&&e.find("absent")!=std::string::npos,"foreign window class accepted");}
    {WindowMemory m;m.put(0x3000+0x318,uintptr_t(1));check(!read(m)&&e.find("logic")!=std::string::npos,"custom native window logic accepted");}
    {WindowMemory m;m.put(0x3000+0x1b8,uint64_t(12));check(!read(m)&&e.find("identity")!=std::string::npos,"wrong exact native window ID length accepted");}
    {WindowMemory m;m.bytes[0x4000]='x';check(!read(m)&&e.find("ID differs")!=std::string::npos,"wrong actual window ID accepted");}
    {WindowMemory m;m.bytes[0x400b]='x';check(!read(m)&&e.find("ID differs")!=std::string::npos,"unterminated exact window ID accepted");}
    {WindowMemory m;m.unreadable=0x3000+0x318;check(!read(m)&&e=="actual window read failed","upstream native window read cause replaced");}
}
}
int main(){try{fixtures();windows();std::cout<<checks<<" actual Developer probe/window boundary checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
