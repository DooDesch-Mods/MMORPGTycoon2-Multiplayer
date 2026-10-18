#include "scenario_vip_authority/alert_lifecycle.h"
#ifdef _WIN32
#include "scenario_vip_authority/alert_cleanup_invoke.h"
#endif
#include <iostream>
#include <stdexcept>
namespace c=mt2mp::game::scenario_vip_authority::client_alert;
namespace v=c::vip;
unsigned checks=0;
void need(bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);}
struct Fake:c::Port {
    c::Owner o; c::Subject s;unsigned creates=0,shows=0,hides=0,deletes=0,owner_reads=0;
    bool alive=false,shown=false,subject_alive=true,foreign=false,post_fail=false;std::string fail_call;
    Fake(){o.image=1;o.mode=20;o.window=30;o.generation=1;s.id=151;s.toon=40;s.uid=50;s.name="Streamer";}
    bool owner(const c::Owner* expected,bool,c::Owner& out,std::string& e)override{
        ++owner_reads;if(foreign||(post_fail&&creates)){e="actual owner differs";return false;}
        out=o;if(expected&&*expected!=o){e="retained owner differs";return false;}return true;}
    bool subject(const c::Owner&,int32_t id,const std::string& name,c::Subject& out,std::string& e)override{
        if(!subject_alive||id!=s.id||name!=s.name){e="actual live Subject missing";return false;}out=s;return true;}
    bool inspect(const c::Owner&,uintptr_t alert,const c::Subject&,bool live,int expected,uintptr_t& selector,std::string& e)override{
        if(!alive||alert!=60||(live&&!subject_alive)||(expected>=0&&shown!=bool(expected))){e="actual Alert readback differs";return false;}
        selector=shown?70:0;return true;}
    bool absent(const c::Owner&,uintptr_t,uintptr_t,std::string& e)override{if(shown){e="actual selector remains";return false;}return true;}
    bool create(const c::Owner&,const c::Subject&,c::Call& call,std::string& e)override{
        ++creates;call.attempted=true;alive=true;call.result=60;
        if(fail_call=="create"){e="original Create failed";return false;}call.returned=true;return true;}
    bool show(const c::Owner&,uintptr_t,bool value,c::Call& call,std::string& e)override{
        if(value)++shows;else ++hides;call.attempted=true;shown=value;
        if(fail_call==(value?"show":"hide")){e="original Show failed";return false;}call.returned=true;return true;}
    bool destroy(const c::Owner&,uintptr_t,c::Call& call,std::string& e)override{
        ++deletes;call.attempted=true;alive=false;
        if(fail_call=="destroy"){e="original D0 failed";return false;}call.returned=true;return true;}
};
v::Origin origin(){return {1,2,3,"11111111-1111-1111-1111-111111111111"};}
v::Transition arrival(){return {1,1,151,"Streamer",true};}
v::Transition retirement(){return {2,1,151,"Streamer",false};}
#ifdef _WIN32
namespace hook=c::cleanup_hook;
unsigned before_calls=0,original_calls=0,after_calls=0;uintptr_t actual_mode=0;bool abnormal=false,cleanup_failed=false;
void before(uintptr_t mode)noexcept{++before_calls;actual_mode=mode;}
void after(uintptr_t mode,bool failed)noexcept{++after_calls;actual_mode=mode;abnormal=failed;}
void original(uintptr_t mode){++original_calls;actual_mode=mode;}
void cpp_error(uintptr_t){++original_calls;throw std::runtime_error("original C++ error");}
void seh_error(uintptr_t){++original_calls;RaiseException(0xe0123456,0,0,nullptr);}
uint32_t seh_run(){
    __try{hook::invoke(&seh_error,{&before,&after},25);}
    __except(EXCEPTION_EXECUTE_HANDLER){return GetExceptionCode();}return 0;
}
#endif
int main(){try{
    std::string e;const auto o=origin();const auto a=arrival(),r=retirement();
    {Fake p;c::Lifecycle l;need(l.bind(o,e),"bind");need(l.preflight(p,o,{a},e)&&p.creates==0,"preflight pure");
     need(l.apply(p,o,{a},e)&&p.creates==1&&p.shows==1&&l.owned()==1,"original create/show once");
     need(l.result().attempted&&l.result().confirmed&&!l.result().unknown,"success effect receipt");
     need(l.apply(p,o,{a},e)&&p.creates==1&&!l.result().attempted&&l.last().duplicate,"duplicate no effect");
     need(l.apply(p,o,{},e)&&l.result().confirmed&&!l.result().attempted&&l.result().rows.empty(),"empty batch confirms no effect");
     need(l.apply(p,o,{r},e)&&p.hides==1&&p.deletes==1&&!l.owned(),"original retirement once");
     need(l.apply(p,o,{r},e)&&p.deletes==1&&!l.result().attempted,"duplicate retirement no delete");
     need(l.cleanup(p,20,e),"empty cleanup");l.after_deinit(20);need(l.cleanup(p,20,e),"retired empty cleanup safe");
     auto next=o;next.generation=4;need(l.bind(next,e),"new Host Origin after confirmed cleanup");}
    {Fake p;c::Lifecycle l;need(l.bind(o,e)&&l.apply(p,o,{a,r},e),"expired pair accepted");
     need(!p.creates&&!p.shows&&!p.hides&&!p.deletes&&!l.result().attempted,"expired pair never invokes native");
     need(l.result().rows.size()==2&&l.result().rows[0].expired_before_presentation&&l.result().rows[1].expired_before_presentation,"both transitions retained truthful absent");}
    {Fake p;c::Lifecycle l;need(l.bind(o,e)&&l.apply(p,o,{a},e),"arrival before original Toon retirement");
     p.shown=false;p.subject_alive=false;
     need(l.apply(p,o,{r},e)&&p.hides==1&&p.deletes==1,"already hidden weak-cleared Alert deleted once");}
    {Fake p;c::Lifecycle l;need(l.bind(o,e)&&l.apply(p,o,{a},e),"cleanup setup");
     need(l.cleanup(p,20,e)&&p.hides==1&&p.deletes==1,"pre-Deinit hide then delete");
     l.after_deinit(20);need(!l.apply(p,o,{a},e)&&p.creates==1,"no creation after retired owner");}
    for(const auto* fail:{"create","show","hide","destroy"}){Fake p;c::Lifecycle l;need(l.bind(o,e),"failure bind");
        const bool retiring=std::string(fail)=="hide"||std::string(fail)=="destroy";
        if(retiring)need(l.apply(p,o,{a},e),"retirement setup");
        p.fail_call=fail;need(!l.apply(p,o,retiring?std::vector{r}:std::vector{a},e),"native failure visible");
        need(l.unknown()&&l.result().unknown&&l.result().attempted&&!e.empty(),"partial failure sticky Unknown");
        const auto calls=p.creates+p.shows+p.hides+p.deletes;
        need(!l.apply(p,o,{a},e)&&!l.cleanup(p,20,e)&&calls==p.creates+p.shows+p.hides+p.deletes,"Unknown never retries");
        l.after_deinit(20);need(!l.cleanup(p,20,e)&&calls==p.creates+p.shows+p.hides+p.deletes,"no late destructor");}
    {Fake p;c::Lifecycle l;need(l.bind(o,e),"owner failure bind");p.foreign=true;
     need(!l.apply(p,o,{a},e)&&!l.result().attempted&&!l.unknown()&&!p.creates,"preflight rejects foreign owner before effect");}
    {Fake p;c::Lifecycle l;need(l.bind(o,e),"Subject failure bind");p.subject_alive=false;
     need(!l.apply(p,o,{a},e)&&!p.creates&&!l.unknown(),"active arrival without live Toon rejects");}
    {Fake p;c::Lifecycle l;need(l.bind(o,e),"post-read bind");p.post_fail=true;
     need(!l.apply(p,o,{a},e)&&l.unknown()&&p.creates==1&&!p.shows,"post-constructor owner failure never shows");}
    {Fake p;c::Lifecycle l;need(l.bind(o,e)&&l.apply(p,o,{a},e),"conflict bind");auto bad=a;bad.name="Other";
     need(!l.apply(p,o,{bad},e)&&p.creates==1,"conflicting duplicate rejects");
     auto foreign=o;foreign.generation++;
     need(!l.apply(p,foreign,{r},e)&&!p.deletes,"foreign source generation rejects");
     need(!l.bind(foreign,e),"cannot rebind live allocation");}
    {Fake p;c::Lifecycle l;need(l.bind(o,e)&&l.apply(p,o,{a},e),"unexpected deinit setup");l.after_deinit(20);
     need(l.unknown()&&!l.cleanup(p,20,e)&&!p.deletes,"unexpected retired allocation retained as Unknown");}
#ifdef _WIN32
    cleanup_failed=true;hook::invoke(&original,{&before,&after},25);
    need(cleanup_failed&&before_calls==1&&original_calls==1&&after_calls==1&&!abnormal&&actual_mode==25,"cleanup failure cannot cancel original Deinit");
    try{hook::invoke(&cpp_error,{&before,&after},25);need(false,"C++ original should throw");}
    catch(const std::runtime_error& x){need(std::string(x.what())=="original C++ error","original C++ exception preserved");}
    need(original_calls==2&&after_calls==2&&abnormal,"abnormal C++ exit retires owner once");
    need(seh_run()==0xe0123456&&original_calls==3&&after_calls==3&&abnormal,"original SEH unchanged and after once");
#endif
    std::cout<<checks<<" VIP lifecycle checks passed\n";return 0;
}catch(const std::exception& x){std::cerr<<"failure after "<<checks<<": "<<x.what()<<'\n';return 1;}}
