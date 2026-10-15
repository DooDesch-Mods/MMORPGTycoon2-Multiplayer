#include "vendor/nlohmann/json.hpp"
#include <cstdint>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <iostream>
#include <stdexcept>
using json=nlohmann::json;
constexpr uintptr_t fixture_image=0x140000000,fixture_mode=0x1000,fixture_brush=0x2000,theme=0x3000,alternate_window=0x4000;
struct Memory {std::map<uintptr_t,uint64_t> fields;std::map<uintptr_t,std::string> names;uintptr_t fail_address=0;bool change=false;int mode_reads=0;};
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>bool field(Memory& m,uintptr_t p,size_t o,T& v,std::string& e){
    const auto a=p+o;if(a==m.fail_address)return fail(e,"fixture read failed");
    const auto it=m.fields.find(a);if(it==m.fields.end())return fail(e,"fixture address missing");v=static_cast<T>(it->second);
    if(a==fixture_mode+0xa0&&++m.mode_reads==2&&m.change)++v;return true;
}
bool name(Memory& m,uintptr_t p,std::string& v,std::string& e){const auto it=m.names.find(p);if(it==m.names.end())return fail(e,"fixture name missing");v=it->second;return true;}
#include "zone_theme_diagnostic.inc"
Memory fixture(){Memory m;m.fields={{fixture_brush+0x5d8,theme},{theme,fixture_image+0x17321a0},{alternate_window,fixture_image+0x17321a0},
    {fixture_mode+0xa0,0x5000},{fixture_mode+0xa8,1},{fixture_mode+0xac,1},{0x5000,alternate_window},
    {fixture_image+0x1a3c478,0x6000},{fixture_image+0x1a3c480,2},{fixture_image+0x1a3c484,2},{0x6000,theme},{0x6008,alternate_window}};
    m.names={{theme+0x1b0,"character_theme"},{alternate_window+0x1b0,"monster_placement"}};return m;}
void check(bool v,const char* reason){if(!v)throw std::runtime_error(reason);}
int main(){try{
    {auto m=fixture();json out;std::string e;check(theme_ownership(m,fixture_image,fixture_mode,fixture_brush,out,e),"disjoint Mode/Registry observation failed");check(out["mode"]["brush_members"]==0&&out["mode"]["named_members"]==0&&out["registry"]["brush_members"]==1&&out["registry"]["named_members"]==1,"owner distinction lost");check(out["brush_window"]["class_rva"]==0x17321a0&&out["brush_window"]["name"]=="character_theme","normalized identity differs");}
    {auto m=fixture();m.fields[0x5000]=theme;json out;std::string e;check(theme_ownership(m,fixture_image,fixture_mode,fixture_brush,out,e)&&out["mode"]["brush_members"]==1,"Mode membership lost");}
    {auto m=fixture();m.change=true;json out;std::string e;check(!theme_ownership(m,fixture_image,fixture_mode,fixture_brush,out,e)&&e=="Zone diagnostic window array changed"&&!out["confirmed"].get<bool>(),"changed owner accepted");}
    {auto m=fixture();m.fields[0x6008]=theme;json out;std::string e;check(!theme_ownership(m,fixture_image,fixture_mode,fixture_brush,out,e)&&e=="Zone diagnostic window membership differs","duplicate registry member accepted");}
    {auto m=fixture();m.fail_address=alternate_window;json out;std::string e;check(!theme_ownership(m,fixture_image,fixture_mode,fixture_brush,out,e)&&e=="fixture read failed","original read cause lost");}
    std::cout<<"PASS 5 actual diagnostic body cases\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
