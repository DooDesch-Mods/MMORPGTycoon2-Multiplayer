#pragma once
#include "shared_state.h"
#include "scalar.h"
namespace mt2mp::game::developer_staff {
struct FieldChange {size_t slot=0;scalar::Field field=scalar::Field::State;int32_t value=0;bool operator==(const FieldChange&)const=default;};
inline bool frame_plan(const PoolState& before,const PoolState& target,std::vector<FieldChange>& changes,std::string& error){
    changes.clear();if(!valid(before,error)||!valid(target,error))return false;
    if(before.construction||target.construction){error="Developer construction frame requires bound native construction route";return false;}
    if(before.count!=target.count||before.indoor!=target.indoor||before.outdoor!=target.outdoor){error="Developer frame count/ordered native manager membership differs; ordered Hire/Fire or original map closure required";return false;}
    for(size_t slot=0;slot<50;++slot){const auto& a=before.rows[slot];const auto& b=target.rows[slot];
        if(a.id!=b.id||a.employed!=b.employed||a.position!=b.position){error="Developer frame fixed native ID/employment/pose differs";return false;}
        if(a.job_link||b.job_link||a.on_map||b.on_map){error="Developer frame actual JobQueue/outdoor closure is not yet bound";return false;}
        const auto indoor=[](int32_t state){return state==0||state==6||state==7;};
        if(!indoor(a.state)||!indoor(b.state)){error="Developer frame native non-indoor transition requires original movement closure";return false;}
        if(a.state!=b.state)changes.push_back({slot,scalar::Field::State,b.state});
        if(a.active!=b.active)changes.push_back({slot,scalar::Field::Active,int32_t(b.active)});
    }
    error.clear();return true;
}
template<class Calls>
bool apply_frame(const PoolState& before,const PoolState& target,Calls& calls,bool& attempted,bool& unknown,std::string& error){
    attempted=unknown=false;std::vector<FieldChange> changes;
    try{
        if(!frame_plan(before,target,changes,error)||!calls.pin(before,error))return false;
        if(changes.empty()){error.clear();return true;}
        for(const auto& change:changes){attempted=true;
            if(!calls.scalar(change,error)){unknown=true;return false;}}
        PoolState observed;
        if(!calls.read(observed,error)){unknown=true;return false;}
        if(observed!=target){unknown=true;error="Developer frame original property/full50 readback differs; no replay";return false;}
        if(!calls.invalidate_display(error)){unknown=true;return false;}
        error.clear();return true;
    }catch(const std::exception& cause){unknown=attempted;error=std::string("Developer frame original exception: ")+cause.what();return false;}
    catch(...){unknown=attempted;error="Developer frame original nonstandard exception";return false;}
}
}
