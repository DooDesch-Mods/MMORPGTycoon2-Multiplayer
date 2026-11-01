#include "intervention_actor_runtime.h"
#include "intervention_census.h"
#include <Windows.h>

namespace actor_empty_read_fixture {
using Census=mt2mp::game::interventions::Census;
std::array<Census,2> samples;
unsigned census_calls=0,code_calls=0,memory_calls=0,native_calls=0;
unsigned fail_census=0;
bool fail_code=false;
void reset(){samples={};census_calls=code_calls=memory_calls=native_calls=fail_census=0;fail_code=false;}
}
namespace mt2mp::game::interventions {
bool actor_census(const action_session::Context&,const action_session::Binding&,Census& out,std::string& error){
    using namespace actor_empty_read_fixture;++census_calls;
    if(census_calls==fail_census){error="fixture original census failure";return false;}
    out=samples[(census_calls-1)%2];return true;
}
}
namespace mt2mp::game::root_saved_building {
bool bytes(const void*,size_t,void*,size_t,std::string& error){++actor_empty_read_fixture::memory_calls;error="fixture unexpected memory read";return false;}
}
namespace mt2mp::game::interventions::actors::native {
uint32_t maximum(uintptr_t,uintptr_t,int32_t,float&)noexcept{++actor_empty_read_fixture::native_calls;return 1;}
uint32_t clip(uintptr_t,const Render&,int32_t,Clip&)noexcept{++actor_empty_read_fixture::native_calls;return 1;}
uint32_t registered(uintptr_t,uint64_t,uintptr_t&)noexcept{++actor_empty_read_fixture::native_calls;return 1;}
}
namespace mt2mp::game::interventions::actors {
namespace {
bool fail(std::string& error,const std::string& cause){error=cause;return false;}
struct Read {template<class T>bool operator()(uintptr_t object,size_t offset,T& value,std::string& error){return root_saved_building::bytes(reinterpret_cast<void*>(object),offset,&value,sizeof value,error);}};
bool code(uintptr_t,std::string& error){++actor_empty_read_fixture::code_calls;return !actor_empty_read_fixture::fail_code||fail(error,"fixture original code proof failed");}
}
#include "actor_empty_read_actual.inc"
}
namespace actor_empty_read_fixture {
void run(){
    namespace actors=mt2mp::game::interventions::actors;
    mt2mp::action_session::Context context;mt2mp::action_session::Binding binding;
    actors::State values;actors::Locals locals;actors::EmptyRead before,after;std::string error;
    reset();need(actors::read_current(context,binding,values,locals,error,&before)&&before&&values.empty()&&locals.empty()&&census_calls==1&&code_calls==1,"empty witness requires one complete census and fresh code proof");
    need(actors::read_current(context,binding,values,locals,error,&after)&&after&&census_calls==2&&code_calls==2&&actors::same_empty_read(before,after,error),"empty outer pair must reread complete census and compare");
    need(memory_calls==0&&native_calls==0,"empty paired capture called an Actor native leaf");
    reset();need(actors::read_current(context,binding,values,locals,error)&&census_calls==2&&code_calls==1,"standalone empty read lost its original paired census");
    for(unsigned drift=0;drift<8;++drift){reset();error.clear();
        need(actors::read_current(context,binding,values,locals,error,&before),"seed physical empty witness");
        if(drift==0)samples[1].bosses.world.world=reinterpret_cast<void*>(123);
        if(drift==1)++samples[1].bosses.world.world_epoch;
        if(drift==2)samples[1].owners.emplace(123,"actual-owner");
        if(drift==3)samples[1].zones.push_back({"zone",123,456});
        if(drift==4)samples[1].users.rows.emplace_back();
        if(drift==5)samples[1].bosses.pools.emplace_back();
        if(drift==6)samples[1].bosses.live.emplace_back();
        if(drift==7)samples[1].bosses.world.sha256="different";
        need(actors::read_current(context,binding,values,locals,error,&after)&&!actors::same_empty_read(before,after,error)&&error=="Actor Frame complete canonical owner census changed during read","empty outer pair admitted physical owner drift");
    }
    reset();samples[0].actors.push_back({{mt2mp::game::interventions::Family::Toon,"subscriber:1"},123,456});samples[1]=samples[0];
    need(actors::read_current(context,binding,values,locals,error,&before)&&!before&&census_calls==2&&values.empty(),"nonempty census including Toon must use original complete paired path");
    reset();samples[0].actors.push_back({{mt2mp::game::interventions::Family::Toon,"subscriber:1"},123,456});samples[1]=samples[0];++samples[1].actors[0].uid;
    need(!actors::read_current(context,binding,values,locals,error,&before)&&!before&&census_calls==2&&error=="Actor Frame complete canonical owner census changed during read","nonempty fallback lost original complete census comparison");
    for(unsigned failure=0;failure<2;++failure){reset();before=after;error.clear();fail_census=failure==0?1:0;fail_code=failure==1;
        need(!actors::read_current(context,binding,values,locals,error,&before)&&!before&&values.empty()&&locals.empty()&&error==(failure==0?"fixture original census failure":"fixture original code proof failed"),"failed empty sample published a witness or lost original cause");}
    need(!actors::same_empty_read(before,after,error),"absent empty witness passed paired admission");
}
}
