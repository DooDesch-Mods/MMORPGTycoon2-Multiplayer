#include "coop_cursor/native_read.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>
using namespace mt2mp::game::coop_cursor;
unsigned checks=0;
void require(bool condition,const char* cause){++checks;if(!condition)throw std::runtime_error(cause);}
int main()try{
    constexpr uintptr_t image=0x140000000,cursor=0x200000000;
    std::map<uintptr_t,std::vector<uint8_t>> fields;
    const auto put=[&]<class T>(uintptr_t address,const T& value){auto& bytes=fields[address];bytes.resize(sizeof(T));std::memcpy(bytes.data(),&value,sizeof(T));};
    put(image+0x13df9d0,cursor);put(cursor,image+0x1786dd0);
    put(cursor+0x180,uintptr_t(0));put(cursor+0x257,uint8_t(1));
    const std::array<float,3> world{8500,70,9000};put(cursor+0x1e4,world);
    const std::array<float,2> screen{100,200};put(cursor+0x1f0,screen);
    unsigned cursor_reads=0;bool change_lifetime=false;
    Copy copy=[&](uintptr_t address,void* output,size_t count,std::string& error){
        auto found=fields.find(address);
        if(found==fields.end()||found->second.size()!=count){error="unreadable native field";return false;}
        std::memcpy(output,found->second.data(),count);
        if(address==image+0x13df9d0&&++cursor_reads==2&&change_lifetime){const uintptr_t replacement=cursor+0x1000;std::memcpy(output,&replacement,sizeof replacement);}
        return true;
    };
    Sample sample;std::string error;
    require(read_native_sample(image,copy,sample,error)&&sample.position==world,"reader did not use actual XYZ world pick");
    put(cursor+0x180,uintptr_t(0x300000000));
    require(read_native_sample(image,copy,sample,error)&&!sample.position,"UI-captured cursor was published into world");
    put(cursor+0x180,uintptr_t(0));put(cursor+0x257,uint8_t(0));
    require(read_native_sample(image,copy,sample,error)&&!sample.position,"disabled cursor was published");
    put(cursor+0x257,uint8_t(2));
    require(!read_native_sample(image,copy,sample,error)&&error.find("enabled")!=std::string::npos,"invalid native boolean admitted");
    put(cursor+0x257,uint8_t(1));put(cursor,image+0x1786de0);
    require(!read_native_sample(image,copy,sample,error)&&error.find("class")!=std::string::npos,"wrong native class admitted");
    put(cursor,image+0x1786dd0);put(cursor+0x1e4,std::array<float,3>{1,std::numeric_limits<float>::quiet_NaN(),3});
    require(!read_native_sample(image,copy,sample,error)&&!sample.position,"nonfinite native position escaped");
    put(cursor+0x1e4,world);fields.erase(cursor+0x257);
    require(!read_native_sample(image,copy,sample,error)&&error=="unreadable native field","original memory failure lost");
    put(cursor+0x257,uint8_t(1));change_lifetime=true;cursor_reads=0;
    require(!read_native_sample(image,copy,sample,error)&&error.find("lifetime")!=std::string::npos,"changed native cursor lifetime admitted");
    require(!read_native_sample(UINTPTR_MAX,copy,sample,error),"image overflow admitted");
    require(!read_native_sample(0,copy,sample,error),"unresolved image admitted");
    std::cout<<checks<<" actual native cursor field/lifetime checks passed\n";return 0;
}catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}
