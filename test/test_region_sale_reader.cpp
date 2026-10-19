#include "region_level_reader.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
using namespace mt2mp::game::region_levels;
namespace {
unsigned checks=0;
void check(bool value,const std::string& e){if(!value)throw std::runtime_error(e);++checks;}
struct Memory {
    std::map<uintptr_t,unsigned char> bytes;bool changed=false;unsigned array_reads=0;
    template<class T>void set(uintptr_t p,const T& value){const auto* data=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)bytes[p+i]=data[i];}
    template<class T>bool read(uintptr_t p,size_t offset,T& value,std::string& e){if(changed&&p==0x40000&&offset==0x48&&++array_reads==2){value=T{};e="fixture concurrent array change";return false;}
        auto* data=reinterpret_cast<unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i){auto found=bytes.find(p+offset+i);if(found==bytes.end()){e="fixture missing address="+std::to_string(p+offset+i);return false;}data[i]=found->second;}return true;}
};
constexpr uintptr_t image=0x140000000,region=0x30000;
void actor(Memory& m,uintptr_t object,uintptr_t ap,int32_t level,bool npc=false,int32_t bucket=0){
    m.set(object,image+ap);m.set(object+0x178,region);m.set(object+0x100,std::array<float,10>{0,0,0,1,float(object),8,9,1,1,1});
    m.set(object+(ap==0x1745ce0?0x360:0x6f8),level);m.set(object+0x4a4,std::array<int32_t,2>{3,4});
    if(npc){m.set(object+0x80c,bucket);m.set(object+0x890,int32_t(8));m.set(object+0x870,int32_t(2));}
}
Memory fixture(){Memory m;
    m.set(region+0x998,uintptr_t(0x40000));m.set(0x40000,image+0x16e6aa0);m.set(0x400c8,region);
    m.set(region+0x988,uintptr_t(0x41000));m.set(0x41000,image+0x1714100);m.set(0x41080,region);
    m.set(region+0x968,uintptr_t(0x42000));m.set(0x42000,image+0x16fe170);m.set(0x420f0,region);
    for(int i=0;i<3;++i){const auto off=0x40040+uintptr_t(i)*0x30;m.set(off,uintptr_t(i<2?0x50000+uintptr_t(i)*16:0));m.set(off+8,int32_t(i<2?1:0));m.set(off+12,int32_t(4));}
    m.set(0x41040,uintptr_t(0x51000));m.set(0x41048,int32_t(1));m.set(0x4104c,int32_t(4));
    for(int i=0;i<4;++i){const auto off=0x42020+uintptr_t(i)*0x30;m.set(off,uintptr_t(i==3?0x52000:0));m.set(off+8,int32_t(i==3?1:0));m.set(off+12,int32_t(4));}
    m.set(0x50000,uintptr_t(0x60000));m.set(0x50010,uintptr_t(0x61000));m.set(0x51000,uintptr_t(0x62000));m.set(0x52000,uintptr_t(0x63000));
    actor(m,0x60000,0x1742980,20,true);actor(m,0x61000,0x1742980,12,true,1);actor(m,0x62000,0x16c0770,9);actor(m,0x63000,0x1745ce0,1);return m;
}
bool run(Memory& m,Census& c,std::string& e,std::array<int32_t,2> range={2,4}){return census(image,region,range,c,[&](uintptr_t p,size_t o,auto& v,std::string& error){return m.read(p,o,v,error);},e);}
}
int main(){try{
    auto m=fixture();Census c;std::string e;check(run(m,c,e),e);check(c.counts==std::array<uint64_t,3>{2,1,1},"actor family census differs");
    check(c.levels[0][1]==20&&c.expected[0][1]==14&&c.expected[0][2]==4,"NPC changed level must clamp quest and keep +10 offset");
    check(c.levels[1][1]==12&&c.expected[1][1]==12&&c.expected[1][2]==8,"NPC unchanged level must retain out-of-range quest until original callback");
    check(c.expected[2][1]==4&&c.expected[3][1]==2,"Monster/Zone clamp differs");
    auto local=c;m.set(0x60018,uint64_t(99999));m.set(0x60830,int32_t(99));check(run(m,c,e)&&c.roster==local.roster&&c.levels==local.levels,"local UIDs leaked into canonical census");
    m.set(0x606f8,int32_t(14));m.set(0x60890,int32_t(4));m.set(0x626f8,int32_t(4));m.set(0x63360,int32_t(2));
    check(run(m,c,e)&&c.levels==local.expected&&c.expected==c.levels&&c.roster==local.roster,"actual clamped readback differs from projected original effects");
    for(unsigned which=0;which<10;++which){auto bad=fixture();if(which==0)bad.set(0x400c8,uintptr_t(0));if(which==1)bad.set(0x60000,image+0x1742988);if(which==2)bad.set(0x60178,uintptr_t(0));if(which==3)bad.set(0x6080c,int32_t(2));if(which==4)bad.set(0x40048,int32_t(5));if(which==5)bad.set(0x40048,int32_t(-1));if(which==6)bad.set(0x40040,uintptr_t(0));if(which==7)bad.set(0x50010,uintptr_t(0x60000));if(which==8)bad.set(0x60100,std::numeric_limits<float>::infinity());if(which==9)bad.changed=true;e.clear();Census rejected;check(!run(bad,rejected,e)&&!e.empty(),"invalid membership/array/pose/concurrent census accepted");}
    auto sale=fixture();Census sold;check(run(sale,sold,e,{0,0}),e);
    check(sold.expected[0][1]==10&&sold.expected[0][2]==0&&sold.expected[1][1]==10&&sold.expected[2][1]==0&&sold.expected[3][1]==0,
        "sale zero range must retain NPC offset and clamp Monster/Zone/Quest to zero");
    sale.set(0x606f8,int32_t(10));sale.set(0x60890,int32_t(0));sale.set(0x616f8,int32_t(10));sale.set(0x61890,int32_t(0));
    sale.set(0x626f8,int32_t(0));sale.set(0x63360,int32_t(0));Census after_sale;
    check(run(sale,after_sale,e,{0,0})&&after_sale.roster==sold.roster&&after_sale.levels==sold.expected&&after_sale.levels==after_sale.expected,
        "actual sale level0 readback differs from original clamp");
    for(auto range:{std::array<int32_t,2>{0,1},std::array<int32_t,2>{-1,0},std::array<int32_t,2>{4,2},std::array<int32_t,2>{1,INT32_MAX}}){auto bad=fixture();Census rejected;check(!run(bad,rejected,e,range),"invalid range accepted");}
    std::cout<<"PASS "<<checks<<" actual actor reader/manager ownership/clamp/quest/UID-free/shape checks; game_calls=0\n";
}catch(const std::exception& x){std::cerr<<"error: "<<x.what()<<'\n';return 1;}}
