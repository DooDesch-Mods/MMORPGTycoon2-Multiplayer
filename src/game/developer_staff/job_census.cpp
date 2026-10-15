#include "job_census.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace mt2mp::game::developer_staff::job_census {
namespace {
bool fail(std::string& e,const std::string& cause){e=cause;return false;}
template<class T>bool at(const Read& r,uintptr_t p,size_t off,T& v,std::string& e){
    if(!p||p>UINTPTR_MAX-off||p+off>UINTPTR_MAX-sizeof(T))return fail(e,"Job census local address overflow");
    return r(p+off,&v,sizeof(T),e);
}
bool type(const Read& r,uintptr_t p,uintptr_t expected,std::string& e){
    uintptr_t ap=0;return at(r,p,0,ap,e)&&(ap==expected||fail(e,"Job census exact class addresspoint differs"));
}
bool observe(const Read& r,const Capture& c,Census& out,std::string& e){
    out={};const auto image=c.image;uintptr_t singleton=0;int32_t count=0;
    if(!at(r,reinterpret_cast<uintptr_t>(c.pin.origin.state),0x1a8,out.queue,e)||!at(r,image,0x13df230,singleton,e))return false;
    if(out.queue!=singleton)return fail(e,"Job census GameState queue and singleton differ");
    if(!type(r,out.queue,image+0x16db570,e)||!type(r,out.queue+0x18,image+0x16ef400,e)||
       !at(r,out.queue,0x20,out.data,e)||!at(r,out.queue,0x28,count,e)||!at(r,out.queue,0x2c,out.capacity,e))return false;
    constexpr int32_t budget=65536;
    if(count<0||count>budget||out.capacity<count||out.capacity>budget||(out.capacity&&!out.data))
        return fail(e,"Job census diagnostic budget/header differs count="+std::to_string(count)+" capacity="+std::to_string(out.capacity)+" required_max=65536");
    std::set<uintptr_t> pointers;
    for(int32_t n=0;n<count;++n){
        Job j;
        if(!at(r,out.data,size_t(n)*8,j.pointer,e))return false;
        if(!pointers.insert(j.pointer).second)return fail(e,"Job census duplicate queue receiver");
        if(!type(r,j.pointer,image+0x17427a0,e)||!at(r,j.pointer,0x18,j.type,e)||!at(r,j.pointer,0x1c,j.position,e)||
           !at(r,j.pointer,0x28,j.destination,e)||!at(r,j.pointer,0x38,j.bug.pointer,e)||!at(r,j.pointer,0x68,j.building,e)||
           !at(r,j.pointer,0x70,j.building_proxy,e)||!at(r,j.pointer,0xa0,j.monster,e)||!at(r,j.pointer,0xa8,j.monster_proxy,e)||
           !at(r,j.pointer,0xb0,j.region,e)||!at(r,j.pointer,0xb8,j.developer,e)||!at(r,j.pointer,0xc0,j.raw_id,e)||!at(r,j.pointer,0xc4,j.valid,e))return false;
        if(j.type<0||j.type>8||j.valid>1)return fail(e,"Job census native type/valid differs");
        for(size_t k=0;k<3;++k)if(!std::isfinite(j.position[k])||!std::isfinite(j.destination[k]))return fail(e,"Job census position is not finite");
        if(j.bug.pointer){auto& b=j.bug;
            if(!type(r,b.pointer,image+0x1742200,e)||!at(r,b.pointer,0x28,b.runtime_id,e)||!at(r,b.pointer,0x2c,b.type,e)||
               !at(r,b.pointer,0x30,b.created,e)||!at(r,b.pointer,0x34,b.discovered,e)||!at(r,b.pointer,0x38,b.toons,e)||
               !at(r,b.pointer,0x3c,b.reproducibility,e)||!at(r,b.pointer,0x40,b.variant,e)||!at(r,b.pointer,0x44,b.known,e)||
               !at(r,b.pointer,0x45,b.manual,e)||!at(r,b.pointer,0x46,b.debugged,e)||!at(r,b.pointer,0x48,b.job,e)||!at(r,b.pointer,0x50,b.anchor,e))return false;
            if(b.known>1||b.manual>1||b.debugged>1||!std::isfinite(b.reproducibility))return fail(e,"Job census Bug scalar shape differs");
        }
        out.jobs.push_back(j);
    }
    for(size_t n=0;n<50;++n){
        const auto& expected=c.state.rows[n];Developer d;d.pointer=c.local.developers[n];int32_t state=0;uint8_t employed=0,active=0,onmap=0;
        if(!type(r,d.pointer,image+0x17422e0,e)||!at(r,d.pointer,0x4c0,d.id,e)||!at(r,d.pointer,0x4c4,state,e)||
           !at(r,d.pointer,0x4c8,d.job,e)||!at(r,d.pointer,0x4d0,d.carried,e)||!at(r,d.pointer,0x4d8,employed,e)||
           !at(r,d.pointer,0x4d9,active,e)||!at(r,d.pointer,0x368,onmap,e))return false;
        if(expected.slot!=int32_t(n)||expected.id!=d.id||expected.state!=state||employed!=uint8_t(expected.employed)||
           active!=uint8_t(expected.active)||onmap!=uint8_t(expected.on_map)||bool(d.job)!=expected.job_link)return fail(e,"Job census Developer changed from authorized pool");
        for(size_t k=0;k<out.jobs.size();++k)if(out.jobs[k].pointer==d.job){d.job_index=int32_t(k);d.reciprocal=out.jobs[k].developer==d.pointer;}
        out.developers.push_back(d);
    }
    for(auto& j:out.jobs)for(const auto& d:out.developers)if(j.developer==d.pointer){j.developer_id=d.id;j.reciprocal=d.job==j.pointer;}
    return true;
}
}
bool read(const Read& r,const Capture& c,Census& out,std::string& e){
    out={};e.clear();
    if(!r||!c.image||c.image>UINTPTR_MAX-0x1800000||!c.thread||!c.pin.origin.state||c.state.rows.size()!=50||c.local.developers.size()!=50)
        return fail(e,"Job census requires authorized exact-image fixed50 Capture");
    Census a,b;if(!observe(r,c,a,e)||!observe(r,c,b,e))return false;
    if(a!=b)return fail(e,"Job census changed between two observations");
    out=std::move(a);return true;
}
nlohmann::json encode(const Census& c){
    auto jobs=nlohmann::json::array(),developers=nlohmann::json::array();
    for(size_t n=0;n<c.jobs.size();++n){const auto& j=c.jobs[n];const auto& b=j.bug;
        jobs.push_back({{"queue_index",n},{"type",j.type},{"raw_id_untrusted",j.raw_id},{"valid",j.valid!=0},{"position",j.position},{"destination",j.destination},
            {"developer_present",j.developer!=0},{"developer_id",j.developer_id},{"developer_reciprocal",j.reciprocal},
            {"building_present",j.building!=0},{"building_proxy_present",j.building_proxy!=0},{"monster_present",j.monster!=0},
            {"monster_proxy_present",j.monster_proxy!=0},{"region_present",j.region!=0},
            {"bug",{{"present",b.pointer!=0},{"job_reciprocal",b.pointer&&b.job==j.pointer},{"anchor_present",b.anchor!=0},
                {"runtime_id_local",b.runtime_id},{"type",b.type},{"created",b.created},{"discovered",b.discovered},{"toons",b.toons},
                {"reproducibility",b.reproducibility},{"variant",b.variant},{"known",b.known},{"manual",b.manual},{"debugged",b.debugged}}}});
    }
    for(const auto& d:c.developers)developers.push_back({{"id",d.id},{"job_present",d.job!=0},{"job_index",d.job_index},{"reciprocal",d.reciprocal},{"carried_present",d.carried!=0}});
    return {{"schema",1},{"diagnostic_only",true},{"identity_proven",false},{"queue_count",c.jobs.size()},{"queue_capacity",c.capacity},{"jobs",jobs},{"developers",developers}};
}
}
