#include "typed_action/region_property_action.h"
#include "region_ui_readback.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
namespace action=mt2mp::action_session;
namespace ui_readback=mt2mp::game::ui_region_runtime::ui_readback;
using json=nlohmann::json;
struct Memory {
    std::map<uintptr_t,std::vector<unsigned char>> blocks;
    void add(uintptr_t p,size_t n){blocks[p].resize(n);}
    bool copy(uintptr_t p,void* out,size_t n,std::string& e){
        for(auto& [start,bytes]:blocks)if(p>=start&&p-start<=bytes.size()&&n<=bytes.size()-(p-start)){
            std::memcpy(out,bytes.data()+p-start,n);return true;}
        e="fixture unreadable address";return false;
    }
    template<class T>void set(uintptr_t p,T value){
        for(auto& [start,bytes]:blocks)if(p>=start&&p-start<=bytes.size()&&sizeof(T)<=bytes.size()-(p-start)){
            std::memcpy(bytes.data()+p-start,&value,sizeof value);return;}
        throw std::runtime_error("fixture write outside memory");
    }
};
struct Runtime {Memory memory;uintptr_t image=0x140000000;bool admitted=true;unsigned captures=0;action::RegionPropertyIntent captured;};
struct LocalView {struct {uintptr_t selection=0x4000;} world;};
bool fail(std::string& e,const std::string& text){e=text;return false;}
template<class T>bool at(Runtime& r,uintptr_t p,size_t off,T& out,std::string& e){return r.memory.copy(p+off,&out,sizeof out,e);}
bool property_admit(Runtime& r,std::string& e){return r.admitted||fail(e,"Controller pending");}
bool identity(Runtime&,int32_t id,uintptr_t region,LocalView&,std::string& e){return (id==67&&region==0x2000)||fail(e,"current Region differs");}
bool property_capture(Runtime& r,const action::RegionPropertyIntent& value,uintptr_t region,std::string& e){
    LocalView v;if(!property_admit(r,e)||!identity(r,value.id,region,v,e))return false;r.captured=value;++r.captures;return true;
}
uint32_t label_is(uintptr_t,void* record,const char* label,bool& match){match=std::strcmp(static_cast<const char*>(record),label)==0;return 0;}
uint32_t appearance_picker(uintptr_t,uintptr_t picker,uintptr_t& value){value=picker+0x12a8;return 0;}
uint32_t appearance_slider(uintptr_t,uintptr_t,int32_t,float& value){value=2;return 0;}
uint32_t combo_get(uintptr_t,uintptr_t combo,uintptr_t& value){value=combo;return 0;}
uint32_t appearance_fx(uintptr_t,uintptr_t,int32_t& value,bool& converted){value=6;converted=true;return 0;}
#include "appearance_read_actual.inc"
#include "appearance_capture_actual.inc"
void need(bool ok,const std::string& e){if(!ok)throw std::runtime_error(e);}
Runtime fixture(){
    Runtime r;auto& m=r.memory;m.add(0x1000,0x700);m.add(0x2000,0x1000);m.add(0x4000,0x100);m.add(0x5000,0x40);
    m.add(r.image+0x13df9d0,8);m.set(r.image+0x13df9d0,uintptr_t{0});
    m.set(0x1000,r.image+0x171fad0);m.set(0x145f,uint8_t{1});m.set(0x14e8,uintptr_t{0x2000});
    m.set(0x2370,uintptr_t{0x6000});m.set(0x2834,int32_t{67});
    m.set(0x4030,int32_t{1});m.set(0x4080,uintptr_t{0x6000});m.set(0x40b8,uintptr_t{0x5000});
    m.set(0x5000,r.image+0x17317e0);m.set(0x5010,uintptr_t{0x1000});
    const size_t offsets[]{0x5a0,0x5a8,0x5c0,0x5c8,0x5d0,0x5d8,0x5f0};
    for(size_t i=0;i<7;++i){const uintptr_t p=0x10000+i*0x2000;m.add(p,0x1300);m.set(p,r.image+0x16fae80);m.set(0x1000+offsets[i],p);
        m.set(p+0x12a8,std::array<float,4>{float(i)/10,0.25f,0.5f,1});}
    for(size_t i=0;i<2;++i){const uintptr_t p=0x30000+i*0x1000;m.add(p,0x500);m.set(p,r.image+0x1787a30);m.set(0x1000+0x5b0+i*8,p);}
    m.add(0x40000,0x500);m.set(0x40000,r.image+0x16d9b30);m.set(0x15e8,uintptr_t{0x40000});return r;
}
int main(){try{unsigned count=0;std::string e;
    const char* commands[]{"ColorTint","ColorLight","ColorAmbient","SkyColor","FXChange","ColorRegionFX"};
    for(unsigned i=0;i<6;++i){auto r=fixture();bool handled=false;
        need(appearance_capture(r,reinterpret_cast<void*>(0x1000),const_cast<char*>(commands[i]),handled,e),e);
        need(handled&&r.captures==1&&uint8_t(r.captured.kind)==5+i,"actual command kind/capture differs");
        if(i==1||i==2)need(r.captured.appearance_value[3]==1&&r.captured.appearance_value[1]==0.5f,"original light scale/alpha differs");
        if(i==3)need(r.captured.appearance_value[0]==0.5f&&r.captured.appearance_value[8]==0.3f&&r.captured.appearance_value[4]<0,"original sky coefficient order differs");
        if(i==4)need(r.captured.appearance_value[0]==6,"native particle conversion differs");++count;
        r=fixture();r.admitted=false;need(!appearance_capture(r,reinterpret_cast<void*>(0x1000),const_cast<char*>(commands[i]),handled,e)&&!r.captures,"pending capture accepted");++count;
        r=fixture();r.memory.set(0x145f,uint8_t{0});need(!appearance_capture(r,reinterpret_cast<void*>(0x1000),const_cast<char*>(commands[i]),handled,e)&&!r.captures,"hidden UI capture accepted");++count;
    }
    auto r=fixture();bool handled=true;need(appearance_capture(r,reinterpret_cast<void*>(0x1000),const_cast<char*>("Focus"),handled,e)&&!handled&&!r.captures,"unrelated command swallowed");++count;
    action::RegionAppearance a;need(appearance_read_native(r,0x2000,a,e),e);++count;
    r.memory.set(0x2840,std::array<float,4>{0,0.25f,0.5f,1});
    need(appearance_capture(r,reinterpret_cast<void*>(0x1000),const_cast<char*>("ColorTint"),handled,e)&&handled&&!r.captures,
        "unchanged picker notification submitted a world mutation");++count;
    r.memory.set(0x28fc,uint8_t{1});need(!appearance_read_native(r,0x2000,a,e),"inconsistent particle enable accepted");++count;
    std::cout<<count<<" actual appearance capture/read cases passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
