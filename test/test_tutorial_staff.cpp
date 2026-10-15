#include "tutorial_replication/staff_apply.h"
#include "tutorial_replication/owned_staff_entry.h"
#include <map>
#include <iostream>
#include <stdexcept>
namespace staff=mt2mp::game::tutorial_replication::staff;
namespace sp=mt2mp::game::tutorial_replication::startingpoint;
namespace action=mt2mp::action_session;
namespace {
unsigned checks=0;
void need(bool value,const char* cause){if(!value)throw std::runtime_error(cause);++checks;}
sp::Row project(){sp::Row r;r.project_present=r.purchased=true;return r;}
struct Calls {
    sp::Row row=project();action::Ledger ledger;unsigned reads=0,allocations=0;
    bool admit_ok=true,read_ok=true,allocate_ok=true,wrong_row=false,wrong_ledger=false;
    bool admit(int32_t desired,std::string& e){if(!admit_ok||desired>1){e="actual project/maxworkers guard";return false;}return true;}
    bool read(sp::Row& r,action::Ledger& l,std::string& e){++reads;if(!read_ok){e="actual owner changed";return false;}r=row;l=ledger;return true;}
    bool allocate(int32_t value,std::string& e){++allocations;row.desired_workers=value;if(wrong_row)++row.lines;if(wrong_ledger)++ledger.line_counts[10];if(!allocate_ok){e="original setter exception";return false;}return true;}
};
struct Capture {
    unsigned originals=0,submits=0;bool admitted=true,accepted=true;
    void original(){++originals;}
    bool pin(std::string& e){if(!admitted){e="actual owned Pips mismatch";return false;}return true;}
    bool uuid(std::string& id,std::string&){id="11111111-1111-4111-8111-111111111111";return true;}
    bool submit(const std::string&,std::string& e){++submits;if(!accepted){e="Controller busy";return false;}return true;}
};
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;
    void put(uintptr_t p,const void* data,size_t count){auto* b=static_cast<const unsigned char*>(data);for(size_t i=0;i<count;++i)bytes[p+i]=b[i];}
    bool copy(uintptr_t p,void* data,size_t count,std::string& e){auto* b=static_cast<unsigned char*>(data);for(size_t i=0;i<count;++i){auto at=bytes.find(p+i);if(at==bytes.end()){e="original physical read failed";return false;}b[i]=at->second;}return true;}
};
void check(){
    std::string e;bool attempted=false;Calls calls;
    need(staff::apply(project(),1,calls,attempted,e)&&attempted&&calls.allocations==1&&calls.row.desired_workers==1,"original setter not applied once");
    calls={};calls.admit_ok=false;need(!staff::apply(project(),1,calls,attempted,e)&&!attempted&&!calls.allocations&&e=="actual project/maxworkers guard","admission cause lost/effect attempted");
    calls={};calls.row.work=1;need(!staff::apply(project(),1,calls,attempted,e)&&!attempted&&!calls.allocations,"stale BEFORE accepted");
    calls={};calls.read_ok=false;need(!staff::apply(project(),1,calls,attempted,e)&&!attempted&&e=="actual owner changed","owner read cause lost");
    calls={};calls.allocate_ok=false;need(!staff::apply(project(),1,calls,attempted,e)&&attempted&&calls.allocations==1&&e=="original setter exception","partial setter not retained as attempted");
    calls={};calls.wrong_row=true;need(!staff::apply(project(),1,calls,attempted,e)&&attempted,"unrelated progress changed");
    calls={};calls.wrong_ledger=true;need(!staff::apply(project(),1,calls,attempted,e)&&attempted,"unrelated ledger changed");
    calls={};need(!staff::apply(project(),2,calls,attempted,e)&&!attempted&&!calls.allocations,"native worker maximum ignored");
    for(int value:{-1,51}){calls={};need(!staff::apply(project(),value,calls,attempted,e)&&!attempted&&!calls.allocations,"invalid desired count accepted");}
    auto r=project();r.project_present=false;r.purchased=false;calls={};need(!staff::apply(r,1,calls,attempted,e)&&!attempted,"absent project admitted");
    Capture capture;staff::Receipt receipt;
    staff::capture(false,1,capture,receipt);need(capture.originals==1&&!receipt.suppressed&&!capture.submits,"Host/local original suppressed");
    capture={};staff::capture(true,1,capture,receipt);need(!capture.originals&&capture.submits==1&&receipt.suppressed&&receipt.submitted&&receipt.desired==1,"Client capture mutated locally or lost submission");
    capture={};capture.admitted=false;staff::capture(true,1,capture,receipt);need(!capture.originals&&!capture.submits&&receipt.cause=="actual owned Pips mismatch","failed capture fell through");
    capture={};capture.accepted=false;staff::capture(true,1,capture,receipt);need(!capture.originals&&capture.submits==1&&!receipt.submitted&&receipt.cause=="Controller busy","failed submit fell through");
    need(staff::capture_route(true,true,0x10000000,0x106c82f9)&&!staff::capture_route(false,true,0x10000000,0x106c82f9)&&!staff::capture_route(true,true,0x10000000,0x106c82f8),"exact native UI return routing differs");
    Memory memory;mt2mp::game::tutorial_sync::OwnedSequenceEntry owner{0x1000,0x8000,0x2000,true,true};
    auto entry=staff::allocate_entry;entry[0]=0xe9;int32_t displacement=int32_t(owner.trampoline+20-owner.target-5);std::memcpy(entry.data()+1,&displacement,4);memory.put(owner.target,entry.data(),entry.size());
    std::array<unsigned char,34> trampoline{};std::copy_n(staff::allocate_entry.data(),6,trampoline.data());
    const unsigned char jump[]={0xff,0x25,0,0,0,0};std::memcpy(trampoline.data()+6,jump,6);std::memcpy(trampoline.data()+20,jump,6);
    uintptr_t continuation=owner.target+6;std::memcpy(trampoline.data()+12,&continuation,8);std::memcpy(trampoline.data()+26,&owner.detour,8);memory.put(owner.trampoline,trampoline.data(),trampoline.size());
    const auto verify=[&]{return staff::verify_owned_allocate(owner,owner.target,[&](auto p,auto out,auto count,auto& error){return memory.copy(p,out,count,error);},e);};
    need(verify(),"owned six-byte store/continuation/relay not accepted");
    memory.bytes[owner.trampoline+26]^=1;need(!verify()&&e.find("detour")!=std::string::npos,"foreign relay accepted");memory.bytes[owner.trampoline+26]^=1;
    memory.bytes[owner.target+6]^=1;need(!verify(),"changed native RET accepted");memory.bytes[owner.target+6]^=1;
    owner.enabled=false;need(!verify(),"unconfirmed hook accepted");
}
}
int main(){try{check();std::cout<<"PASS "<<checks<<" actual staff apply/capture/physical hook assertions\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
