#include "marketing_native_probe/contract.h"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace mt2mp::game::marketing_native_probe;
namespace {
int checks=0;
void check(bool v,const char* name){++checks;if(!v)throw std::runtime_error(name);}
class Bytes final:public Memory {
public:
    std::map<uintptr_t,unsigned char> data;
    bool read(uintptr_t p,void* out,size_t n,std::string& e)override{
        auto* dest=static_cast<unsigned char*>(out);for(size_t i=0;i<n;++i){auto f=data.find(p+i);if(f==data.end()){e="fixture byte missing";return false;}dest[i]=f->second;}return true;
    }
    template<class T>void put(uintptr_t p,const T& value){const auto* bytes=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof value;++i)data[p+i]=bytes[i];}
    void str(uintptr_t p,const std::string& text){put(p,p+0x80);put(p+8,uint64_t(text.size()));for(size_t i=0;i<=text.size();++i)data[p+0x80+i]=static_cast<unsigned char>(text.c_str()[i]);
        put(p+0x20,uintptr_t(0));put(p+0x28,uintptr_t(0));put(p+0x30,uintptr_t(0));}
    void arr(uintptr_t p,uintptr_t storage,const std::vector<uintptr_t>& values){put(p,storage);put(p+8,int32_t(values.size()));put(p+12,int32_t(4));for(size_t i=0;i<values.size();++i)put(storage+8*i,values[i]);}
};
constexpr uintptr_t image=0x1000000,state=0x4000000,mode=0x4001000,marketing=0x4002000,hud=0x4003000,window=0x4004000,
    clock_object=0x4005000,launch=0x4006000,view=0x4007000;
void fixture(Bytes& m){
    m.put(state+0x1d0,marketing);m.put(image+0x13df2c0,marketing);m.put(marketing,image+0x16dfa60);
    m.put(mode+0x128,hud);m.put(image+0x13df8b0,hud);m.put(hud,image+0x1742750);m.put(hud+0xb8,window);
    m.put(window,image+0x16fa4d0);m.put(window+0x508,image+0x16e1770);m.put(image+0x13df9a0,clock_object);
    m.arr(marketing+0x40,0,{});m.arr(marketing+0x70,0x4100000,{launch});m.arr(window+0x510,0x4101000,{view});m.arr(window+0x300,0x4102000,{view});
    m.put(window+0x138,400.f);m.put(window+0x13c,45.f);m.put(marketing+0x98,int32_t(6));m.put(marketing+0x9c,int32_t(0));
    m.put(launch,image+0x16e3d50);m.str(launch+0x18,"");
    const int32_t fields[]={6,1,3,0,0,0};for(size_t i=0;i<6;++i)m.put(launch+0x50+4*i,fields[i]);m.put(launch+0x68,uint8_t(1));
    m.put(view,image+0x16dd8a0);m.put(view+0x320,uintptr_t(0));m.put(view+0x328,launch);m.put(view+0x278,window);m.put(view+0x280,window);
}
}
int main(int argc,char** argv){
    try{
        check(argc==2,"artifact argument");std::ifstream file(argv[1]);Json j;file>>j;Payload payload;std::string e;
        check(parse_payload(j,payload,e),"exact retained typed payload accepted");check(payload.award.size()==7&&payload.campaign.size()==8,"exact record counts");
        for(const auto key:{"award","campaign","launch"}){
            auto bad=j;bad[key][1]["tokens"][0]["type"]=1;check(!parse_payload(bad,payload,e),"label/string token distinction");
            bad=j;bad[key][0]["tokens"].push_back({{"type",4},{"bits",1},{"text",""}});check(!parse_payload(bad,payload,e),"unexpected root tokens");
            bad=j;bad[key].push_back(bad[key][1]);check(!parse_payload(bad,payload,e),"duplicate or reference field rejected");
            bad=j;bad[key][1]["parent"]=1;check(!parse_payload(bad,payload,e),"self-parent rejected");
        }
        for(const auto key:{"source_sha256","baseline_sha256","schema","day"}){auto bad=j;bad[key]=nullptr;check(!parse_payload(bad,payload,e),"artifact identity rejected");}
        auto bad=j;bad["extra"]=true;check(!parse_payload(bad,payload,e),"extra artifact field rejected");
        check(current_campaign(2,2,5,1,e),"current exact campaign");check(current_campaign(6,2,5,1,e),"last active day");
        check(!current_campaign(7,2,5,1,e),"expiry rejected");check(!current_campaign(1,2,5,1,e),"future clamp rejected");
        check(!current_campaign(2,2,0,1,e),"zero duration rejected");check(!current_campaign(2,2,-1,1,e),"negative duration rejected");
        check(!current_campaign(INT32_MAX,INT32_MAX,1,1,e),"signed endpoint overflow rejected");check(!current_campaign(2,2,5,0,e),"inactive cache rejected");
        const ClockSample clock_before{2,11819,0.f,1.f,true};ClockSample clock_actual=clock_before;Json clock_diagnostic;
        check(same_clock(clock_before,clock_actual,ClockPhase::Before,clock_diagnostic,e),"before scope preserves exact baseline pause scale");
        clock_actual.pause_scale=0.f;
        check(same_clock(clock_before,clock_actual,ClockPhase::Held,clock_diagnostic,e),"SaveScope Increment zero pause scale accepted");
        check(same_clock(clock_before,clock_actual,ClockPhase::Closed,clock_diagnostic,e),"immediate Decrement leaves zero until Mode Update");
        check(!same_clock(clock_before,clock_actual,ClockPhase::Before,clock_diagnostic,e),"pre-scope unexpected pause scale rejected");
        check(clock_diagnostic["failed_field"]=="pause_scale"&&clock_diagnostic["measured"]["pause_scale"]==0&&clock_diagnostic["required"]["pause_scale"]==1,"pause failure reports measured and required");
        for(const auto phase:{ClockPhase::Held,ClockPhase::Closed}){
            clock_actual=clock_before;check(!same_clock(clock_before,clock_actual,phase,clock_diagnostic,e),"post-entry nonzero pause scale rejected");
            clock_actual.pause_scale=0;clock_actual.day=3;check(!same_clock(clock_before,clock_actual,phase,clock_diagnostic,e)&&clock_diagnostic["failed_field"]=="day","day changed rejected");
            clock_actual.day=2;clock_actual.seconds=11820;check(!same_clock(clock_before,clock_actual,phase,clock_diagnostic,e)&&e.find("measured=11820 required=11819")!=std::string::npos,"seconds change diagnosed exactly");
            clock_actual.seconds=11819;clock_actual.time_scale=1;check(!same_clock(clock_before,clock_actual,phase,clock_diagnostic,e),"actual speed change rejected");
            clock_actual.time_scale=0;clock_actual.paused=false;check(!same_clock(clock_before,clock_actual,phase,clock_diagnostic,e),"unpaused state rejected");
        }
        const ClockSample already_paused{2,11819,0.f,0.f,true};
        check(same_clock(already_paused,already_paused,ClockPhase::Held,clock_diagnostic,e),"already zero pause scale remains valid");
        clock_actual=already_paused;clock_actual.pause_scale=-0.f;
        check(same_clock(already_paused,clock_actual,ClockPhase::Held,clock_diagnostic,e),"native float equality preserves signed zero");
        Bytes m;fixture(m);Snapshot before;
        check(read_snapshot(m,image,state,mode,before,e),"native reader baseline");check(baseline(before,e),"baseline current pane");
        auto corrupt=m;corrupt.put(image+0x13df2c0,marketing+8);Snapshot s;check(!read_snapshot(corrupt,image,state,mode,s,e),"Marketing owner mismatch");
        corrupt=m;corrupt.put(launch,image+0x16e3d58);check(!read_snapshot(corrupt,image,state,mode,s,e),"wrong Campaign class");
        corrupt=m;corrupt.put(launch+0x18+0x28,uintptr_t(8));check(!read_snapshot(corrupt,image,state,mode,s,e),"unexpected LocString argument rejected");
        corrupt=m;corrupt.put(marketing+0x78,int32_t(-1));check(!read_snapshot(corrupt,image,state,mode,s,e),"negative array count");
        corrupt=m;corrupt.put(launch+0x68,uint8_t(2));check(!read_snapshot(corrupt,image,state,mode,s,e),"invalid cached active bool");
        corrupt=m;corrupt.put(view+0x328,launch+0x1000);check(read_snapshot(corrupt,image,state,mode,s,e)&&!baseline(s,e),"stale pane rejected without following target");
        auto after=before;after.awards={0x5000000};after.campaigns.push_back(0x5001000);after.views={1,2,3};after.panes=after.views;after.values=expected_values(true);
        after.bindings=Json::array({{{"award",after.awards[0]},{"campaign",0}},{{"award",0},{"campaign",launch}},{{"award",0},{"campaign",after.campaigns[1]}}});
        check(completed(before,after,after.awards[0],after.campaigns[1],e),"desired exact owners and bindings accepted");
        s=after;s.campaigns[0]+=8;check(!completed(before,s,after.awards[0],after.campaigns[1],e),"Launch replacement rejected");
        s=after;s.values["positiveBuzz"]=7;check(!completed(before,s,after.awards[0],after.campaigns[1],e),"unrelated Marketing mutation rejected");
        s=after;s.bindings[2]["campaign"]=launch;check(!completed(before,s,after.awards[0],after.campaigns[1],e),"duplicate pane rejected");
        s=after;s.clock+=8;check(!completed(before,s,after.awards[0],after.campaigns[1],e),"Clock owner replacement rejected");
        std::cout<<checks<<" Marketing contract assertions passed\n";return 0;
    }catch(const std::exception& ex){std::cerr<<"Marketing assertion failed: "<<ex.what()<<'\n';return 1;}
}
