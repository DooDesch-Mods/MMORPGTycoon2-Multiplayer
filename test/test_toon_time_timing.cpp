#define main existing_normalization_checks
#include "test_toon_time_fence.cpp"
#undef main
#include "live_host_state/memory_query_timing.h"
#include <functional>
#include <intrin.h>

namespace timing=mt2mp::game::live_host_state::apply_timing;
namespace live_host_state=mt2mp::game::live_host_state;
namespace fixture {
using Owner=Calls::Owner;
using Receipt=timef::Receipt;
using timef::normalize;
Calls seed,observed;
Calls* running=nullptr;
bool active=true,entered=false,inside_original=false;
unsigned forwarded=0,published=0;
Receipt last;
std::function<void()> original_effect;
std::function<bool(std::string&)> backlog_probe;
float forwarded_dt=-1;
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
uint32_t update_call(uintptr_t toon,float dt){
    inside_original=timing::totals[size_t(timing::Part::ToonNormalize)].count==0&&
        timing::totals[size_t(timing::Part::ToonOriginal)].count==0;
    std::string error;
    if(original_effect)original_effect();
    return running->Calls::original(toon,dt,error)?0:123;
}
class Native:public Calls {
public:
    explicit Native(uintptr_t){static_cast<Calls&>(*this)=seed;running=this;}
    ~Native(){observed=*this;running=nullptr;}
    bool read_backlog(uintptr_t toon,uint32_t& out,std::string& error){
        if(backlog_probe&&!backlog_probe(error))return false;
        return Calls::read_backlog(toon,out,error);
    }
#include "toon_time_original.inc"
};
struct Callbacks {
    std::function<bool()> client_active=[](){return active;};
    std::function<void(uintptr_t,const Receipt&)> publish=[](uintptr_t,const Receipt& r){++published;last=r;};
} callbacks;
void original_update(uintptr_t,float dt){++forwarded;forwarded_dt=dt;}
#include "toon_time_update.inc"
void reset(){seed={};observed={};active=true;entered=false;inside_original=false;forwarded=published=0;last={};original_effect={};backlog_probe={};timing::totals={};}
}
#include "toon_original_boundary_cases.inc"
int main(){
    if(existing_normalization_checks())return 1;
    unsigned checks=0;
    const auto need=[&](bool yes,const char* cause){++checks;if(!yes)throw std::runtime_error(cause);};
    const auto count=[](timing::Part p){return timing::totals[size_t(p)].count;};
    try{
        fixture::reset();fixture::update(1,0);
        need(fixture::last.confirmed&&fixture::observed.pins==3&&fixture::observed.clears==1&&fixture::observed.originals==1,"zero dt lost owner/CAS/original semantics");
        need(count(timing::Part::ToonNormalize)==1&&count(timing::Part::ToonOriginal)==1&&fixture::inside_original,"two nested timers not recorded once");
        need(timing::totals[size_t(timing::Part::ToonNormalize)].total_ns>=timing::totals[size_t(timing::Part::ToonOriginal)].total_ns,"original timing exceeds enclosing normalization");
        need(timing::snapshot()["toon_normalize"]["count"]==1&&timing::snapshot()["toon_original"]["count"]==1,"existing timing status omitted Toon counters");
        need(timing::take_depth==0&&timing::query_phase==timing::QueryPhase::Other,"timers changed Take/query context");
        fixture::reset();fixture::active=false;fixture::update(1,3);
        need(fixture::forwarded==1&&fixture::forwarded_dt==3&&!count(timing::Part::ToonNormalize)&&!count(timing::Part::ToonOriginal),"Host/offline forwarding instrumented or changed");
        fixture::reset();fixture::seed.owned=false;fixture::update(1,3);
        need(fixture::forwarded==1&&fixture::forwarded_dt==3&&count(timing::Part::ToonNormalize)==1&&!count(timing::Part::ToonOriginal),"unowned decision/forwarding changed");
        fixture::reset();fixture::seed.fail_pin=1;fixture::update(1,0);
        need(fixture::last.cause=="exact owner failure"&&!fixture::last.unknown&&count(timing::Part::ToonNormalize)==1&&!count(timing::Part::ToonOriginal),"owner failure timing/cause changed");
        fixture::reset();fixture::seed.original_ok=false;fixture::update(1,0);
        need(fixture::last.unknown&&fixture::last.cause=="Toon original zero-time entry exception=123"&&count(timing::Part::ToonOriginal)==1&&count(timing::Part::ToonNormalize)==1,"native failure timing/cause lost");
        fixture::reset();fixture::seed.throw_original=true;fixture::update(1,0);
        need(fixture::last.unknown&&fixture::last.cause.find("original exception")!=std::string::npos&&count(timing::Part::ToonOriginal)==1&&count(timing::Part::ToonNormalize)==1,"C++ unwind lost timing/cause");
        fixture::reset();fixture::entered=true;fixture::update(1,0);
        need(fixture::last.cause=="Toon time nested owned entry rejected"&&!count(timing::Part::ToonNormalize)&&!count(timing::Part::ToonOriginal),"nested rejection changed");
        boundary_cases(need);
        std::cout<<checks<<" actual Toon hook timing/boundary checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
