#pragma once
#include <string>

namespace mt2mp::game::interventions::actors {
// The caller retains fresh complete actor reads before and after this guard.
// With no actors or lifecycle work there is no native adoption to execute.
template<class Port,class Pin>class EmptyAdoption {
    Port& port_;Pin pin_{};bool active_=false;
public:
    explicit EmptyAdoption(Port& port):port_(port){}
    template<class State,class Locals,class Work>
    bool begin(const State& before,const State& target,const Locals& locals,const Work& work,std::string& error){
        active_=before.empty()&&target.empty()&&locals.empty()&&work.empty();
        if(!active_)return true;
        if(!port_.read(pin_,error))return false;
        if(pin_.selected!=0){error="Actor authoritative adoption requires actual Client Clock0";return false;}
        return port_.current_owners(pin_,error);
    }
    bool active()const{return active_;}
    bool finish(std::string& error){return !active_||port_.current_owners(pin_,error);}
};
}
