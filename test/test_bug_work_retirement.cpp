#include "developer_staff/job_census.h"
#include "bug_authority/job_calls.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
namespace mt2mp::game::bug_authority {
using developer_staff::Capture;
namespace census=developer_staff::job_census;
struct Memory {
    std::map<uintptr_t,std::vector<unsigned char>> blocks;
    uintptr_t unreadable=0;
    bool copy(uintptr_t p,void* out,size_t size,std::string& e){
        if(p==unreadable){e="original selected memory failure";return false;}
        auto it=blocks.upper_bound(p);if(it==blocks.begin()){e="original unmapped memory";return false;}--it;
        if(p-it->first>it->second.size()||size>it->second.size()-(p-it->first)){e="original unreadable memory";return false;}
        std::memcpy(out,it->second.data()+p-it->first,size);return true;
    }
    template<class T>void put(uintptr_t p,const T& value){auto it=blocks.upper_bound(p);--it;std::memcpy(it->second.data()+p-it->first,&value,sizeof value);}
};
struct Fixture {
    Memory memory;Capture current,original;
    bool owner_failure=false,capture_failure=false,destructor_failure=false,shift_failure=false;
    int captures=0,shifts=0,stores=0,destructors=0,fresh_calls=0;
    Fixture(){
        current.image=0x10000000;current.thread=1;current.pin.origin.state=reinterpret_cast<void*>(0x1000);
        for(const auto& [p,n]:std::map<uintptr_t,size_t>{{0x1000,0x200},{0x2000,0x200},{0x3000,32},{0x4000,0xc8},{0x5000,0xc8},{current.image+0x13df230,8}})memory.blocks[p].resize(n);
        memory.put<uintptr_t>(0x11a8,0x2000);memory.put<uintptr_t>(current.image+0x13df230,0x2000);
        memory.put<uintptr_t>(0x2000,current.image+0x16db570);memory.put<uintptr_t>(0x2018,current.image+0x16ef400);
        memory.put<uintptr_t>(0x2020,0x3000);memory.put<int32_t>(0x2028,2);memory.put<int32_t>(0x202c,4);
        memory.put<uintptr_t>(0x3000,0x4000);memory.put<uintptr_t>(0x3008,0x5000);
        memory.put<uintptr_t>(0x4000,current.image+0x17427a0);memory.put<uintptr_t>(0x5000,current.image+0x17427a0);
        memory.put<int32_t>(0x5018,7);memory.put<uint8_t>(0x50c4,1);
        for(int n=0;n<50;++n){const uintptr_t p=0x10000+uintptr_t(n)*0x1000;memory.blocks[p].resize(0x500);
            memory.put<uintptr_t>(p,current.image+0x17422e0);memory.put<int32_t>(p+0x4c0,n);
            current.local.developers.push_back(p);developer_staff::Row row;row.id=n;row.slot=n;current.state.rows.push_back(row);}
        original=current;original.state.rows[0].state=1;original.state.rows[0].job_link=true;
    }
};
Fixture* fixture=nullptr;
Fixture& runtime(){return *fixture;}
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
census::Read reader(){return [](uintptr_t p,void* out,size_t n,std::string& e){return runtime().memory.copy(p,out,n,e);};}
bool fresh(const Capture& capture,std::string& e){
    ++runtime().fresh_calls;
    if(capture.state!=runtime().original.state)return fail(e,"original pre-transition guard reference lost");
    return !runtime().owner_failure||fail(e,"original Origin ledger Clock catalogue changed");
}
bool capture_current(Capture& out,std::string& e){++runtime().captures;
    if(runtime().capture_failure)return fail(e,"original current capture failed");out=runtime().current;return true;}
bool returns(const JobCall& call,bool& attempted,const char* name,std::string& e){
    attempted|=call.attempted;
    if(call.returned&&!call.exception&&!call.cleanup_exception)return true;
    e=std::string("original ")+name+" failed";return false;
}
uint32_t work_unary(uintptr_t address,uintptr_t pointer,JobCall* call){
    auto& f=runtime();if(address!=f.current.image+0xc2e810||pointer!=0x4000)throw std::runtime_error("wrong selected destructor");
    ++f.destructors;call->attempted=true;
    if(f.destructor_failure){call->exception=17;return 17;}
    f.memory.blocks.erase(pointer);call->returned=true;return 0;
}
}
namespace mt2mp::game::developer_staff::construction {
uint32_t shift(uintptr_t image,uintptr_t destination,uintptr_t source,size_t bytes,Call* out){
    auto& f=bug_authority::runtime();if(image!=f.current.image||destination!=0x3000||source!=0x3008||bytes!=8)throw std::runtime_error("wrong selected queue shift");
    ++f.shifts;out->attempted=true;if(f.shift_failure){out->exception=18;return 18;}
    uintptr_t value=0;std::string e;if(!f.memory.copy(source,&value,8,e))throw std::runtime_error(e);
    f.memory.put(destination,value);out->returned=true;return 0;
}
}
namespace mt2mp::game::live_host_state {
bool compare_fixed_i32(uintptr_t p,int32_t before,int32_t after,std::string& e){
    auto& f=bug_authority::runtime();++f.stores;int32_t actual=0;if(!f.memory.copy(p,&actual,4,e))return false;
    if(p!=0x2028||actual!=before||before!=2||after!=1){e="original compare/store mismatch";return false;}
    f.memory.put(p,after);return true;
}
}
namespace mt2mp::game::bug_authority {
#include "bug_work_retirement_actual.inc"
}
int main(){using namespace mt2mp::game::bug_authority;
    auto check=[](bool ok,const char* cause){if(!ok)throw std::runtime_error(cause);};int cases=0;
    try{
        {Fixture f;fixture=&f;census::Census queue;std::string e;
            check(!census::read(reader(),f.original,queue,e)&&e=="Job census Developer changed from authorized pool","old pool no longer fails exact original guard");
            check(census::read(reader(),f.current,queue,e),"current post-transition pool rejected");++cases;}
        for(int scenario=0;scenario<8;++scenario){Fixture f;fixture=&f;std::string e;bool attempted=false;
            if(scenario==1)f.owner_failure=true;
            if(scenario==2)f.capture_failure=true;
            if(scenario==3)f.memory.put<uintptr_t>(0x40b8,0x10000);
            if(scenario==4){f.memory.put<uintptr_t>(0x104c8,0x4000);f.current.state.rows[0].job_link=true;}
            if(scenario==5)f.memory.unreadable=0x3000;
            if(scenario==6)f.shift_failure=true;
            if(scenario==7)f.destructor_failure=true;
            const bool ok=erase_aborted(f.original,0x4000,attempted,e);
            if(scenario==0){check(ok&&attempted&&f.shifts==1&&f.stores==1&&f.destructors==1,"current selected retirement failed");
                census::Census after;check(census::read(reader(),f.current,after,e)&&after.jobs.size()==1&&after.jobs[0].pointer==0x5000&&after.developers.size()==50,"surviving graph differs");
                check(f.fresh_calls>=2&&!f.memory.blocks.contains(0x4000),"original owner guard/destructor missing");}
            else{check(!ok,"failure accepted");
                if(scenario<=5)check(!attempted&&!f.shifts&&!f.stores&&!f.destructors,"pre-effect failure mutated");
                if(scenario==1)check(e=="original Origin ledger Clock catalogue changed"&&f.captures==0,"original owner guard weakened");
                if(scenario==2)check(e=="original current capture failed","capture cause lost");
                if(scenario==3||scenario==4)check(e.find("references")!=std::string::npos||e.find("reference")!=std::string::npos,"selected link guard lost");
                if(scenario==5)check(e=="original selected memory failure","read cause lost");
                if(scenario==6)check(attempted&&f.shifts==1&&!f.stores&&!f.destructors,"partial shift not retained");
                if(scenario==7)check(attempted&&f.stores==1&&f.destructors==1,"partial destructor not retained");}
            ++cases;
        }
        std::cout<<"PASS "<<cases<<" actual selected retirement cases\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
