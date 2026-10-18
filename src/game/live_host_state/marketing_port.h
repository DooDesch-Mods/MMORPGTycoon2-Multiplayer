#pragma once
#include "state.h"
namespace mt2mp::game::live_host_state {
struct MarketingReceipt {bool attempted=false,confirmed=false,unknown=false;std::string cause;};
struct MarketingPort {
    virtual ~MarketingPort()=default;
    virtual bool bind(const Context&,const Binding&,uint64_t,std::string& e){e.clear();return true;}
    virtual bool suspend(std::string& e){e.clear();return true;}
    virtual bool sent(const Frame&,std::string& e){e.clear();return true;}
    virtual bool acknowledge(uint64_t,uint64_t,std::string& e){e.clear();return true;}
    virtual bool admit(const Frame&,const Frame*,std::string& e){e.clear();return true;}
    virtual bool capture(const Context&,const Binding&,const Frame&,marketing_authority::State&,std::string&)=0;
    virtual bool apply(const Context&,const Binding&,const Frame&,MarketingReceipt&,std::string&)=0;
    virtual nlohmann::json status()const=0;
};
}
