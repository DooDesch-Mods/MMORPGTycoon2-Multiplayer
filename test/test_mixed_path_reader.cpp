#include "mixed_path_reader.h"
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
using namespace mt2mp::game::mixed_path_native;
void need(bool condition,const std::string& cause){if(!condition)throw std::runtime_error(cause);}
struct Memory {
    std::map<uintptr_t,uint8_t> bytes;
    uintptr_t next=0xa00000;
    template<class T>void put(uintptr_t address,const T& value){
        const auto* data=reinterpret_cast<const uint8_t*>(&value);
        for(size_t i=0;i<sizeof(T);++i)bytes[address+i]=data[i];
    }
    template<class T>void field(uintptr_t address,size_t offset,const T& value){put(address+offset,value);}
    template<class T>void vector(uintptr_t address,size_t offset,const std::vector<T>& value){
        const uintptr_t data=value.empty()?0:next;
        const int32_t count=int32_t(value.size());
        field(address,offset,data);field(address,offset+8,count);field(address,offset+12,count);
        for(size_t i=0;i<value.size();++i)put(data+i*sizeof(T),value[i]);
        next+=0x1000;
    }
    bool copy(uintptr_t address,void* target,size_t size,std::string& error){
        auto* out=reinterpret_cast<uint8_t*>(target);
        for(size_t i=0;i<size;++i){const auto found=bytes.find(address+i);
            if(found==bytes.end()){error="fixture unreadable native address="+std::to_string(address+i);return false;}
            out[i]=found->second;}
        return true;
    }
};
int main(){try{
    constexpr uintptr_t image=0x10000000,map=0x100000,root=map+0x1c0;
    constexpr uintptr_t r1=0x200000,r2=0x210000,m1=0x300000,m2=0x310000;
    constexpr uintptr_t wall=0x500000,pass=0x510000,n1=0x600000,n2=0x610000,n3=0x620000;
    Memory memory;std::string error;unsigned cases=0;
    memory.put(map,image+0x1742860);memory.put(root,image+0x174ad20);memory.field(root,0x48,int32_t(0));
    memory.put(map+0x230,image+0x16f04e0);memory.vector(map+0x230,8,std::vector<uintptr_t>{r1,r2});
    memory.vector(map,0x358,std::vector<uintptr_t>{r1});
    auto region=[&](uintptr_t address,int32_t id,uintptr_t manager){
        memory.put(address,image+0x1787930);memory.field(address,0x70,root);
        memory.field(address,0x834,id);memory.field(address,0x9a0,manager);
        memory.field(manager,0x6a0,address);
        for(int type=0;type<11;++type){
            memory.vector(manager,0x28+size_t(type)*0x30,std::vector<uintptr_t>{});
            memory.vector(manager,0x370+size_t(type)*0x48,std::vector<uintptr_t>{});}
        for(int type=0;type<6;++type)memory.vector(manager,0x238+size_t(type)*0x30,std::vector<uintptr_t>{});
    };
    region(r1,1,m1);region(r2,2,m2);
    memory.vector(m1,0x28+4*0x30,std::vector<uintptr_t>{wall});
    memory.vector(m1,0x28+5*0x30,std::vector<uintptr_t>{pass});
    memory.vector(m2,0x370+5*0x48,std::vector<uintptr_t>{pass});
    memory.vector(m1,0x238,std::vector<uintptr_t>{n1,n2});
    memory.vector(m2,0x238,std::vector<uintptr_t>{n3});
    auto path=[&](uintptr_t address,uint64_t uid,int32_t type,uintptr_t second,uintptr_t start,uintptr_t end,
                  std::vector<Point> points){
        memory.put(address,image+0x1743f10);memory.field(address,0x18,uid);memory.field(address,0x21c,type);
        memory.field(address,0x178,r1);memory.field(address,0x2f8,second);
        memory.field(address,0x268,start);memory.field(address,0x270,end);
        memory.field(address,0x46e,uint8_t(0));
        memory.vector(address,0x298,points);memory.vector(address,0x2b0,std::vector<float>{0,1});
    };
    path(wall,11,4,0,n1,n2,{{0,0,0},{1,0,0}});
    path(pass,12,5,r2,n2,n3,{{1,0,0},{2,0,0}});
    auto node=[&](uintptr_t address,uint64_t uid,uintptr_t owner,Point position,std::vector<uintptr_t> edges){
        memory.put(address,image+0x1700a10);memory.field(address,0x18,uid);
        memory.field(address,0x298,int32_t(0));memory.field(address,0x178,owner);
        memory.field(address,0x110,position);memory.vector(address,0x2c8,edges);
    };
    node(n1,21,r1,{0,0,0},{wall});node(n2,22,r1,{1,0,0},{wall,pass});node(n3,23,r2,{2,0,0},{pass});
    Census actual;need(read_world(memory,image,map,root,actual,error),error);++cases;
    need(actual.regions.size()==2&&actual.paths.size()==2&&actual.nodes.size()==3&&actual.remote.size()==1,
         "complete mixed Region bucket census differs");++cases;
    auto unchanged=actual;memory.vector(m2,0x370+5*0x48,std::vector<uintptr_t>{});
    need(!read_world(memory,image,map,root,actual,error)&&actual==unchanged&&
         error.find("remote")!=std::string::npos,"missing reciprocal remote membership accepted");++cases;
    memory.vector(m2,0x370+5*0x48,std::vector<uintptr_t>{pass});
    memory.vector(n2,0x2c8,std::vector<uintptr_t>{wall});
    need(!read_world(memory,image,map,root,actual,error)&&actual==unchanged&&
         error.find("reciprocal")!=std::string::npos,"missing Intersection edge accepted");++cases;
    memory.vector(n2,0x2c8,std::vector<uintptr_t>{wall,pass});
    memory.field(pass,0x2f8,r1);
    need(!read_world(memory,image,map,root,actual,error)&&actual==unchanged&&
         error.find("second Region")!=std::string::npos,"invalid second Region accepted");++cases;
    std::cout<<cases<<" complete mixed native census cases passed\n";return 0;
}catch(const std::exception& cause){std::cerr<<cause.what()<<'\n';return 1;}}
