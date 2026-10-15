#pragma once
#include "read.h"
#include "initial_release/release_entry.h"
#include <map>
namespace mt2mp::game::developer_staff {
struct Request {
    Command command=Command::Hire;
    std::string token,origin_uuid;
    uint64_t epoch=0,generation=0;
    int32_t expected_count=0;
};
using NativeReceipt=initial_release::CallReceipt;
struct Receipt {
    std::string token,cause;
    Command command=Command::Hire;
    Capture before,after;
    NativeReceipt native;
    bool immediate_effect=false,unknown=false,replay_rejected=false;
};
struct ProbePort {
    virtual ~ProbePort()=default;
    virtual bool authorize(Capture&,uintptr_t& window,std::string&)=0;
    virtual bool call(Command,uintptr_t image,uintptr_t window,NativeReceipt&,std::string&)=0;
    virtual bool after(Capture&,std::string&)=0;
};
class Probe {
    std::map<std::string,Receipt> retained_;
    bool busy_=false,locked_=false;
    std::string cause_;
public:
    bool run(const Request&,ProbePort&,Receipt&,std::string&);
    const std::map<std::string,Receipt>& retained()const{return retained_;}
    bool locked()const{return locked_;}
    const std::string& cause()const{return cause_;}
};
nlohmann::json encode(const Receipt&);
class FixtureUpdateScope {
    bool outer_=false;
public:
    explicit FixtureUpdateScope(void*)noexcept;
    ~FixtureUpdateScope();
    FixtureUpdateScope(const FixtureUpdateScope&)=delete;
    FixtureUpdateScope& operator=(const FixtureUpdateScope&)=delete;
};
bool observe_fixture(nlohmann::json&,std::string&);
bool run_fixture(const nlohmann::json&,nlohmann::json&,std::string&);
bool authorize_fixture(Capture&,uintptr_t& owned_development_window,std::string&);
}
