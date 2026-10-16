#pragma once
#include "developer_staff/read.h"
#include <map>

namespace mt2mp::game::developer_staff::bug_policy {
enum class Scope {Online,Offline};
enum class Command {Show,Toggle};
enum class Input {Hover,Press,Release};
struct Request {
    std::string token,origin_uuid;
    uint64_t epoch=0,generation=0;
    Scope scope=Scope::Online;
    Command command=Command::Show;
    bool expected=false;
};
struct Snapshot {
    Capture capture;
    std::array<uint8_t,6> policies{};
    int32_t server_current=-1,server_target=-1;
};
struct Control {
    uintptr_t window=0,tabs=0,view=0,page=0,checkbox=0,value=0,field=0;
    bool operator==(const Control&)const=default;
};
struct Receipt {
    Request request;
    Snapshot before,after;
    uint32_t attempted_mask=0,returned_mask=0;
    bool confirmed=false,unknown=false,replay_rejected=false;
    std::string cause;
};
struct Port {
    virtual ~Port()=default;
    virtual bool read(Snapshot&,std::string&)=0;
    virtual bool window(const Snapshot&,Control&,std::string&,bool shown=false)=0;
    virtual bool control(const Snapshot&,Scope,Control&,bool pressed,std::string&)=0;
    virtual bool show(const Control&,std::string&)=0;
    virtual bool page(const Control&,std::string&)=0;
    virtual bool requested(const Control&,std::string&)=0;
    virtual bool input(const Control&,Input,std::string&)=0;
};
bool parse(const nlohmann::json&,Request&,std::string&);
bool ready_controller(const nlohmann::json&,bool live_active,std::string&);
size_t index(Scope);
const char* property(Scope);
bool same_owner(const Snapshot&,const Snapshot&);
class Fixture {
    std::map<std::string,Receipt> receipts_;
    bool busy_=false,locked_=false;
    std::string cause_;
public:
    bool run(const Request&,Port&,Receipt&,std::string&);
    const auto& receipts()const{return receipts_;}
    bool locked()const{return locked_;}
    const auto& cause()const{return cause_;}
};
nlohmann::json encode(const Snapshot&);
nlohmann::json encode(const Receipt&);
bool observe(nlohmann::json&,std::string&);
bool run(const nlohmann::json&,nlohmann::json&,std::string&);
}
