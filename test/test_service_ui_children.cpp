#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

using DWORD = unsigned long;
struct MEMORY_BASIC_INFORMATION {void* AllocationBase; DWORD State; DWORD Type;};
constexpr DWORD MEM_COMMIT=1, MEM_IMAGE=2;
struct Runtime {uintptr_t image=0x10000000;std::map<uintptr_t,uintptr_t> values;std::map<uintptr_t,std::vector<uintptr_t>> arrays;int reads=0;};
uintptr_t allocation=0x10000000;
size_t VirtualQuery(const void*,MEMORY_BASIC_INFORMATION* p,size_t){*p={reinterpret_cast<void*>(allocation),MEM_COMMIT,MEM_IMAGE};return sizeof(*p);}
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
bool at(Runtime& r,uintptr_t base,size_t offset,uintptr_t& out,std::string& e){auto i=r.values.find(base+offset);if(i==r.values.end())return fail(e,"missing field");out=i->second;return true;}
bool at(Runtime& r,uintptr_t base,size_t offset,uint8_t& out,std::string& e){uintptr_t value=0;if(!at(r,base,offset,value,e))return false;out=static_cast<uint8_t>(value);return true;}
bool loan_ui_array(Runtime& r,uintptr_t pane,size_t offset,std::vector<uintptr_t>& out,std::string& e){++r.reads;auto i=r.arrays.find(pane+offset);if(i==r.arrays.end())return fail(e,"invalid array");out=i->second;return true;}
#include "service_ui_children.inc"
void need(bool condition,const char* cause){if(!condition)throw std::runtime_error(cause);}
int main(){try{
    Runtime r;std::string e;std::vector<uintptr_t> pending;const uintptr_t pane=0x4000,klass=0x10003000,window=0x6000,child=0x7000;
    r.values[klass+0x178]=r.image+0xc35730;
    need(service_ui_children(r,pane,klass,window,pending,e)&&r.reads==0&&pending.empty(),"leaf read View arrays");
    r.values[klass+0x178]=r.image+0x8167c0;r.arrays[pane+0x2c8]={child};r.arrays[pane+0x2f8]={};r.values[child+0x278]=pane;r.values[child+0x280]=window;
    need(service_ui_children(r,pane,klass,window,pending,e)&&r.reads==2&&pending==std::vector<uintptr_t>{child},"View children missing");
    pending.clear();r.values[child+0x280]=window+1;e.clear();need(!service_ui_children(r,pane,klass,window,pending,e)&&pending.empty(),"foreign owner accepted");
    r.values[child+0x280]=window;r.values[klass+0x178]=r.image+42;e.clear();need(!service_ui_children(r,pane,klass,window,pending,e),"unknown slot accepted");
    allocation=0x20000000;r.values[klass+0x178]=r.image+0xc35730;e.clear();need(!service_ui_children(r,pane,klass,window,pending,e),"foreign vtable accepted");
    std::vector<uint8_t> flags;r.values[child+0x4a]=1;e.clear();need(service_ui_priceable(r,{child},1,flags,e)&&flags==std::vector<uint8_t>{1},"priceable flag read differs");
    e.clear();need(!service_ui_priceable(r,{child},2,flags,e),"priceable row count mismatch accepted");r.values[child+0x4a]=2;e.clear();need(!service_ui_priceable(r,{child},1,flags,e),"invalid priceable flag accepted");
    std::cout<<"actual Service UI child traversal passed\n";return 0;
}catch(const std::exception& x){std::cerr<<x.what()<<'\n';return 1;}}
