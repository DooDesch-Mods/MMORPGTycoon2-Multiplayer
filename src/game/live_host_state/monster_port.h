#pragma once
#include "state.h"
namespace mt2mp::game::live_host_state {
struct MonsterPort {
    virtual ~MonsterPort()=default;
    virtual bool bind(const Context&,const Binding&,uint64_t,std::string&)=0;
    virtual bool suspend(std::string&)=0;
    virtual bool capture(const Context&,const Binding&,const Frame&,monsters::State&,std::string&)=0;
    virtual bool read_current(const Context&,const Binding&,uint64_t,monsters::State&,std::string& error){
        error="Monster provider has no bound current-read implementation";return false;
    }
    virtual bool preflight(const Context&,const Binding&,const Frame&,std::string&)=0;
    virtual bool apply(const Context&,const Binding&,const Frame&,monsters::Receipt&,std::string&)=0;
    virtual bool after_action(const Context&,const Binding&,std::string&)=0;
    virtual nlohmann::json status()const=0;
};
}
