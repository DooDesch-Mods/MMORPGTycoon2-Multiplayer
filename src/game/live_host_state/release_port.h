#pragma once
#include "state.h"
namespace mt2mp::game::live_host_state {
struct ReleaseReceipt {bool attempted=false,confirmed=false,unknown=false;std::string cause;};
struct ReleasePort {
    virtual ~ReleasePort()=default;
    virtual bool bind(const Context&,const Binding&,uint64_t,std::string&)=0;
    virtual bool suspend(std::string&)=0;
    virtual bool capture(const Context&,const Binding&,release_cycle::Publication&,std::string&)=0;
    virtual bool apply(const Context&,const Binding&,const Frame&,ReleaseReceipt&,std::string&)=0;
    virtual nlohmann::json status()const=0;
};
}
