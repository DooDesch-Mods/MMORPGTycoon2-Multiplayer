#pragma once
#include "visual.h"
#include <exception>
namespace mt2mp::game::toon_weapon_visual {
class RandomDiagnostic {
    Calls& calls_;
    RandomObservation& result_;
    bool read(std::array<uint64_t,4>& out,std::string& cause)noexcept{
        try{return calls_.random(out,cause);}
        catch(const std::exception& e){cause=std::string("Weapon RNG observation exception: ")+e.what();return false;}
        catch(...){cause="Weapon RNG observation nonstandard exception";return false;}
    }
public:
    RandomDiagnostic(Calls& calls,RandomObservation& result):calls_(calls),result_(result){
        result_={};result_.before_observed=read(result_.before,result_.before_cause);
        if(!result_.before_observed&&result_.before_cause.empty())result_.before_cause="Weapon RNG before reader failed without cause";
    }
    ~RandomDiagnostic(){
        result_.after_observed=read(result_.after,result_.after_cause);
        if(!result_.after_observed&&result_.after_cause.empty())result_.after_cause="Weapon RNG after reader failed without cause";
        result_.observed=result_.before_observed&&result_.after_observed;
        result_.unchanged=result_.observed&&result_.before==result_.after;
        if(!result_.observed)result_.cause="Weapon RNG observation unavailable: before="+result_.before_cause+"; after="+result_.after_cause;
        else if(!result_.unchanged)result_.cause="Global RNG changed during weapon reconcile";
    }
};
}
