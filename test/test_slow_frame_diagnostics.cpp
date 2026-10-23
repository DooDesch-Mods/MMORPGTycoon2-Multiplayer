#include "mt2mp/core/slow_frame_diagnostics.h"
#include <iostream>
#include <stdexcept>
namespace s=mt2mp::slow_frame;
int main(){try{
    unsigned checks=0;auto need=[&](bool value,const char* cause){++checks;if(!value)throw std::runtime_error(cause);};
    s::Counters before{},after{};before[0]=90;after[0]=140;before[1]=10;after[1]=2;
    auto value=s::delta(7,100'000'000,before,after);
    need(value.frame==7&&value.elapsed_ns==100'000'000,"sample identity lost");
    need(value.work[0]==50&&value.work[1]==0,"counter delta/reset incorrect");
    value.work[2]=80;value.work[s::outer_count+3]=400;
    need(s::dominant(value,0,s::outer_count)==2,"outer attribution includes nested work");
    need(s::dominant(value,s::outer_count,s::counter_count)==s::outer_count+3,"nested attribution missing");
    s::Tracker tracker;auto healthy=value;healthy.elapsed_ns=s::threshold_ns-1;
    need(!tracker.observe(1,healthy),"healthy frame logged");
    auto report=tracker.observe(2,value);
    need(report&&report->count==1&&report->first.frame==7,"first stall not immediate");
    value.frame=8;value.elapsed_ns=200'000'000;value.work[2]=160;
    need(!tracker.observe(3,value),"repeat bypassed rate limit");
    value.frame=9;value.elapsed_ns=300'000'000;value.work[2]=260;
    need(!tracker.observe(4,value),"worst bypassed rate limit");
    value.frame=10;value.elapsed_ns=150'000'000;
    need(!tracker.observe(5,value),"last repeat bypassed rate limit");
    need(!tracker.observe(s::interval_ns+1,healthy),"window ended early");
    report=tracker.observe(s::interval_ns+2,healthy);
    need(report&&report->count==3&&report->first.frame==8&&report->worst.frame==9,"first/worst/count not retained through recovery");
    need(report->worst.work[2]==260,"worst attribution replaced by later frame");
    need(!tracker.observe(s::interval_ns*2+2,healthy),"empty window logged");
    value.elapsed_ns=s::threshold_ns;
    need(bool(tracker.observe(s::interval_ns*2+3,value)),"new stall after quiet interval not immediate");
    std::cout<<checks<<" slow-frame checks passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
