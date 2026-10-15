#include "developer_staff/read.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
namespace staff=mt2mp::game::developer_staff;
namespace action=mt2mp::action_session;
namespace {
unsigned checks=0;
void need(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
void check(bool ok,const std::string& why){++checks;need(ok,why);}
struct Memory:staff::Port {
    static constexpr uintptr_t image=0x140000000,state=0x100000,lounge=0x200000,business=0x300000,inside=0x400000,outside=0x500000,catalogue=0x600000,indoor_data=0x700000,outdoor_data=0x800000;
    std::map<uintptr_t,uint8_t> bytes;staff::Pin value;unsigned pins=0;uintptr_t unreadable=0;bool resolver_ok=true;std::function<void(unsigned)> on_pin;
    static uintptr_t dev(size_t row){return 0x1000000+row*0x1000;}
    template<class T>void seed(uintptr_t address,T data){const auto* b=reinterpret_cast<const uint8_t*>(&data);for(size_t i=0;i<sizeof(T);++i)bytes[address+i]=b[i];}
    Memory(){
        value.origin={reinterpret_cast<void*>(1),reinterpret_cast<void*>(2),reinterpret_cast<void*>(state),reinterpret_cast<void*>(4),reinterpret_cast<void*>(5),9,7,100,"11111111-1111-4111-8111-111111111111",std::string(64,'a')};
        value.ledger.cash=value.ledger.record_cash=1000;value.ledger.record_present=1;value.ledger.flat_day=value.ledger.record_day=7;
        value.pause_scale=1;seed(state+0x180,lounge);seed(state+0x1c0,business);seed(lounge,image+0x16de6c0);seed(business,image+0x16d97e0);seed(image+0x13df290,lounge);
        seed(business+0x4c,int32_t(6));seed(lounge+0x310,int32_t(6));seed(lounge+0x70,catalogue);seed(lounge+0x78,int32_t(50));seed(lounge+0x7c,int32_t(64));seed(lounge+0x98,inside);seed(lounge+0xa0,outside);
        seed(inside,image+0x16e47f0);seed(outside,image+0x16e47f0);seed(inside+0x38,indoor_data);seed(inside+0x40,int32_t(50));seed(inside+0x44,int32_t(64));seed(outside+0x38,outdoor_data);seed(outside+0x40,int32_t(0));seed(outside+0x44,int32_t(0));
        for(size_t i=0;i<50;++i){const auto p=dev(i);seed(catalogue+i*8,p);seed(indoor_data+i*8,p);seed(p,image+0x17422e0);seed(p+0x4c0,int32_t((i+7)%50));seed(p+0x4c4,int32_t(0));seed(p+0x4d8,uint8_t(i<6));seed(p+0x4d9,uint8_t(i<6));seed(p+0x368,uint8_t(0));seed(p+0x4c8,uintptr_t(0));for(size_t j=0;j<3;++j)seed(p+0x110+j*4,float(i));}
    }
    bool pin(staff::Pin& out,std::string&)override{++pins;if(on_pin)on_pin(pins);out=value;return true;}
    bool resolver(uintptr_t& base,uint32_t& thread,std::string& e)override{if(!resolver_ok){e="original native resolver failure";return false;}base=image;thread=7;return true;}
    bool copy(uintptr_t p,void* out,size_t n,std::string& e)override{if(p==unreadable){e="original owned field read failure";return false;}auto* b=static_cast<uint8_t*>(out);for(size_t i=0;i<n;++i){auto it=bytes.find(p+i);if(it==bytes.end()){e="fixture unmapped field";return false;}b[i]=it->second;}return true;}
};
void reader(){std::string e;staff::Capture out;
    {Memory m;check(staff::read(m,out,e)&&out.state.rows.size()==50&&out.state.business_count==6&&out.local.inside.size()==50&&m.pins==3,"actual full pool reader failed: "+e);check(out.state.rows[0].id==7&&out.state.rows[0].slot==0,"array row was mistaken for persisted ID");const auto encoded=staff::encode(out);check(encoded.at("count_settled")==true&&encoded.at("developers").at(0).at("id")==7&&!encoded.contains("local")&&!encoded.contains("address"),"public receipt lost actual identity or exposed local owner fields");}
    {Memory m;m.seed(Memory::business+0x4c,int32_t(7));check(staff::read(m,out,e)&&staff::encode(out).at("count_settled")==false,"real post-UI pre-SetDevCount transient was hidden or called settled");}
    {Memory m;m.seed(Memory::dev(6)+0x4d9,uint8_t(1));check(staff::read(m,out,e)&&!out.state.rows[6].employed&&out.state.rows[6].active,"departing worker was falsely retired from requested employment alone");}
    {Memory m;m.seed(Memory::dev(1)+0x4c0,int32_t(7));check(!staff::read(m,out,e)&&out.state.rows.empty()&&e.find("persisted ID")!=std::string::npos,"duplicate persisted ID accepted");}
    {Memory m;m.seed(Memory::catalogue+8,Memory::dev(0));check(!staff::read(m,out,e)&&e.find("duplicate receiver")!=std::string::npos,"duplicate local owner accepted");}
    {Memory m;m.seed(Memory::lounge+0x78,int32_t(49));check(!staff::read(m,out,e)&&e.find("fixed pool")!=std::string::npos,"incomplete native pool accepted");}
    {Memory m;m.seed(Memory::outside+0x40,int32_t(1));m.seed(Memory::outside+0x44,int32_t(4));m.seed(Memory::outdoor_data,Memory::dev(0));check(!staff::read(m,out,e)&&e.find("both native managers")!=std::string::npos,"cross-manager duplicated worker accepted");}
    {Memory m;m.seed(Memory::inside+0x40,int32_t(49));check(!staff::read(m,out,e)&&e.find("manager union")!=std::string::npos,"missing worker registration accepted");}
    {Memory m;m.seed(Memory::dev(0)+0x368,uint8_t(1));check(!staff::read(m,out,e)&&e.find("map flag")!=std::string::npos,"map lifecycle disagreement accepted");}
    {Memory m;m.seed(Memory::dev(0)+0x4d8,uint8_t(0));check(!staff::read(m,out,e)&&e.find("employment prefix")!=std::string::npos,"native requested prefix disagreement accepted");}
    {Memory m;m.seed(Memory::dev(0),Memory::image+0x17422e1);check(!staff::read(m,out,e)&&e.find("addresspoint")!=std::string::npos,"foreign actual worker class accepted");}
    {Memory m;m.seed(Memory::image+0x13df290,Memory::lounge+1);check(!staff::read(m,out,e)&&e.find("singleton")!=std::string::npos,"foreign singleton accepted");}
    {Memory m;m.resolver_ok=false;check(!staff::read(m,out,e)&&e=="original native resolver failure"&&!m.pins,"native resolver failure was hidden");}
    {Memory m;m.unreadable=Memory::dev(0)+0x4d9;check(!staff::read(m,out,e)&&e=="original owned field read failure","original read cause lost");}
    {Memory m;m.on_pin=[&](unsigned n){if(n==2)++m.value.ledger.line_counts[19];};check(!staff::read(m,out,e)&&e.find("ledger")!=std::string::npos,"full twentieth ledger group was not pinned");}
    {Memory m;m.on_pin=[&](unsigned n){if(n==2)m.seed(Memory::dev(0)+0x4c4,int32_t(1));};check(!staff::read(m,out,e)&&e.find("second census")!=std::string::npos,"second census actor mutation accepted");}
    {Memory m;m.on_pin=[&](unsigned n){if(n==3)++m.value.origin.generation;};check(!staff::read(m,out,e)&&e.find("after second census")!=std::string::npos,"late native World replacement accepted");}
    {Memory m;m.seed(Memory::dev(0)+0x110,std::numeric_limits<float>::quiet_NaN());check(!staff::read(m,out,e)&&e.find("finite")!=std::string::npos,"nonfinite native pose accepted");}
}
void accounting(){Memory m;auto before=m.value.ledger,after=before;std::string e;
    after.cash-=500;after.record_cash-=500;after.record_expense+=500;++after.line_counts[10];after.line_income[10]-=500;
    check(staff::payment(staff::Command::Hire,before,after,e),"original500 category10 payment rejected");
    auto wrong=after;++wrong.line_counts[0];check(!staff::payment(staff::Command::Hire,before,wrong,e),"unrelated ledger effect accepted");
    wrong=after;wrong.cash++;check(!staff::payment(staff::Command::Hire,before,wrong,e),"wrong Hire price accepted");
    wrong=after;wrong.line_counts[10]--;wrong.line_counts[9]++;check(!staff::payment(staff::Command::Hire,before,wrong,e),"wrong native expense category accepted");
    check(staff::payment(staff::Command::Fire,before,before,e)&&e.empty(),"original Fire no-cost ledger rejected");
    check(!staff::payment(staff::Command::Fire,before,after,e),"Fire incorrectly accepted Hire expense");
    auto invalid=before;invalid.cash=invalid.record_cash=499;check(!staff::payment(staff::Command::Hire,invalid,after,e),"unaffordable Hire accepted");
    invalid=before;invalid.record_present=0;check(!staff::payment(staff::Command::Hire,invalid,after,e),"missing current native record accepted");
    invalid=before;invalid.line_counts[10]=INT32_MAX;check(!staff::payment(staff::Command::Hire,invalid,after,e),"native expense count overflow accepted");
    invalid=before;invalid.record_expense=INT32_MAX;check(!staff::payment(staff::Command::Hire,invalid,after,e),"native expense sum overflow accepted");
    invalid=before;invalid.line_income[10]=INT32_MIN;check(!staff::payment(staff::Command::Hire,invalid,after,e),"native category amount underflow accepted");
}
}
int main(){try{reader();accounting();std::cout<<checks<<" actual Developer census/payment checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<"error: "<<e.what()<<'\n';return 1;}}
