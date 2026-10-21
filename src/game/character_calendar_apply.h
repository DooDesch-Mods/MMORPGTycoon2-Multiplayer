#pragma once
#include "character_calendar_state.h"
namespace mt2mp::game::character_calendar {
inline bool same_owners(const State& local,const State& target,std::string& error){
    if(local.definitions.size()!=target.definitions.size()||local.zones.size()!=target.zones.size()||local.demand_order!=target.demand_order)
        return fail(error,"Character calendar current definition/Zone/DemandManager membership needs its original ordered lifecycle");
    for(size_t i=0;i<local.definitions.size();++i){const auto& a=local.definitions[i];const auto& b=target.definitions[i];
        if(a.kind!=b.kind||a.id!=b.id||a.deleted!=b.deleted||a.statistics.has_value()!=b.statistics.has_value())
            return fail(error,"Character calendar original global definition lifetime differs");
        for(size_t index=0;index<2;++index){
            if(a.demands[index].has_value()!=b.demands[index].has_value()||
               (a.demands[index]&&a.demands[index]->name!=b.demands[index]->name))
                return fail(error,"Character calendar registered embedded Demand owner/name differs");
            if(a.statistics&&a.statistics->durations[index].limit!=b.statistics->durations[index].limit)
                return fail(error,"Character calendar original duration-history policy differs");
        }
    }
    for(size_t i=0;i<local.zones.size();++i){const auto& a=local.zones[i];const auto& b=target.zones[i];
        if(a.key!=b.key||a.scene!=b.scene||a.type!=b.type||a.constructed!=b.constructed)
            return fail(error,"Zone calendar original canonical scene/type/construction lifetime differs");
    }
    return true;
}
template<class Port>bool project(Port& port,const State& target,Receipt& receipt,std::string& error){
    receipt={};State before;
    const auto failed=[&]{receipt.cause=error.empty()?"Character calendar projection rejected without cause":error;
        receipt.unknown=receipt.attempted;error=receipt.cause;return false;};
    if(!valid(target,error)||!port.sample(before,error)||!valid(before,error)||!same_owners(before,target,error)||!port.preflight(target,error))return failed();
    for(size_t i=0;i<before.definitions.size();++i){auto& local=before.definitions[i];const auto& wanted=target.definitions[i];
        if(local.statistics&&local.statistics!=wanted.statistics){
            if(!port.statistics(i,*local.statistics,*wanted.statistics,receipt,error))return failed();local.statistics=wanted.statistics;
        }
        for(size_t index=0;index<2;++index)if(local.demands[index]&&local.demands[index]!=wanted.demands[index]){
            if(!port.demand(i,index,*local.demands[index],*wanted.demands[index],receipt,error))return failed();local.demands[index]=wanted.demands[index];
        }
    }
    for(size_t i=0;i<before.zones.size();++i)if(before.zones[i]!=target.zones[i]){
        if(!port.zone(i,before.zones[i],target.zones[i],receipt,error))return failed();before.zones[i]=target.zones[i];
    }
    State after;if(!port.sample(after,error)||after!=target){if(error.empty())error="Character/Zone complete native projection readback differs from Host";return failed();}
    receipt.confirmed=true;error.clear();return true;
}
}
