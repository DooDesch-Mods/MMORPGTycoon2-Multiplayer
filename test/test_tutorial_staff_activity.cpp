#include "tutorial_startingpoint_fixture/activity_format_probe.h"
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <vector>
using json=nlohmann::json;
static unsigned cases=0;
void check(bool good,const char* why){if(!good)throw std::runtime_error(why);}
struct Calls {
    std::vector<std::string> log;std::string failure,throw_at,key;bool mismatch=false;
    json project={{"key","{startingpoint}"},{"args",json::array()}};
    bool step(const char* s,std::string& e){log.push_back(s);if(throw_at==s)throw std::runtime_error(s);if(failure==s){e=std::string("original ")+s;return false;}return true;}
    bool name(std::string& e){return step("name",e);}
    bool read_name(json& out,std::string& e){out=project;return step("read_name",e);}
    bool arg(std::string& e){return step("arg",e);}
    bool format(const char* k,std::string& e){key=k;return step("format",e);}
    bool read_format(json& out,std::string& e){out={{"key",mismatch?"wrong":key},{"args",json::array({{{"name","techname"},{"type",0},{"value",project}}})}};return step("read_format",e);}
    bool release_format(std::string& e){return step("release_format",e);}
    bool release_arg(std::string& e){return step("release_arg",e);}
    bool release_name(std::string& e){return step("release_name",e);}
};
void format_tests(){
    namespace a=mt2mp::game::tutorial_startingpoint_fixture::activity_probe;
    Calls c;json out;std::string e;
    check(a::format(c,out,e)&&out.size()==2,"fixed labels");
    check(c.log==std::vector<std::string>{"name","read_name","arg","format","read_format","release_format","format","read_format","release_format","release_arg","release_name"},"native lifecycle order");++cases;
    for(const char* fail_at:{"name","read_name","arg","format","read_format","release_format","release_arg","release_name"}){
        c={};e.clear();c.failure=fail_at;check(!a::format(c,out,e)&&e.find(std::string("original ")+fail_at)!=std::string::npos,"cause preserved");
        const auto count=[&](const char* x){return std::count(c.log.begin(),c.log.end(),x);};
        check(count("release_name")==(c.failure=="name"?0:1),"name released once if constructed");
        check(count("release_arg")<=1,"arg not retried");
        if(c.failure=="release_format")check(count("release_format")==1,"failed destructor not retried");++cases;
    }
    c={};e.clear();c.mismatch=true;check(!a::format(c,out,e)&&e.find("result differs")!=std::string::npos&&c.log.back()=="release_name","mismatched result cleanup");++cases;
    c={};e.clear();c.throw_at="read_format";check(!a::format(c,out,e)&&e.find("exception: read_format")!=std::string::npos&&c.log.back()=="release_name","exception cleanup");++cases;
}
namespace memory_tests {
std::map<uintptr_t,unsigned char> bytes;std::map<uintptr_t,std::string> strings;
bool fail(std::string& e,const std::string& why){e=why;return false;}
template<class T>void put(uintptr_t p,T v){const auto* data=reinterpret_cast<const unsigned char*>(&v);for(size_t i=0;i<sizeof(v);++i)bytes[p+i]=data[i];}
template<class T>bool at(uintptr_t p,size_t off,T& v,std::string& e){auto* data=reinterpret_cast<unsigned char*>(&v);for(size_t i=0;i<sizeof(v);++i){auto it=bytes.find(p+off+i);if(it==bytes.end())return fail(e,"unmapped read");data[i]=it->second;}return true;}
bool text(uintptr_t p,size_t off,std::string& v,std::string& e){auto it=strings.find(p+off);if(it==strings.end())return fail(e,"unmapped text");v=it->second;return true;}
bool type(uintptr_t p,uintptr_t expected,std::string& e){uintptr_t value=0;return at(p,0,value,e)&&(value==expected||fail(e,"wrong type"));}
void loc(uintptr_t p,const char* key,uintptr_t first=0,uintptr_t last=0,uintptr_t end=0){strings[p]=key;put(p+0x20,first);put(p+0x28,last);put(p+0x30,end);}
#include "loc.inc"
void parser_tests(){
    json out;std::string e;loc(0x1000,"{dev_state_tech_develop}",0x2000,0x2068,0x2068);strings[0x2000]="techname";put<int32_t>(0x2060,0);loc(0x2020,"{startingpoint}");
    check(activity_loc(0x1000,out,e)&&out["args"][0]["value"]["key"]=="{startingpoint}","actual native nested stride");++cases;
    const auto saved=bytes;
    for(auto [offset,value]:std::vector<std::pair<uintptr_t,uintptr_t>>{{0x20,0},{0x28,0x2067},{0x30,0x2000},{0x28,0x2208},{0x30,0x23a8}}){bytes=saved;put(0x1000+offset,value);e.clear();check(!activity_loc(0x1000,out,e),"invalid bounds");++cases;}
    bytes=saved;put<int32_t>(0x2060,3);e.clear();check(!activity_loc(0x1000,out,e)&&e.find("type outside")!=std::string::npos,"unknown arg type");++cases;
    bytes=saved;put<int32_t>(0x2060,2);put<float>(0x205c,std::numeric_limits<float>::infinity());put<int32_t>(0x2058,2);e.clear();check(!activity_loc(0x1000,out,e)&&e.find("not finite")!=std::string::npos,"nonfinite arg");++cases;
    bytes=saved;strings[0x2000]="";e.clear();check(!activity_loc(0x1000,out,e)&&e.find("name absent")!=std::string::npos,"missing argument name");++cases;
    strings[0x2000]="techname";e.clear();check(!activity_loc(0x1000,out,e,3)&&e.find("nesting")!=std::string::npos,"depth guard");++cases;
}
struct ActivityDeveloper{uintptr_t address=0;int32_t id=0,job=0;uint8_t employed=0,on_map=0;};
namespace tutorial_development {struct StartingpointSnapshot{uintptr_t image=0,lounge=0;};}
int32_t native_tech=1,native_active=1;unsigned native_count_calls=0;
unsigned activity_count(uintptr_t,uintptr_t rva,uintptr_t,int32_t& out){++native_count_calls;out=rva==0x25ed30?native_tech:native_active;return 0;}
bool activity_error(unsigned code,const char*,std::string& e){return !code||fail(e,"native count exception");}
#include "managers.inc"
void manager_tests(){
    bytes.clear();std::string e;tutorial_development::StartingpointSnapshot s{0x100000,0x1000};
    for(uintptr_t side=0;side<2;++side){uintptr_t owner=0x3000+side*0x1000;put(s.lounge+0x98+side*8,owner);put(owner,s.image+0x16e47f0);put(owner+0x38,uintptr_t(0x5000+side*0x1000));put<int32_t>(owner+0x40,side==0?1:0);put<int32_t>(owner+0x44,4);}
    put<uintptr_t>(0x5000,0x7000);std::vector<ActivityDeveloper> ds{{0x7000,0,6,1,0}};ActivityManagers out;
    check(activity_managers(s,ds,out,e)&&out.active_tech==1&&native_count_calls==2,"owned indoor native count");++cases;
    const auto saved=bytes;put<uintptr_t>(0x5000,0xdead);native_count_calls=0;e.clear();check(!activity_managers(s,ds,out,e)&&native_count_calls==0&&e.find("membership")!=std::string::npos,"foreign dev rejected before native getter");++cases;
    bytes=saved;put<uintptr_t>(0x3000,s.image+1);e.clear();check(!activity_managers(s,ds,out,e)&&e=="wrong type","manager vtable");++cases;
    bytes=saved;put<int32_t>(0x3044,100);e.clear();check(!activity_managers(s,ds,out,e)&&e.find("bounds")!=std::string::npos,"manager capacity");++cases;
    bytes=saved;native_tech=0;e.clear();check(!activity_managers(s,ds,out,e)&&e.find("active counts")!=std::string::npos,"actual getter disagreement");++cases;
}
}
namespace receipt_tests {
struct Request{std::string operation="staff_activity_state",token="fixed";};
struct ActivityNativeCalls{std::array<unsigned,3> attempted{},returned{};unsigned exception=0;bool unknown=false;std::string cause;};
ActivityNativeCalls activity_calls;bool activity_faulted=false;std::string activity_fault_cause;unsigned samples=0;bool native_fault=false;
bool fail(std::string& e,const std::string& why){e=why;return false;}
bool activity_sample(const Request&,json&,std::string& e){++samples;activity_calls.attempted={3,3,5};activity_calls.returned={3,3,5};if(native_fault){activity_calls.returned[1]=2;activity_calls.exception=0xc0000005;activity_calls.unknown=true;return fail(e,"original LocArg exception");}return true;}
#include "receipt.inc"
void run(){Request r;json out;std::string e;check(staff_activity_state(r,out,e)&&out["attempted"]==false&&out["native_calls"]["formatter"]["attempted"]==3&&out["native_calls"]["destructor"]["confirmed"]==true,"separate native receipts");++cases;
 native_fault=true;check(!staff_activity_state(r,out,e)&&out["unknown"]==true&&out["retry_blocked"]==true&&e=="original LocArg exception","partial object unknown");++cases;
 auto before=samples;check(!staff_activity_state(r,out,e)&&samples==before&&out["attempted"]==false&&out["unknown"]==true&&e.find("original LocArg exception")!=std::string::npos,"no automatic retry");++cases;
}
}
int main(){try{format_tests();memory_tests::parser_tests();memory_tests::manager_tests();receipt_tests::run();std::cout<<cases<<" actual-body checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
