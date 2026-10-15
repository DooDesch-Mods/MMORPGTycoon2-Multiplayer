#include "developer_staff/job_census.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
using namespace mt2mp::game::developer_staff;
namespace {
struct Fixture {
    std::map<uintptr_t,std::vector<unsigned char>> memory;
    Capture capture;int calls=0,change_on=0;
    Fixture(){
        capture.image=0x10000000;capture.thread=1;capture.pin.origin.state=reinterpret_cast<void*>(0x1000);
        block(0x1000,0x200);block(0x2000,0x200);block(0x3000,32);block(0x4000,0xc8);block(0x5000,0xc8);block(0x6000,0x58);
        block(capture.image+0x13df230,8);put<uintptr_t>(0x11a8,0x2000);put<uintptr_t>(capture.image+0x13df230,0x2000);
        put<uintptr_t>(0x2000,capture.image+0x16db570);put<uintptr_t>(0x2018,capture.image+0x16ef400);
        put<uintptr_t>(0x2020,0x3000);put<int32_t>(0x202c,4);put<uintptr_t>(0x3000,0x4000);put<uintptr_t>(0x3008,0x5000);
        put<uintptr_t>(0x4000,capture.image+0x17427a0);put<uintptr_t>(0x5000,capture.image+0x17427a0);
        put<uintptr_t>(0x6000,capture.image+0x1742200);
        for(int n=0;n<50;++n){const uintptr_t actor=0x10000+uintptr_t(n)*0x1000;block(actor,0x500);
            put<uintptr_t>(actor,capture.image+0x17422e0);put<int32_t>(actor+0x4c0,n);
            capture.local.developers.push_back(actor);Row row;row.id=n;row.slot=n;capture.state.rows.push_back(row);}
    }
    void block(uintptr_t p,size_t n){memory[p].resize(n);}
    template<class T>void put(uintptr_t p,T value){auto it=memory.upper_bound(p);--it;std::memcpy(it->second.data()+p-it->first,&value,sizeof(value));}
    bool copy(uintptr_t p,void* target,size_t bytes,std::string& error){
        if(++calls==change_on)put<int32_t>(0x40c0,99);
        auto it=memory.upper_bound(p);if(it==memory.begin()){error="fixture unmapped";return false;}--it;
        if(p-it->first>it->second.size()||bytes>it->second.size()-(p-it->first)){error="fixture unreadable";return false;}
        std::memcpy(target,it->second.data()+p-it->first,bytes);return true;
    }
    bool run(job_census::Census& out,std::string& e){calls=0;return job_census::read([&](auto p,auto t,auto b,auto& why){return copy(p,t,b,why);},capture,out,e);}
    void assigned(){put<int32_t>(0x2028,1);put<uintptr_t>(0x40b8,0x10000);put<uintptr_t>(0x104c8,0x4000);capture.state.rows[0].job_link=true;}
};
int failures=0,checks=0;
void check(bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<name<<" failed\n";}}
}
int main(){
    job_census::Census out;std::string e;
    {Fixture f;check(f.run(out,e)&&out.jobs.empty()&&out.developers.size()==50,"empty queue");}
    {Fixture f;f.assigned();check(f.run(out,e)&&out.jobs[0].developer_id==0&&out.jobs[0].reciprocal&&out.developers[0].job_index==0,"assigned reciprocity");
        const auto encoded=job_census::encode(out);check(encoded["identity_proven"]==false&&encoded["jobs"][0]["queue_index"]==0,"diagnostic identity boundary");}
    {Fixture f;f.assigned();f.put<uintptr_t>(0x104c8,0x5000);check(f.run(out,e)&&out.developers[0].job_index==-1&&!out.jobs[0].reciprocal,"dangling link visible");}
    {Fixture f;f.assigned();f.put<uintptr_t>(0x4038,0x6000);f.put<uintptr_t>(0x6048,0x4000);check(f.run(out,e)&&out.jobs[0].bug.job==0x4000,"Bug incoming link");}
    {Fixture f;f.put<uintptr_t>(0x2018,1);check(!f.run(out,e)&&e.find("addresspoint")!=std::string::npos,"wrong array class");}
    {Fixture f;f.put<uintptr_t>(f.capture.image+0x13df230,0x3000);check(!f.run(out,e)&&e.find("singleton")!=std::string::npos,"owner mismatch");}
    {Fixture f;f.assigned();f.put<int32_t>(0x2028,2);f.put<uintptr_t>(0x3008,0x4000);check(!f.run(out,e)&&e.find("duplicate")!=std::string::npos,"duplicate job");}
    {Fixture f;f.assigned();f.put<uintptr_t>(0x3000,0);check(!f.run(out,e),"null job");}
    {Fixture f;f.put<int32_t>(0x2028,-1);check(!f.run(out,e)&&e.find("header")!=std::string::npos,"negative count");}
    {Fixture f;f.put<int32_t>(0x202c,65537);check(!f.run(out,e)&&e.find("65536")!=std::string::npos,"bounded diagnostic");}
    {Fixture f;f.assigned();f.put<float>(0x401c,std::numeric_limits<float>::quiet_NaN());check(!f.run(out,e)&&e.find("finite")!=std::string::npos,"invalid position");}
    {Fixture f;f.assigned();check(f.run(out,e),"mutation setup");f.change_on=f.calls/2+10;check(!f.run(out,e)&&e.find("two observations")!=std::string::npos,"second census mutation");}
    {Fixture f;f.put<int32_t>(0x104c4,6);check(!f.run(out,e)&&e.find("authorized pool")!=std::string::npos,"pool changed");}
    {Fixture f;f.memory.erase(0x10000);check(!f.run(out,e)&&e=="fixture unreadable","original read cause");}
    {Fixture f;f.assigned();f.memory.erase(0x3000);check(!f.run(out,e)&&e=="fixture unreadable","original queue-slot read cause");}
    std::cout<<checks<<" job census checks, "<<failures<<" failures\n";return failures?1:0;
}
